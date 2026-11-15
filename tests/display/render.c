#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lvgl.h>
#include "reel_canvas.h"

static uint8_t display_buffer[160 * 68 / 8 + 8];
static uint8_t flushed[160 * 68 / 8];

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
    assert(area->x1 == 0 && area->x2 == 159 && area->y1 == 0 && area->y2 == 67);
    memcpy(flushed, pixels + 8, sizeof(flushed));
    lv_display_flush_ready(display);
}

static void write_portrait(const char *name) {
    FILE *file = fopen(name, "wb");
    assert(file);
    fprintf(file, "P5\n68 160\n255\n");
    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 68; x++) {
            int px = 159 - y;
            int py = x;
            int bit = (flushed[py * 20 + px / 8] >> (7 - px % 8)) & 1;
            fputc(bit ? 255 : 0, file);
        }
    }
    assert(fclose(file) == 0);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    lv_init();
    lv_display_t *display = lv_display_create(160, 68);
    assert(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(display, display_buffer, NULL, sizeof(display_buffer),
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    reel_canvas_create(screen);
    lv_screen_load(screen);

    const struct reel_screen_state states[] = {
        {.battery = 75, .connected = true, .remote_valid = true, .layer = 0, .profile = 0},
        {.battery = 50, .connected = true, .charging = true, .remote_valid = true, .layer = 1, .profile = 2},
        {.battery = 100, .connected = true, .remote_valid = true, .layer = 2, .profile = 4},
        {.battery = 3, .charging = true, .connected = true, .remote_valid = true, .layer = 3, .profile = 1},
        {.battery = 0, .remote_valid = false},
        {.battery = -1, .remote_valid = false},
    };
    for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
        char name[1024];
        reel_canvas_update(states[i]);
        lv_refr_now(display);
        snprintf(name, sizeof(name), "%s/state-%u.pgm", argv[1], (unsigned)i);
        write_portrait(name);
    }

    // Reconnection must restore actual state after the disconnected/unknown frames.
    reel_canvas_update(states[0]);
    lv_refr_now(display);
    char name[1024];
    snprintf(name, sizeof(name), "%s/reconnected.pgm", argv[1]);
    write_portrait(name);
    lv_display_delete(display);
    lv_deinit();
    return 0;
}
