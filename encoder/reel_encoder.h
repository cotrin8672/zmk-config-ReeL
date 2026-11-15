/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
struct reel_encoder_stats {
    uint32_t mask, pos[3], neg[3], dwell01, dwell10, dropped;
};
struct reel_encoder_stats reel_encoder_stats_get(void);
