#define DT_DRV_COMPAT reel_smart_aml
#include <drivers/input_processor.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include "aml_motion.h"

enum { NUMBERS = 1, NAVIGATION = 2, MOUSE = 3, TIMEOUT_MS = 5000 };
static K_MUTEX_DEFINE(lock);
static struct aml_motion motion;
static int frame_x, frame_y;
static int64_t deadline;

// Only this processor owns MOUSE in the ReeL keymap.
static void release(void) {
    if (deadline) {
        deadline = 0;
        zmk_keymap_layer_deactivate(MOUSE, false);
    }
}

static void expire(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(timeout, expire);

static void expire(struct k_work *work) {
    k_mutex_lock(&lock, K_FOREVER);
    if (deadline && k_uptime_get() >= deadline) {
        release();
    }
    k_mutex_unlock(&lock);
}

static int handle_event(const struct device *dev, struct input_event *ev, uint32_t p1,
                        uint32_t p2, struct zmk_input_processor_state *state) {
    k_mutex_lock(&lock, K_FOREVER);
    if (zmk_keymap_layer_active(NUMBERS) || zmk_keymap_layer_active(NAVIGATION)) {
        motion = (struct aml_motion){0};
        frame_x = frame_y = 0;
    } else {
        if (ev->type == INPUT_EV_REL && ev->code == INPUT_REL_X) {
            frame_x += ev->value;
        } else if (ev->type == INPUT_EV_REL && ev->code == INPUT_REL_Y) {
            frame_y += ev->value;
        }
        if (ev->sync) {
            int64_t now = k_uptime_get();
            if (aml_moved(&motion, frame_x, frame_y, now)) {
                bool enter = !deadline;
                deadline = now + TIMEOUT_MS;
                if (enter) {
                    zmk_keymap_layer_activate(MOUSE, false);
                }
                k_work_reschedule(&timeout, K_MSEC(TIMEOUT_MS));
            }
            frame_x = frame_y = 0;
        }
    }
    k_mutex_unlock(&lock);
    return ZMK_INPUT_PROC_CONTINUE;
}

static int on_key_or_layer(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *layer = as_zmk_layer_state_changed(eh);
    const struct zmk_keycode_state_changed *key = as_zmk_keycode_state_changed(eh);
    // Mouse clicks and wheel events are separate from resolved keyboard keys.
    if ((key && key->state) ||
        (layer && layer->state && (layer->layer == NUMBERS || layer->layer == NAVIGATION))) {
        k_mutex_lock(&lock, K_FOREVER);
        motion = (struct aml_motion){0};
        frame_x = frame_y = 0;
        release();
        k_mutex_unlock(&lock);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(reel_aml_events, on_key_or_layer);
ZMK_SUBSCRIPTION(reel_aml_events, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(reel_aml_events, zmk_layer_state_changed);

static const struct zmk_input_processor_driver_api api = {.handle_event = handle_event};
DEVICE_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                      &api);
