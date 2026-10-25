#include "wfr_list_view.h"
#include "wfr_draw.h"
#include <gui/canvas.h>
#include <furi.h>
#include "infrafi_icons.h"

#define ROWS_VISIBLE 4
#define ROW_H        12
#define LIST_Y       15

struct WfrListView {
    View* view;
    WfrListViewCallback callback;
    void* context;
};

typedef struct {
    const char* title;
    const char* names[WFR_LIST_VIEW_MAX_ITEMS];
    bool locked[WFR_LIST_VIEW_MAX_ITEMS];
    uint8_t count;
    uint8_t selected;
    uint8_t offset;
} WfrListViewModel;

static void wfr_list_view_draw(Canvas* canvas, void* _model) {
    WfrListViewModel* model = _model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, model->title ? model->title : "");
    wfr_draw_dotted_hline(canvas, 0, 12, 128);

    if(model->count == 0) {
        canvas_draw_icon(canvas, 54, 18, &I_menu_saved_20x20);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignBottom, "No saved networks yet");
        canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignBottom, "Sent networks show up here");
        return;
    }

    char counter[8];
    snprintf(counter, sizeof(counter), "%u/%u", model->selected + 1, model->count);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, counter);

    for(uint8_t row = 0; row < ROWS_VISIBLE; row++) {
        uint8_t i = model->offset + row;
        if(i >= model->count) break;
        int32_t y = LIST_Y + row * ROW_H;
        bool selected = (i == model->selected);

        if(selected) {
            canvas_draw_rbox(canvas, 0, y, 123, ROW_H, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_icon(canvas, 3, y + 4, &I_wifi_small_9x5);
        wfr_draw_str_fit(canvas, 15, y + 9, model->names[i], model->locked[i] ? 90 : 104);
        if(model->locked[i]) {
            canvas_draw_icon(canvas, 111, y + 1, &I_lock_9x10);
        }
        canvas_set_color(canvas, ColorBlack);
    }

    /* Scrollbar: dotted track with a solid thumb */
    if(model->count > ROWS_VISIBLE) {
        for(int32_t y = 14; y < 64; y += 2) {
            canvas_draw_dot(canvas, 126, y);
        }
        int32_t track = 64 - 14;
        int32_t thumb = track * ROWS_VISIBLE / model->count;
        if(thumb < 4) thumb = 4;
        int32_t ty = 14 + (track - thumb) * model->selected / (model->count - 1);
        canvas_draw_box(canvas, 125, ty, 3, thumb);
    }
}

static bool wfr_list_view_input(InputEvent* event, void* context) {
    WfrListView* list = context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        int32_t step = 0;
        if(event->key == InputKeyUp) step = -1;
        if(event->key == InputKeyDown) step = 1;
        if(step != 0) {
            with_view_model(
                list->view,
                WfrListViewModel * model,
                {
                    if(model->count) {
                        model->selected = (model->selected + model->count + step) % model->count;
                        if(model->selected < model->offset) {
                            model->offset = model->selected;
                        } else if(model->selected >= model->offset + ROWS_VISIBLE) {
                            model->offset = model->selected - ROWS_VISIBLE + 1;
                        }
                    }
                },
                true);
            consumed = true;
        }
    }

    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint8_t count = 0;
        uint32_t index = 0;
        with_view_model(
            list->view,
            WfrListViewModel * model,
            {
                count = model->count;
                index = model->selected;
            },
            false);
        if(count && list->callback) list->callback(list->context, index);
        consumed = true;
    }

    return consumed;
}

WfrListView* wfr_list_view_alloc(void) {
    WfrListView* list = malloc(sizeof(WfrListView));
    list->callback = NULL;
    list->context = NULL;
    list->view = view_alloc();
    view_allocate_model(list->view, ViewModelTypeLocking, sizeof(WfrListViewModel));
    view_set_context(list->view, list);
    view_set_draw_callback(list->view, wfr_list_view_draw);
    view_set_input_callback(list->view, wfr_list_view_input);
    wfr_list_view_reset(list, NULL);
    return list;
}

void wfr_list_view_free(WfrListView* list) {
    view_free(list->view);
    free(list);
}

View* wfr_list_view_get_view(WfrListView* list) {
    return list->view;
}

void wfr_list_view_reset(WfrListView* list, const char* title) {
    with_view_model(
        list->view,
        WfrListViewModel * model,
        {
            model->title = title;
            model->count = 0;
            model->selected = 0;
            model->offset = 0;
        },
        true);
}

void wfr_list_view_add_item(WfrListView* list, const char* name, bool locked) {
    with_view_model(
        list->view,
        WfrListViewModel * model,
        {
            if(model->count < WFR_LIST_VIEW_MAX_ITEMS) {
                model->names[model->count] = name;
                model->locked[model->count] = locked;
                model->count++;
            }
        },
        true);
}

void wfr_list_view_set_selected(WfrListView* list, uint32_t index) {
    with_view_model(
        list->view,
        WfrListViewModel * model,
        {
            if(index < model->count) {
                model->selected = index;
                if(model->selected >= ROWS_VISIBLE) {
                    model->offset = model->selected - ROWS_VISIBLE + 1;
                } else {
                    model->offset = 0;
                }
            }
        },
        true);
}

void wfr_list_view_set_callback(WfrListView* list, WfrListViewCallback callback, void* context) {
    list->callback = callback;
    list->context = context;
}
