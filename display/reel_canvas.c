// ReeL appearance, composed with LVGL drawing primitives and image/font assets.
// The upstream nice_view shield and Zephyr ls0xx driver retain LCD/SPI/VCOM ownership.
#include <stdio.h>
#include "reel_canvas.h"
#include "reel_assets.h"

enum { WIDTH = 68, HEIGHT = 160 };
#define FORMAT LV_COLOR_FORMAT_L8
#define BUFFER_SIZE LV_CANVAS_BUF_SIZE(WIDTH, HEIGHT, 8, LV_DRAW_BUF_STRIDE_ALIGN)
static LV_ATTRIBUTE_MEM_ALIGN uint8_t portrait_buffer[BUFFER_SIZE];
static LV_ATTRIBUTE_MEM_ALIGN uint8_t physical_buffer[BUFFER_SIZE];
static lv_obj_t *portrait;
static lv_obj_t *physical;
static struct reel_screen_state previous;
static bool rendered;

static void rectangle(lv_layer_t *layer, int x, int y, int width, int height, bool fill,
                      lv_color_t ink) {
    if (width <= 0 || height <= 0) {
        return;
    }
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = ink;
    dsc.bg_opa = fill ? LV_OPA_COVER : LV_OPA_TRANSP;
    dsc.border_color = ink;
    dsc.border_width = fill ? 0 : 1;
    dsc.radius = 0;
    const lv_area_t area = {x, y, x + width - 1, y + height - 1};
    lv_draw_rect(layer, &dsc, &area);
}

static void image(lv_layer_t *layer, const lv_image_dsc_t *source, int x, int y) {
    lv_draw_image_dsc_t dsc;
    lv_draw_image_dsc_init(&dsc);
    dsc.src = source;
    dsc.recolor = REEL_FOREGROUND;
    dsc.recolor_opa = LV_OPA_COVER;
    const lv_area_t area = {x, y, x + source->header.w - 1, y + source->header.h - 1};
    lv_draw_image(layer, &dsc, &area);
}

static void charging_mark(lv_layer_t *layer, int fill) {
    // Five tiny scanline runs preserve the old inverted bolt inside a partially
    // filled battery. LVGL still owns rasterization and clipping.
    static const uint8_t mark[] = {0x04, 0x0c, 0x1f, 0x06, 0x0c};
    for (int row = 0; row < 5; row++) {
        for (int column = 0; column < 5; column++) {
            if (mark[row] & (1u << (4 - column))) {
                int x = 8 + column;
                rectangle(layer, x, 6 + row, 1, 1, true,
                          x < 4 + fill ? REEL_BACKGROUND : REEL_FOREGROUND);
            }
        }
    }
}

static bool unchanged(struct reel_screen_state s) {
    return rendered && s.battery == previous.battery && s.charging == previous.charging &&
           s.connected == previous.connected && s.remote_valid == previous.remote_valid &&
           s.layer == previous.layer;
}

void reel_canvas_update(struct reel_screen_state state) {
    if (unchanged(state)) {
        return;
    }
    lv_canvas_fill_bg(portrait, REEL_BACKGROUND, LV_OPA_COVER);
    lv_layer_t layer;
    lv_canvas_init_layer(portrait, &layer);
    image(&layer, &reel_status_base, 0, 0);

    int level = state.battery < 0 ? -1 : LV_MIN(state.battery, 100);
    int fill = level < 0 ? 0 : (13 * level + 50) / 100;
    rectangle(&layer, 4, 6, fill, 5, true, REEL_FOREGROUND);
    if (state.charging) {
        charging_mark(&layer, fill);
    }

    char percentage[5];
    if (level < 0) {
        snprintf(percentage, sizeof(percentage), "--%%");
    } else {
        snprintf(percentage, sizeof(percentage), "%d%%", level);
    }
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.font = &reel_status_font;
    label.color = REEL_FOREGROUND;
    label.text = percentage;
    const lv_area_t text_area = {24, 4, 50, 13};
    lv_draw_label(&layer, &label, &text_area);

    rectangle(&layer, 52, 9, 4, 4, state.connected, REEL_FOREGROUND);
    rectangle(&layer, 57, 6, 4, 7, state.connected, REEL_FOREGROUND);
    rectangle(&layer, 62, 3, 4, 10, state.connected, REEL_FOREGROUND);
    if (state.remote_valid) {
        image(&layer, &reel_layer_pointer, 4, 65 + LV_MIN(state.layer, 3) * 13);
    }
    lv_canvas_finish_layer(portrait, &layer);

    // Same software rotation API as nice_view's upstream status widget. In LVGL,
    // ROTATION_270 maps logical (x,y) to physical (159-y,x), matching the RMK LCD.
    lv_draw_sw_rotate(portrait_buffer, physical_buffer, WIDTH, HEIGHT,
                      lv_draw_buf_width_to_stride(WIDTH, FORMAT),
                      lv_draw_buf_width_to_stride(HEIGHT, FORMAT), LV_DISPLAY_ROTATION_270, FORMAT);
    lv_obj_invalidate(physical);
    previous = state;
    rendered = true;
}

lv_obj_t *reel_canvas_create(lv_obj_t *parent) {
    physical = lv_canvas_create(parent);
    lv_canvas_set_buffer(physical, physical_buffer, HEIGHT, WIDTH, FORMAT);
    lv_obj_set_pos(physical, 0, 0);
    lv_obj_set_style_image_opa(physical, LV_OPA_COVER, 0);

    portrait = lv_canvas_create(parent);
    lv_canvas_set_buffer(portrait, portrait_buffer, WIDTH, HEIGHT, FORMAT);
    lv_obj_add_flag(portrait, LV_OBJ_FLAG_HIDDEN);
    rendered = false;
    return physical;
}
