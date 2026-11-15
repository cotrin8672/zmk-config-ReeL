/* SPDX-License-Identifier: MIT */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zmk/event_manager.h>

struct reel_display_state {
    bool valid;
    uint8_t layer;
};

struct reel_display_state_changed {
    struct reel_display_state state;
};

ZMK_EVENT_DECLARE(reel_display_state_changed);

/* Thread-safe snapshot. Ignore layer while valid is false. */
struct reel_display_state reel_display_state_get(void);
