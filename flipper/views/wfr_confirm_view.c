#include "wfr_confirm_view.h"
#include "wfr_draw.h"
#include "../protocol/wfr_protocol.h"
#include <gui/canvas.h>
#include <gui/elements.h>
#include <furi.h>
#include "infrafi_icons.h"

struct WfrConfirmView {
    View* view;
    WfrConfirmViewCallback callback;
    void* context;
};

typedef struct {
    char ssid[WFR_SSID_MAX_LEN + 1];
    uint8_t security;
    bool has_password;
    bool hidden;
    bool show_delete;
} WfrConfirmViewModel;

static const char* wfr_security_label(uint8_t security) {
    switch(security) {
    case WFR_SEC_OPEN:
        return "Open network";
    case WFR_SEC_WPA:
        return "WPA/WPA2";
    case WFR_SEC_WEP:
        return "WEP";
    case WFR_SEC_SAE:
        return "WPA3 (SAE)";
    default:
        return "Unknown";
    }
}

static void wfr_confirm_view_draw(Canvas* canvas, void* _model) {
    WfrConfirmViewModel* model = _model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* Header: network name */
    canvas_draw_icon(canvas, 2, 3, &I_wifi_small_9x5);
    canvas_set_font(canvas, FontPrimary);
    wfr_draw_str_fit(canvas, 14, 9, model->ssid, 112);
    wfr_draw_dotted_hline(canvas, 0, 12, 128);

    /* Details */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_icon(canvas, 1, 15, &I_lock_9x10);
    canvas_draw_str(canvas, 14, 23, wfr_security_label(model->security));
    canvas_draw_icon(canvas, 2, 29, &I_key_10x5);
    canvas_draw_str(canvas, 14, 35, model->has_password ? "********" : "No password");
    canvas_draw_icon(canvas, 2, 40, &I_wifi_small_9x5);
    canvas_draw_str(canvas, 14, 46, model->hidden ? "Hidden SSID" : "Broadcast SSID");

    /* Destination */
    canvas_draw_icon(canvas, 105, 18, &I_server_20x18);

    elements_button_left(canvas, "Back");
    elements_button_right(canvas, "Send");
    if(model->show_delete) elements_button_center(canvas, "Del");
}

static bool wfr_confirm_view_input(InputEvent* event, void* context) {
    WfrConfirmView* confirm = context;
    if(event->type != InputTypeShort) return false;

    bool show_delete = false;
    with_view_model(
        confirm->view,
        WfrConfirmViewModel * model,
        { show_delete = model->show_delete; },
        false);

    WfrConfirmResult result;
    switch(event->key) {
    case InputKeyLeft:
        result = WfrConfirmResultBack;
        break;
    case InputKeyRight:
        result = WfrConfirmResultSend;
        break;
    case InputKeyOk:
        result = show_delete ? WfrConfirmResultDelete : WfrConfirmResultSend;
        break;
    default:
        return false;
    }

    if(confirm->callback) confirm->callback(confirm->context, result);
    return true;
}

WfrConfirmView* wfr_confirm_view_alloc(void) {
    WfrConfirmView* confirm = malloc(sizeof(WfrConfirmView));
    confirm->callback = NULL;
    confirm->context = NULL;
    confirm->view = view_alloc();
    view_allocate_model(confirm->view, ViewModelTypeLocking, sizeof(WfrConfirmViewModel));
    view_set_context(confirm->view, confirm);
    view_set_draw_callback(confirm->view, wfr_confirm_view_draw);
    view_set_input_callback(confirm->view, wfr_confirm_view_input);
    return confirm;
}

void wfr_confirm_view_free(WfrConfirmView* confirm) {
    view_free(confirm->view);
    free(confirm);
}

View* wfr_confirm_view_get_view(WfrConfirmView* confirm) {
    return confirm->view;
}

void wfr_confirm_view_set_data(
    WfrConfirmView* confirm,
    const char* ssid,
    uint8_t security,
    bool has_password,
    bool hidden,
    bool show_delete) {
    with_view_model(
        confirm->view,
        WfrConfirmViewModel * model,
        {
            strlcpy(model->ssid, ssid, sizeof(model->ssid));
            model->security = security;
            model->has_password = has_password;
            model->hidden = hidden;
            model->show_delete = show_delete;
        },
        true);
}

void wfr_confirm_view_set_callback(
    WfrConfirmView* confirm,
    WfrConfirmViewCallback callback,
    void* context) {
    confirm->callback = callback;
    confirm->context = context;
}
