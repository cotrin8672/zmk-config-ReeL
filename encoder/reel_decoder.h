/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Bits are pulled-up voltage levels: bit 1 = A, bit 0 = B. The three
 * samples are (X,Y) released, X with Y low, and (X,Y) released again. */
static inline int reel_sample(unsigned before, bool driven_x, unsigned after) {
    if (before != after || (!(before & 1) && driven_x != !!(before & 2))) {
        return -1;
    }
    return (driven_x ? 2 : 0) | (before & 1);
}

struct reel_decoder {
    bool ready, invalid;
    uint8_t prev, stable_a;
    int sum;
    uint32_t edge_at;
};

/* Feed ordered, verified scans. Debounce A, including cancelled excursions.
 * Only valid A crossings contribute: a return across the same contact cancels
 * the departure. B-only tail chatter never carries over into a reversal. */
static inline int reel_decode(struct reel_decoder *d, uint8_t ab, uint32_t now,
                              uint32_t settle, bool *ambiguous) {
    uint8_t a = ab >> 1;
    if (!d->ready) {
        *d = (struct reel_decoder){.ready = true, .prev = ab, .stable_a = a, .edge_at = now};
        return 0;
    }
    unsigned changed = ab ^ d->prev;
    d->prev = ab;
    if (changed & 2) {
        d->edge_at = now;
        if (changed == 3) {
            d->invalid = true;
        } else if (!d->invalid) {
            d->sum += a != (ab & 1) ? 1 : -1;
            if (d->sum > 3 || d->sum < -3) {
                d->invalid = true;
            }
        }
    }
    if (now - d->edge_at < settle) {
        return 0;
    }
    int dir = 0;
    if (a != d->stable_a) {
        if (!d->invalid && (d->sum == 1 || d->sum == -1)) {
            dir = d->sum;
        } else {
            *ambiguous = true;
        }
    }
    d->stable_a = a;
    d->sum = 0;
    d->invalid = false;
    return dir;
}
