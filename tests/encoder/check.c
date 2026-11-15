/* SPDX-License-Identifier: MIT */
#include "../../encoder/reel_decoder.h"
#include <assert.h>

static unsigned passive(bool a_high, bool b_high) {
    return 2 * (a_high || b_high) + b_high;
}

int main(void) {
    /* Physical X=C, Y=A, B grounded. Drive both independently on the
     * very same ideal motion. A wrong topology can alternate even when
     * nothing bounces and no samples were lost. */
    for (int direction = -1; direction <= 1; direction += 2) {
        struct reel_decoder seen[2] = {0};
        const unsigned path[] = {3, 1, 0, 2};
        unsigned count[2][2] = {0};
        for (unsigned edge = 0; edge <= 8; ++edge) {
            unsigned both = path[(direction > 0 ? edge : 8 - edge) % 4];
            bool a = both >> 1, b = both & 1, unclear = false;
            unsigned raw = (2U * b) + (a || b);
            unsigned yx = reel_swap(raw);
            int obs[] = {reel_sample(yx, a, yx), reel_sample(raw, a && b, raw)};
            for (unsigned m = 0; m < 2; ++m) {
                assert(obs[m] >= 0);
                for (unsigned n = 0; n < 6; ++n) {
                    int ev = reel_decode(&seen[m], obs[m], edge * 6 + n, 4, &unclear);
                    if (ev) { ++count[m][ev > 0]; }
                }
            }
        }
        assert(count[0][direction > 0] == 4 && count[0][direction < 0] == 0);
        assert(count[1][0] == 2 && count[1][1] == 2);
    }
    struct reel_dwell dwell = {0};
    assert(!reel_dwell_update(&dwell, 1, UINT32_MAX - 1, 4, 8));
    assert(reel_dwell_update(&dwell, 1, 1, 4, 8) == 3);
    assert(!reel_dwell_update(&dwell, -1, 2, 4, 8));
    assert(!reel_dwell_update(&dwell, 1, 3, 4, 8));
    assert(!reel_dwell_update(&dwell, 2, 4, 4, 8));
    assert(!reel_dwell_update(&dwell, 2, 10, 4, 8));
    assert(reel_dwell_update(&dwell, 2, 14, 4, 8) == 4);
    assert(reel_dwell_update(&dwell, 2, 17, 4, 8) == 7);
    assert(reel_dwell_update(&dwell, 2, 20, 4, 8) == 8);
    struct reel_decoder d = {0};
    uint8_t ring[] = {3, 1, 0, 2};
    uint32_t time = UINT32_MAX - 30;
    unsigned phase = 0;
    int emitted = 0;
    bool ambiguous = false;
    for (unsigned ab = 0; ab < 4; ++ab) {
        unsigned raw = passive(ab >> 1, ab & 1);
        assert(reel_sample(raw, ab >> 1, raw) == (int)ab);
    }
    assert(reel_sample(3, 0, 0) < 0);
    assert(reel_sample(0, 1, 0) < 0);
    for (int leg = 0; leg < 4; leg++) {
        int dir = leg % 2 ? -1 : 1;
        for (int click = 0; click < 18; click++) {
            int per_click = 0;
            for (int step = 0; step < 3; step++) {
                if (step) { phase = (phase + (dir > 0 ? 1 : 3)) % 4; }
                uint8_t ab = ring[phase];
                unsigned raw = passive(ab >> 1, ab & 1);
                for (int i = 0; i < 6; i++) {
                    int event = reel_decode(&d, reel_sample(raw, ab >> 1, raw), time++, 4,
                                            &ambiguous);
                    if (event) { assert(event == dir); per_click++; }
                }
            }
            assert(per_click == 1);
            emitted += per_click;
        }
    }
    assert(emitted == 72 && !ambiguous);
    /* Unobserved intermediate at reversal: never reuse the prior direction. */
    for (int i = 0; i < 8; ++i) {
        assert(!reel_decode(&d, 0, time++, 4, &ambiguous));
    }
    assert(ambiguous);
    /* A bouncing while B is stable produces one event, B chatter alone none. */
    d = (struct reel_decoder){0};
    uint8_t bounce[] = {3, 2, 3, 1, 3, 1, 1, 1, 1, 1, 0, 1, 0};
    int total = 0;
    ambiguous = false;
    for (unsigned i = 0; i < sizeof(bounce); ++i) {
        int event = reel_decode(&d, bounce[i], time++, 4, &ambiguous);
        assert(event >= 0);
        total += event;
    }
    assert(total == 1 && !ambiguous);
}
