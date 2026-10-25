#pragma once

#include <gui/view.h>

/* Network summary before sending: SSID, security, password, visibility. */

typedef struct WfrConfirmView WfrConfirmView;

typedef enum {
    WfrConfirmResultBack, /* Left */
    WfrConfirmResultSend, /* Right (or OK when delete is hidden) */
    WfrConfirmResultDelete, /* OK when delete is shown */
} WfrConfirmResult;

typedef void (*WfrConfirmViewCallback)(void* context, WfrConfirmResult result);

WfrConfirmView* wfr_confirm_view_alloc(void);
void wfr_confirm_view_free(WfrConfirmView* confirm);
View* wfr_confirm_view_get_view(WfrConfirmView* confirm);

void wfr_confirm_view_set_data(
    WfrConfirmView* confirm,
    const char* ssid,
    uint8_t security,
    bool has_password,
    bool hidden,
    bool show_delete);
void wfr_confirm_view_set_callback(
    WfrConfirmView* confirm,
    WfrConfirmViewCallback callback,
    void* context);
