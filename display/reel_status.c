#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include "reel_canvas.h"
#include "reel_display_state.h"

static const struct gpio_dt_spec charge_pin = GPIO_DT_SPEC_GET(DT_NODELABEL(charge_sense), gpios);

static struct reel_screen_state get_state(const zmk_event_t *event) {
    struct reel_display_state remote = reel_display_state_get();
    return (struct reel_screen_state){
        .battery = zmk_battery_state_of_charge(),
        .charging = gpio_pin_get_dt(&charge_pin) > 0,
        .connected = zmk_split_bt_peripheral_is_connected(),
        .remote_valid = remote.valid,
        .layer = remote.layer,
    };
}

// This macro snapshots state in the event context, then marshals all LVGL work
// onto ZMK's display queue. No LVGL access from Bluetooth or GPIO callbacks.
ZMK_DISPLAY_WIDGET_LISTENER(reel_status_widget, struct reel_screen_state, reel_canvas_update,
                            get_state)
ZMK_SUBSCRIPTION(reel_status_widget, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(reel_status_widget, zmk_split_peripheral_status_changed);
ZMK_SUBSCRIPTION(reel_status_widget, reel_display_state_changed);

static void charge_changed(struct input_event *event, void *user_data) {
    if (event->sync) {
        reel_status_widget_cb(NULL);
    }
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(reel_charge)), charge_changed, NULL);

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, REEL_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    reel_canvas_create(screen);
    reel_status_widget_init();
    return screen;
}
