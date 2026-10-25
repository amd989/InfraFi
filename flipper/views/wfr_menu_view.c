#include "wfr_menu_view.h"
#include "wfr_draw.h"
#include <gui/canvas.h>
#include <gui/icon.h>
#include <furi.h>

struct WfrMenuView {
    View* view;
    WfrMenuViewCallback callback;
    void* context;
};

typedef struct {
    const char* labels[WFR_MENU_VIEW_MAX_ITEMS];
    const Icon* icons[WFR_MENU_VIEW_MAX_ITEMS];
    uint8_t count;
    uint8_t selected;
} WfrMenuViewModel;

#define TILE_SIZE 32
#define TILE_X    ((128 - TILE_SIZE) / 2)
#define TILE_Y    14

static void wfr_menu_view_draw(Canvas* canvas, void* _model) {
    WfrMenuViewModel* model = _model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* Header: logo + wordmark, page dots on the right */
    wfr_draw_wordmark(canvas, 2, 9);
    for(uint8_t i = 0; i < model->count; i++) {
        int32_t x = 124 - (model->count - 1 - i) * 6;
        if(i == model->selected) {
            canvas_draw_disc(canvas, x, 5, 2);
        } else {
            canvas_draw_circle(canvas, x, 5, 1);
        }
    }
    wfr_draw_dotted_hline(canvas, 0, 12, 128);

    if(model->count == 0) return;

    /* Icon tile with a drop shadow */
    canvas_draw_rbox(canvas, TILE_X + 2, TILE_Y + 2, TILE_SIZE, TILE_SIZE, 5);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, TILE_X, TILE_Y, TILE_SIZE, TILE_SIZE, 5);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, TILE_X, TILE_Y, TILE_SIZE, TILE_SIZE, 5);

    const Icon* icon = model->icons[model->selected];
    canvas_draw_icon(
        canvas,
        TILE_X + (TILE_SIZE - icon_get_width(icon)) / 2,
        TILE_Y + (TILE_SIZE - icon_get_height(icon)) / 2,
        icon);

    /* Arrows hugging the tile */
    wfr_draw_arrow_left(canvas, TILE_X - 10, TILE_Y + TILE_SIZE / 2);
    wfr_draw_arrow_right(canvas, TILE_X + TILE_SIZE + 11, TILE_Y + TILE_SIZE / 2);

    /* Label pill */
    const char* label = model->labels[model->selected];
    canvas_set_font(canvas, FontPrimary);
    int32_t tw = canvas_string_width(canvas, label);
    canvas_draw_rbox(canvas, (128 - tw) / 2 - 6, 50, tw + 12, 13, 3);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, (128 - tw) / 2, 60, label);
    canvas_set_color(canvas, ColorBlack);
}

static bool wfr_menu_view_input(InputEvent* event, void* context) {
    WfrMenuView* menu = context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        int32_t step = 0;
        if(event->key == InputKeyLeft || event->key == InputKeyUp) step = -1;
        if(event->key == InputKeyRight || event->key == InputKeyDown) step = 1;
        if(step != 0) {
            with_view_model(
                menu->view,
                WfrMenuViewModel * model,
                {
                    if(model->count) {
                        model->selected = (model->selected + model->count + step) % model->count;
                    }
                },
                true);
            consumed = true;
        }
    }

    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint32_t index = 0;
        with_view_model(
            menu->view, WfrMenuViewModel * model, { index = model->selected; }, false);
        if(menu->callback) menu->callback(menu->context, index);
        consumed = true;
    }

    return consumed;
}

WfrMenuView* wfr_menu_view_alloc(void) {
    WfrMenuView* menu = malloc(sizeof(WfrMenuView));
    menu->callback = NULL;
    menu->context = NULL;
    menu->view = view_alloc();
    view_allocate_model(menu->view, ViewModelTypeLocking, sizeof(WfrMenuViewModel));
    view_set_context(menu->view, menu);
    view_set_draw_callback(menu->view, wfr_menu_view_draw);
    view_set_input_callback(menu->view, wfr_menu_view_input);
    wfr_menu_view_reset(menu);
    return menu;
}

void wfr_menu_view_free(WfrMenuView* menu) {
    view_free(menu->view);
    free(menu);
}

View* wfr_menu_view_get_view(WfrMenuView* menu) {
    return menu->view;
}

void wfr_menu_view_reset(WfrMenuView* menu) {
    with_view_model(
        menu->view,
        WfrMenuViewModel * model,
        {
            model->count = 0;
            model->selected = 0;
        },
        true);
}

void wfr_menu_view_add_item(WfrMenuView* menu, const char* label, const Icon* icon) {
    with_view_model(
        menu->view,
        WfrMenuViewModel * model,
        {
            if(model->count < WFR_MENU_VIEW_MAX_ITEMS) {
                model->labels[model->count] = label;
                model->icons[model->count] = icon;
                model->count++;
            }
        },
        true);
}

void wfr_menu_view_set_selected(WfrMenuView* menu, uint32_t index) {
    with_view_model(
        menu->view,
        WfrMenuViewModel * model,
        {
            if(index < model->count) model->selected = index;
        },
        true);
}

void wfr_menu_view_set_callback(WfrMenuView* menu, WfrMenuViewCallback callback, void* context) {
    menu->callback = callback;
    menu->context = context;
}
