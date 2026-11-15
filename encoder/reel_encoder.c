/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT reel_active_encoder
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include "reel_decoder.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(reel_encoder, CONFIG_SENSOR_LOG_LEVEL);

/* One local encoder. No GPIO callback is armed on our own probe pulses. */
static const struct gpio_dt_spec x = GPIO_DT_SPEC_INST_GET(0, sense_gpios);
static const struct gpio_dt_spec y = GPIO_DT_SPEC_INST_GET(0, probe_gpios);
BUILD_ASSERT(DT_SAME_NODE(DT_GPIO_CTLR(DT_DRV_INST(0), sense_gpios),
                          DT_GPIO_CTLR(DT_DRV_INST(0), probe_gpios)));
BUILD_ASSERT(DT_INST_GPIO_PIN(0, sense_gpios) != DT_INST_GPIO_PIN(0, probe_gpios));

static atomic_t dropped;
static struct reel_decoder decoder;
static uint32_t last_scan;
static bool scanned;

K_MSGQ_DEFINE(events, sizeof(int8_t), 64, 1);
static int levels(void) {
    gpio_port_value_t bits;
    if (gpio_port_get_raw(x.port, &bits)) {
        return -1;
    }
    return (!!(bits & BIT(x.pin)) << 1) | !!(bits & BIT(y.pin));
}

/* probe-gpios is the physical common C (P1.14); sense-gpios is A
 * (P1.15). The PCB grounds physical B. A released C reads B-C; with C low,
 * A reads A-C independently. Never drive C high: B-C may short it to GND. */
static void scan(struct k_timer *timer) {
    uint32_t now = k_cycle_get_32();
    int before = levels();
    int low_rc = gpio_pin_set_raw(y.port, y.pin, 0);
    k_busy_wait(5);
    int driven = levels();
    int release_rc = gpio_pin_set_raw(y.port, y.pin, 1);
    k_busy_wait(8);
    int after = levels();
    if (before < 0 || driven < 0 || after < 0 || low_rc || release_rc || (driven & 1)) {
        return;
    }
    int ab = reel_sample(before, driven & 2, after);
    if (ab < 0) {
        return;
    }
    if (scanned && now - last_scan > k_us_to_cyc_ceil32(1000)) {
        decoder.ready = false;
    }
    scanned = true;
    last_scan = now;
    bool unclear = false;
    int8_t dir = reel_decode(&decoder, ab, now, k_us_to_cyc_ceil32(1000), &unclear);
    if (dir && k_msgq_put(&events, &dir, K_NO_WAIT)) {
        atomic_inc(&dropped);
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
        atomic_val_t lost = atomic_set(&dropped, 0);
        if (lost) {
            LOG_WRN("Encoder event queue dropped %ld clicks", (long)lost);
        }
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
