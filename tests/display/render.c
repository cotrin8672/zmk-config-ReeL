#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lvgl.h>
#include <lvgl_display.h>
#include "reel_canvas.h"

static uint8_t display_buffer[160 * 68 / 8 + 8];
static uint8_t flushed[160 * 68 / 8];
static uint8_t conversion_buffer[sizeof(display_buffer)];
static bool partial_rows;

int display_write(const struct device *device, uint16_t x, uint16_t y,
                  const struct display_buffer_descriptor *desc, const void *pixels) {
    (void)device;
    assert(x == 0 && desc->width == 160 && desc->pitch == 160);
    assert(y + desc->height <= 68);
    assert(desc->buf_size == (size_t)desc->height * 20);
    memcpy(flushed + y * 20, pixels, desc->buf_size);
    partial_rows |= y > 0;
    return 0;
}
int display_blanking_on(const struct device *device) { (void)device; return 0; }
int display_blanking_off(const struct device *device) { (void)device; return 0; }

static void write_portrait(const char *name) {
    FILE *file = fopen(name, "wb");
    assert(file);
    fprintf(file, "P5\n68 160\n255\n");
    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 68; x++) {
            int px = 159 - y;
            int py = x;
            // Raw ls0xx wire bytes: least significant bit first, 1 means white.
            int bit = (flushed[py * 20 + px / 8] >> (px % 8)) & 1;
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
    static const struct device lcd;
    struct lvgl_disp_data data = {
        .display_dev = &lcd,
        .cap = {.x_resolution = 160, .y_resolution = 68,
                .current_pixel_format = PIXEL_FORMAT_MONO01,
                .screen_info = SCREEN_INFO_X_ALIGNMENT_WIDTH},
    };
    lv_display_set_user_data(display, &data);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(display, display_buffer, NULL, sizeof(display_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, lvgl_flush_cb_mono);
    lv_display_add_event_cb(display, lvgl_rounder_cb_mono, LV_EVENT_INVALIDATE_AREA, display);
    lvgl_set_mono_conversion_buffer(conversion_buffer, sizeof(conversion_buffer));
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, REEL_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    reel_canvas_create(screen);
    lv_screen_load(screen);

    const struct reel_screen_state states[] = {
        {.battery = 75, .connected = true, .remote_valid = true, .layer = 0},
        {.battery = 50, .connected = true, .charging = true, .remote_valid = true, .layer = 1},
        {.battery = 100, .connected = true, .remote_valid = true, .layer = 2},
        {.battery = 3, .charging = true, .connected = true, .remote_valid = true, .layer = 3},
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

    // Exercise a flush with a nonzero physical y, not only full-frame refreshes.
    lv_area_t area = {0, 17, 159, 19};
    uint8_t partial[sizeof(display_buffer)] = {0};
    for (size_t i = 8; i < 20 * 3 + 8; i++) {
        partial[i] = (uint8_t)(i * 37 + 3);
    }
    uint8_t expected[20 * 3];
    for (size_t i = 0; i < sizeof(expected); i++) {
        uint8_t value = partial[i + 8], reversed = 0;
        for (int bit = 0; bit < 8; bit++) {
            reversed |= ((value >> bit) & 1) << (7 - bit);
        }
        expected[i] = (uint8_t)~reversed;
    }
    lvgl_flush_cb_mono(display, &area, partial);
    assert(memcmp(flushed + 17 * 20, expected, sizeof(expected)) == 0);
    assert(partial_rows);
    lv_display_delete(display);
    lv_deinit();
    return 0;
}
