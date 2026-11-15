// Host capture shim. Values and fields used by lvgl_display_mono.c match Zephyr 4.1.
// The actual pixel conversion is compiled from Zephyr, not reimplemented here.
#pragma once
#include <zephyr/kernel.h>

struct device { int unused; };
enum display_pixel_format {
    PIXEL_FORMAT_MONO01 = BIT(1),
    PIXEL_FORMAT_MONO10 = BIT(2),
};
enum display_screen_info {
    SCREEN_INFO_MONO_VTILED = BIT(0),
    SCREEN_INFO_MONO_MSB_FIRST = BIT(1),
    SCREEN_INFO_EPD = BIT(2),
    SCREEN_INFO_DOUBLE_BUFFER = BIT(3),
    SCREEN_INFO_X_ALIGNMENT_WIDTH = BIT(4),
};
struct display_capabilities {
    uint16_t x_resolution, y_resolution;
    enum display_pixel_format current_pixel_format;
    uint32_t screen_info;
};
struct display_buffer_descriptor {
    size_t buf_size;
    uint16_t width, height, pitch;
    bool frame_incomplete;
};
int display_write(const struct device *, uint16_t, uint16_t,
                  const struct display_buffer_descriptor *, const void *);
int display_blanking_on(const struct device *);
int display_blanking_off(const struct device *);
