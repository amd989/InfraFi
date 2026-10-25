#pragma once

#include <gui/canvas.h>

/* Shared drawing helpers for InfraFi's custom views. */

/* Dotted horizontal rule (every other pixel). */
void wfr_draw_dotted_hline(Canvas* canvas, int32_t x, int32_t y, int32_t width);

/* App logo + bold "InfraFi" wordmark, baseline at y. */
void wfr_draw_wordmark(Canvas* canvas, int32_t x, int32_t y);

/* Bold centered title on line 9 with a dotted rule under it. */
void wfr_draw_title(Canvas* canvas, const char* title);

/* Small solid arrows used by the carousel. (x, y) is the tip. */
void wfr_draw_arrow_left(Canvas* canvas, int32_t x, int32_t y);
void wfr_draw_arrow_right(Canvas* canvas, int32_t x, int32_t y);

/* Black disc with a white ring and a white icon centred on it. */
void wfr_draw_badge(Canvas* canvas, int32_t cx, int32_t cy, const Icon* icon);

/* Truncate str with "..." so it fits in width px using the current font. */
void wfr_draw_str_fit(Canvas* canvas, int32_t x, int32_t y, const char* str, size_t width);
