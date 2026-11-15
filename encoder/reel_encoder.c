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

static atomic_t link_seen, passive_mask, cw_count, ccw_count, rejected, ambiguous, dropped;
static unsigned link_run;
static struct reel_decoder decoder;
static uint32_t last_scan;
static bool scanned;

struct reel_encoder_stats reel_encoder_stats_get(void) {
    return (struct reel_encoder_stats){
        .link = atomic_get(&link_seen), .mask = atomic_get(&passive_mask),
        .cw = atomic_get(&cw_count), .ccw = atomic_get(&ccw_count),
        .rejected = atomic_get(&rejected), .ambiguous = atomic_get(&ambiguous),
        .dropped = atomic_get(&dropped),
    };
}

K_MSGQ_DEFINE(events, sizeof(int8_t), 64, 1);
static int levels(void) {
    gpio_port_value_t bits;
    if (gpio_port_get_raw(x.port, &bits)) {
        return -1;
    }
    return (!!(bits & BIT(x.pin)) << 1) | !!(bits & BIT(y.pin));
}

static void scan(struct k_timer *timer) {
    uint32_t now = k_cycle_get_32();
    int before = levels();
    int low_rc = gpio_pin_set_raw(y.port, y.pin, 0);
    k_busy_wait(5);
    int driven = levels();
    /* Always release, including after a failed read; high means disconnected
     * for S0D1. Internal pull-ups then restore open-contact voltage. */
    int release_rc = gpio_pin_set_raw(y.port, y.pin, 1);
    k_busy_wait(8);
    int after = levels();
    bool late = scanned && now - last_scan > k_us_to_cyc_ceil32(1000);
    if (before >= 0 && before == after) {
        atomic_or(&passive_mask, BIT(before));
    }
    int ab = (before < 0 || driven < 0 || after < 0 || (driven & 1) || low_rc || release_rc)
                 ? -1 : reel_sample(before, driven & 2, after);
    if (ab < 0) {
        atomic_inc(&rejected);
        link_run = 0;
        return; // discard a torn scan; compare the next complete scan
    }
    if (late) {
        atomic_inc(&rejected);
        decoder.ready = false;
        link_run = 0;
    }
    last_scan = now;
    scanned = true;
    if (before == 3 && !(driven & 2)) {
        if (link_run < 8 && ++link_run == 8) {
            atomic_set(&link_seen, 1);
        }
    } else {
        link_run = 0;
    }
    bool unclear = false;
    int8_t dir = reel_decode(&decoder, ab, now, k_us_to_cyc_ceil32(1000), &unclear);
    if (unclear) {
        atomic_inc(&ambiguous);
    }
    if (dir) {
        atomic_inc(dir > 0 ? &cw_count : &ccw_count);
        if (k_msgq_put(&events, &dir, K_NO_WAIT)) {
            atomic_inc(&dropped);
        }
    }
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
    int rc = gpio_pin_configure_dt(&x, GPIO_INPUT);
    return rc ? rc : gpio_pin_configure_dt(&y, GPIO_INPUT | GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN);
}
static const struct sensor_driver_api api = {
    .sample_fetch = fetch, .channel_get = get, .trigger_set = set_trigger,
};
SENSOR_DEVICE_DT_INST_DEFINE(0, init, NULL, NULL, NULL, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
                             &api);
