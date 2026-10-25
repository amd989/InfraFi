#pragma once

#include <gui/view.h>
#include <input/input.h>

/* Animated full-screen status view: transmit, waiting, results, NFC scan, about.
 * Runs its own frame timer while it is on screen. */

typedef struct WfrStatusView WfrStatusView;

typedef enum {
    WfrStatusBadgeCheck,
    WfrStatusBadgeCross,
    WfrStatusBadgeQuestion,
} WfrStatusBadge;

/* Called for Left / OK / Right presses on result screens. */
typedef void (*WfrStatusViewCallback)(void* context, InputKey key);

WfrStatusView* wfr_status_view_alloc(void);
void wfr_status_view_free(WfrStatusView* status);
View* wfr_status_view_get_view(WfrStatusView* status);
void wfr_status_view_set_callback(WfrStatusView* status, WfrStatusViewCallback callback, void* context);

/* IR beam animation with progress bar. Safe to call from any thread. */
void wfr_status_view_set_transmit(WfrStatusView* status, uint16_t sent, uint16_t total, const char* proto);

/* Return pulses from the server with a countdown. */
void wfr_status_view_set_waiting(WfrStatusView* status, uint8_t timeout_sec);

/* Server online with its IP address. OK dismisses. */
void wfr_status_view_set_connected(WfrStatusView* status, const char* ip);

/* Generic result: illustration + badge, title, two lines, optional buttons (NULL hides). */
void wfr_status_view_set_result(
    WfrStatusView* status,
    const Icon* subject,
    WfrStatusBadge badge,
    const char* title,
    const char* line1,
    const char* line2,
    const char* btn_left,
    const char* btn_right);

/* Hold-tag-to-back NFC animation. */
void wfr_status_view_set_nfc_scan(WfrStatusView* status);

/* About screen. */
void wfr_status_view_set_about(WfrStatusView* status, const char* version);
