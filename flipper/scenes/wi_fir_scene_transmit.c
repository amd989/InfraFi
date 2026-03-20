#include "../wi_fir.h"
#include "../wfr_encode.h"
#include "../wfr_storage.h"
#include <notification/notification_messages.h>

void wi_fir_scene_transmit_on_enter(void* context) {
    WiFirApp* app = context;

    /* Show loading animation while transmitting */
    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewLoading);

    /* Build credentials struct */
    WfrWifiCreds creds;
    memset(&creds, 0, sizeof(creds));
    strncpy(creds.ssid, app->ssid, WFR_SSID_MAX_LEN);
    strncpy(creds.password, app->password, WFR_PASS_MAX_LEN);
    creds.security = app->security_type;
    creds.hidden = app->hidden;

    /* Blink LED during transmission */
    notification_message(app->notifications, &sequence_blink_start_magenta);

    /* Transmit via IR */
    bool success = wfr_transmit_credentials(&creds);

    notification_message(app->notifications, &sequence_blink_stop);

    if(success) {
        notification_message(app->notifications, &sequence_success);
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventTransmitDone);
    } else {
        notification_message(app->notifications, &sequence_error);
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventTransmitFail);
    }
}

bool wi_fir_scene_transmit_on_event(void* context, SceneManagerEvent event) {
    WiFirApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WiFirCustomEventTransmitDone) {
            /* Auto-save credentials to SD card */
            WfrWifiCreds save_creds;
            memset(&save_creds, 0, sizeof(save_creds));
            strncpy(save_creds.ssid, app->ssid, WFR_SSID_MAX_LEN);
            strncpy(save_creds.password, app->password, WFR_PASS_MAX_LEN);
            save_creds.security = app->security_type;
            save_creds.hidden = app->hidden;
            wfr_storage_save(app->storage, &save_creds);

            /* Show success popup */
            popup_reset(app->popup);
            popup_set_header(app->popup, "Sent!", 64, 20, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup, "Credentials transmitted\nvia IR", 64, 40, AlignCenter, AlignCenter);
            popup_set_timeout(app->popup, 2000);
            popup_set_callback(app->popup, NULL);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewPopup);
            consumed = true;
        } else if(event.event == WiFirCustomEventTransmitFail) {
            /* Show error popup */
            popup_reset(app->popup);
            popup_set_header(app->popup, "Error!", 64, 20, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup, "IR transmission failed", 64, 40, AlignCenter, AlignCenter);
            popup_set_timeout(app->popup, 2000);
            popup_set_callback(app->popup, NULL);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewPopup);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        /* Go back to main menu after transmit */
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, WiFirSceneMainMenu);
        consumed = true;
    }

    return consumed;
}

void wi_fir_scene_transmit_on_exit(void* context) {
    WiFirApp* app = context;
    popup_reset(app->popup);
    notification_message(app->notifications, &sequence_blink_stop);
}
