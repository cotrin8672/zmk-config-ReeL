#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <lvgl.h>

// Follow the stock nice_view widget's palette option. The pinned Zephyr mono
// adapter inverts I1 pixels when converting to ls0xx's MONO01 wire format.
#if defined(CONFIG_NICE_VIEW_WIDGET_INVERTED) && CONFIG_NICE_VIEW_WIDGET_INVERTED
#define REEL_BACKGROUND lv_color_black()
#define REEL_FOREGROUND lv_color_white()
#else
#define REEL_BACKGROUND lv_color_white()
#define REEL_FOREGROUND lv_color_black()
#endif

struct reel_screen_state {
    int battery; // -1 when no reading is available
    bool charging;
    bool connected;
    bool remote_valid;
    uint8_t layer;
};

lv_obj_t *reel_canvas_create(lv_obj_t *parent);
void reel_canvas_update(struct reel_screen_state state);
