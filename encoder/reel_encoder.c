/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT reel_active_encoder
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include "reel_decoder.h"
#include "reel_encoder.h"

/* One local encoder. No GPIO callback is armed on our own probe pulses. */
static const struct gpio_dt_spec x = GPIO_DT_SPEC_INST_GET(0, sense_gpios);
static const struct gpio_dt_spec y = GPIO_DT_SPEC_INST_GET(0, probe_gpios);
BUILD_ASSERT(DT_SAME_NODE(DT_GPIO_CTLR(DT_DRV_INST(0), sense_gpios),
                          DT_GPIO_CTLR(DT_DRV_INST(0), probe_gpios)));
BUILD_ASSERT(DT_INST_GPIO_PIN(0, sense_gpios) != DT_INST_GPIO_PIN(0, probe_gpios));

enum { PROBE_X, PROBE_Y, PASSIVE, MODES };
static atomic_t passive_mask, positive[MODES], negative[MODES], peaks[2], dropped;
static struct reel_decoder decoder[MODES];
static struct reel_dwell dwell;
static uint32_t last_scan[MODES];
static bool scanned[MODES];

struct reel_encoder_stats reel_encoder_stats_get(void) {
    struct reel_encoder_stats stats = {
        .mask = atomic_get(&passive_mask), .dropped = atomic_get(&dropped),
        .dwell01 = k_cyc_to_us_floor32(atomic_get(&peaks[0])) / 100,
        .dwell10 = k_cyc_to_us_floor32(atomic_get(&peaks[1])) / 100,
    };
    for (unsigned m = 0; m < MODES; ++m) {
        stats.pos[m] = atomic_get(&positive[m]);
        stats.neg[m] = atomic_get(&negative[m]);
    }
    return stats;
}

K_MSGQ_DEFINE(events, sizeof(int8_t), 64, 1);
static int levels(void) {
    gpio_port_value_t bits;
    if (gpio_port_get_raw(x.port, &bits)) {
        return -1;
    }
    return (!!(bits & BIT(x.pin)) << 1) | !!(bits & BIT(y.pin));
}

/* Both GPIOs are open-drain, input buffers connected. The other pin is
 * already released whenever one pin is temporarily driven low. */
static int probe(const struct gpio_dt_spec *pin) {
    int rc = gpio_pin_set_raw(pin->port, pin->pin, 0);
    k_busy_wait(5);
    int driven = levels();
    int release = gpio_pin_set_raw(pin->port, pin->pin, 1);
    k_busy_wait(8);
    return rc || release ? -1 : driven;
}

static void account(unsigned mode, int ab, uint32_t now) {
    if (ab < 0) {
        return;
    }
    if (scanned[mode] && now - last_scan[mode] > k_us_to_cyc_ceil32(1000)) {
        decoder[mode].ready = false;
    }
    scanned[mode] = true;
    last_scan[mode] = now;
    bool unclear = false;
    int8_t dir = reel_decode(&decoder[mode], ab, now, k_us_to_cyc_ceil32(1000), &unclear);
    if (dir) {
        atomic_inc(dir > 0 ? &positive[mode] : &negative[mode]);
        if (mode == PROBE_X && k_msgq_put(&events, &dir, K_NO_WAIT)) {
            atomic_inc(&dropped);
        }
    }
}

static void scan(struct k_timer *timer) {
    uint32_t now = k_cycle_get_32();
    int before = levels();
    int y_low = probe(&y);
    int mid = levels();
    int x_low = probe(&x);
    int after = levels();
    int raw = before >= 0 && before == mid && mid == after ? before : -1;
    uint32_t run = reel_dwell_update(&dwell, raw, now, k_us_to_cyc_ceil32(1000),
                                     k_ms_to_cyc_ceil32(999));
    if (raw < 0) {
        return;
    }
    atomic_or(&passive_mask, BIT(raw));
    if (raw == 1 || raw == 2) {
        atomic_t *peak = &peaks[raw - 1];
        if (run > (uint32_t)atomic_get(peak)) {
            atomic_set(peak, run);
        }
    }
    if (x_low >= 0 && !(x_low & 2)) {
        unsigned yx = reel_swap(raw);
        account(PROBE_X, reel_sample(yx, x_low & 1, yx), now);
    }
    if (y_low >= 0 && !(y_low & 1)) {
        account(PROBE_Y, reel_sample(raw, y_low & 2, raw), now);
    }
    account(PASSIVE, raw, now);
}
K_TIMER_DEFINE(scan_timer, scan, NULL);

/* ZMK fetches/gets synchronously when its data-ready callback runs outside
 * an ISR. Deliver each click separately so quick reversals cannot cancel. */
static const struct sensor_trigger *trigger;
static sensor_trigger_handler_t handler;
static int8_t current;
static void deliver(void *arg, void *unused1, void *unused2) {
    for (;;) {
        k_msgq_get(&events, &current, K_FOREVER);
        handler(arg, trigger);
    }
}
static struct k_thread thread;
K_THREAD_STACK_DEFINE(stack, 2048);

static int set_trigger(const struct device *dev, const struct sensor_trigger *trig,
                       sensor_trigger_handler_t cb) {
    if (!cb || trig->type != SENSOR_TRIG_DATA_READY || trig->chan != SENSOR_CHAN_ROTATION) {
        return -ENOTSUP;
    }
    if (handler) {
        return -EBUSY;
    }
    trigger = trig;
    handler = cb;
    k_thread_create(&thread, stack, K_THREAD_STACK_SIZEOF(stack), deliver, (void *)dev,
                    NULL, NULL, K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
    k_timer_start(&scan_timer, K_MSEC(2), K_USEC(250));
    return 0;
}
static int fetch(const struct device *dev, enum sensor_channel chan) {
    return chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_ROTATION ? 0 : -ENOTSUP;
}
static int get(const struct device *dev, enum sensor_channel chan, struct sensor_value *v) {
    if (chan != SENSOR_CHAN_ROTATION) {
        return -ENOTSUP;
    }
    *v = (struct sensor_value){.val1 = 20 * current};
    return 0;
}
static int init(const struct device *dev) {
    if (!gpio_is_ready_dt(&x) || !gpio_is_ready_dt(&y)) {
        return -ENODEV;
    }
    int rc = gpio_pin_configure_dt(&x, GPIO_INPUT | GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN);
    return rc ? rc : gpio_pin_configure_dt(&y, GPIO_INPUT | GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN);
}
static const struct sensor_driver_api api = {
    .sample_fetch = fetch, .channel_get = get, .trigger_set = set_trigger,
};
SENSOR_DEVICE_DT_INST_DEFINE(0, init, NULL, NULL, NULL, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
                             &api);
