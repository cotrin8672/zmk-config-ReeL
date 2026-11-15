/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
struct reel_encoder_stats {
    bool link;
    uint32_t mask, cw, ccw, rejected, ambiguous, dropped;
};
struct reel_encoder_stats reel_encoder_stats_get(void);
