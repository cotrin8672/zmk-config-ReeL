#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct aml_motion {
    int x, y;
    int64_t last;
};

// Match RMK smart_aml_trigger.rs: net displacement, with at most 100 ms
// between nonzero reports. Opposite deltas cancel instead of counting as travel.
static inline bool aml_moved(struct aml_motion *s, int x, int y, int64_t now) {
    if (!x && !y) {
        return false;
    }
    if (now - s->last > 100) {
        s->x = s->y = 0;
    }
    s->last = now;
    // Inputs are signed sensor deltas; residuals stay below 12.
    s->x += x;
    s->y += y;
    if (abs(x) >= 12 || abs(y) >= 12 || abs(s->x) + abs(s->y) >= 12) {
        s->x = s->y = 0;
        return true;
    }
    return false;
}
