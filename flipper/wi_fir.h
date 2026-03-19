#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <notification/notification_messages.h>

#include "protocol/wfr_protocol.h"
#include "scenes/wi_fir_scene.h"

/* View IDs for ViewDispatcher */
typedef enum {
    WiFirViewSubmenu,
    WiFirViewTextInput,
    WiFirViewVariableItemList,
    WiFirViewDialogEx,
    WiFirViewPopup,
    WiFirViewLoading,
} WiFirView;

/* Main application struct */
typedef struct {
    /* GUI infrastructure */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    /* View modules */
    Submenu* submenu;
    TextInput* text_input;
    VariableItemList* variable_item_list;
    DialogEx* dialog_ex;
    Popup* popup;
    Loading* loading;

    /* WiFi credential data */
    char ssid[WFR_SSID_MAX_LEN + 1];
    char password[WFR_PASS_MAX_LEN + 1];
    uint8_t security_type;
    bool hidden;

    /* Text input buffer (shared across text input scenes) */
    char text_input_buf[WFR_PASS_MAX_LEN + 1];

    /* Confirm screen text buffer */
    char confirm_text[128];
} WiFirApp;

/* App entry point */
int32_t wi_fir_app(void* p);
