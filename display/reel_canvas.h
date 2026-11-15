#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <lvgl.h>

struct reel_screen_state {
    int battery; // -1 when no reading is available
    bool charging;
    bool connected;
    bool remote_valid;
    uint8_t layer;
    uint8_t profile;
};

lv_obj_t *reel_canvas_create(lv_obj_t *parent);
void reel_canvas_update(struct reel_screen_state state);
