#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

#include <infrared_worker.h>

#include "protocol/wfr_protocol.h"
#include "wfr_decode.h"
#include "wfr_storage.h"
#include "scenes/wi_fir_scene.h"
#include "views/wfr_menu_view.h"
#include "views/wfr_list_view.h"
#include "views/wfr_confirm_view.h"
#include "views/wfr_status_view.h"
#include "infrafi_icons.h"

/* View IDs for ViewDispatcher */
typedef enum {
    WiFirViewMenu,
    WiFirViewList,
    WiFirViewConfirm,
    WiFirViewStatus,
    WiFirViewTextInput,
    WiFirViewVariableItemList,
    WiFirViewDialogEx,
} WiFirView;

/* Main application struct */
typedef struct {
    /* GUI infrastructure */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    /* Custom views */
    WfrMenuView* menu;
    WfrListView* list;
    WfrConfirmView* confirm;
    WfrStatusView* status;

    /* Stock view modules */
    TextInput* text_input;
    VariableItemList* variable_item_list;
    DialogEx* dialog_ex;

    /* Storage */
    Storage* storage;

    /* NFC (allocated per-scene to avoid holding the HAL) */
    Nfc* nfc;
    NfcPoller* nfc_poller;

    /* WiFi credential data (current working set) */
    char ssid[WFR_SSID_MAX_LEN + 1];
    char password[WFR_PASS_MAX_LEN + 1];
    uint8_t security_type;
    bool hidden;

    /* Text input buffer (shared across text input scenes) */
    char text_input_buf[WFR_PASS_MAX_LEN + 1];

    /* Dialog text buffer (delete confirmation) */
    char confirm_text[128];

    /* NFC scan result */
    WfrWifiCreds nfc_creds;

    /* Saved networks list (populated by saved scene) */
    char saved_ssids[WFR_SAVED_MAX][WFR_SSID_MAX_LEN + 1];
    char saved_files[WFR_SAVED_MAX][WFR_FILENAME_MAX];
    uint8_t saved_security[WFR_SAVED_MAX];
    uint8_t saved_count;

    /* Currently selected saved file (for delete from confirm screen) */
    char selected_saved_file[WFR_FILENAME_MAX];

    /* Settings */
    WfrIrProtocol ir_protocol;
    bool ack_enabled;

    /* IR ACK receive state */
    InfraredWorker* ir_worker;
    FuriTimer* ack_timer;
    WfrAckDecoder ack_decoder;
    char ack_result_text[64];

    /* IR transmit worker (keeps the UI animating while sending) */
    FuriThread* tx_thread;
    WfrWifiCreds tx_creds;
    volatile bool tx_cancel;
    bool tx_ok;

    /* Ticks (100ms) until a result screen dismisses itself; 0 = never */
    uint16_t dismiss_ticks;
} WiFirApp;

/* App entry point */
int32_t wi_fir_app(void* p);
