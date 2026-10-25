#include "wfr_draw.h"
#include <gui/elements.h>
#include <gui/icon.h>
#include <furi.h>
#include "infrafi_icons.h"

void wfr_draw_dotted_hline(Canvas* canvas, int32_t x, int32_t y, int32_t width) {
    for(int32_t i = 0; i < width; i += 2) {
        canvas_draw_dot(canvas, x + i, y);
    }
}

void wfr_draw_wordmark(Canvas* canvas, int32_t x, int32_t y) {
    canvas_draw_icon(canvas, x, y - 9, &I_infrafi_10px);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, x + 13, y, "InfraFi");
}

void wfr_draw_title(Canvas* canvas, const char* title) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, title);
}

void wfr_draw_arrow_left(Canvas* canvas, int32_t x, int32_t y) {
    for(int32_t k = 0; k < 4; k++) {
        canvas_draw_line(canvas, x + k, y - k, x + k, y + k);
    }
}

void wfr_draw_arrow_right(Canvas* canvas, int32_t x, int32_t y) {
    for(int32_t k = 0; k < 4; k++) {
        canvas_draw_line(canvas, x - k, y - k, x - k, y + k);
    }
}

void wfr_draw_badge(Canvas* canvas, int32_t cx, int32_t cy, const Icon* icon) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_disc(canvas, cx, cy, 11);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_disc(canvas, cx, cy, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_icon(
        canvas, cx - icon_get_width(icon) / 2, cy - icon_get_height(icon) / 2, icon);
    canvas_set_color(canvas, ColorBlack);
}

void wfr_draw_str_fit(Canvas* canvas, int32_t x, int32_t y, const char* str, size_t width) {
    FuriString* s = furi_string_alloc_set(str);
    elements_string_fit_width(canvas, s, width);
    canvas_draw_str(canvas, x, y, furi_string_get_cstr(s));
    furi_string_free(s);
}
