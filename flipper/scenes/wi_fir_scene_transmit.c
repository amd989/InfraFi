#include "../wi_fir.h"
#include "../wfr_encode.h"
#include "../wfr_decode.h"
#include "../wfr_storage.h"
#include <infrared_worker.h>
#include <notification/notification_messages.h>

#define TAG "InfraFiTX"

#define TX_THREAD_STACK      2048
#define DISMISS_SENT_TICKS   25 /* 2.5s */
#define DISMISS_CONNECTED    60 /* 6s */

/* Whether the result on screen offers Retry (Right button) */
static bool retry_allowed = false;

static const char* wi_fir_proto_label(WiFirApp* app) {
    return (app->ir_protocol == WfrIrProtocolNEC) ? "NEC" : "RC-6";
}

/* ---------------------------------------------------------------- ACK receive */

/* IR receive callback — runs on IR worker thread */
static void wi_fir_ir_rx_callback(void* context, InfraredWorkerSignal* signal) {
    WiFirApp* app = context;

    if(!infrared_worker_signal_is_decoded(signal)) return;

    const InfraredMessage* message = infrared_worker_get_decoded_signal(signal);
    if(message->protocol != InfraredProtocolRC6 &&
       message->protocol != InfraredProtocolNEC) return;

    uint8_t address = (uint8_t)(message->address & 0xFF);
    uint8_t command = (uint8_t)(message->command & 0xFF);

    char payload[WFR_MAX_TOTAL_PAYLOAD + 1];
    int result = wfr_ack_decode_feed(
        &app->ack_decoder, address, command, payload, sizeof(payload));

    if(result > 0) {
        if(strncmp(payload, WFR_ACK_PREFIX_OK, strlen(WFR_ACK_PREFIX_OK)) == 0) {
            const char* ip = payload + strlen(WFR_ACK_PREFIX_OK);
            strncpy(app->ack_result_text, ip, sizeof(app->ack_result_text) - 1);
            app->ack_result_text[sizeof(app->ack_result_text) - 1] = '\0';
            view_dispatcher_send_custom_event(
                app->view_dispatcher, WiFirCustomEventAckSuccess);
        } else {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, WiFirCustomEventAckFail);
        }
    }
}

/* ACK timeout — no response from server within WFR_ACK_TIMEOUT_SEC */
static void wi_fir_ack_timeout_callback(void* context) {
    WiFirApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, WiFirCustomEventAckTimeout);
}

/* Stop ACK listener (IR worker + timer) — safe to call multiple times */
static void wi_fir_stop_ack_listener(WiFirApp* app) {
    if(app->ack_timer) {
        furi_timer_stop(app->ack_timer);
        furi_timer_free(app->ack_timer);
        app->ack_timer = NULL;
    }
    if(app->ir_worker) {
        infrared_worker_rx_stop(app->ir_worker);
        infrared_worker_free(app->ir_worker);
        app->ir_worker = NULL;
    }
}

static void wi_fir_start_ack_listener(WiFirApp* app) {
    FURI_LOG_I(TAG, "Starting ACK listener");
    wfr_ack_decode_init(&app->ack_decoder);
    app->ack_result_text[0] = '\0';

    app->ir_worker = infrared_worker_alloc();
    infrared_worker_rx_enable_signal_decoding(app->ir_worker, true);
    infrared_worker_rx_enable_blink_on_receiving(app->ir_worker, true);
    infrared_worker_rx_set_received_signal_callback(
        app->ir_worker, wi_fir_ir_rx_callback, app);
    infrared_worker_rx_start(app->ir_worker);

    app->ack_timer = furi_timer_alloc(
        wi_fir_ack_timeout_callback, FuriTimerTypeOnce, app);
    furi_timer_start(app->ack_timer, furi_ms_to_ticks(WFR_ACK_TIMEOUT_SEC * 1000));

    notification_message(app->notifications, &sequence_blink_start_cyan);
    wfr_status_view_set_waiting(app->status, WFR_ACK_TIMEOUT_SEC);
}

/* ---------------------------------------------------------------- IR transmit worker */

/* Runs on the TX thread after every payload byte */
static bool wi_fir_tx_progress(uint16_t sent, uint16_t total, void* context) {
    WiFirApp* app = context;
    wfr_status_view_set_transmit(app->status, sent, total, wi_fir_proto_label(app));
    return !app->tx_cancel;
}

static int32_t wi_fir_tx_worker(void* context) {
    WiFirApp* app = context;
    app->tx_ok = wfr_transmit_credentials(
        &app->tx_creds, app->ir_protocol, wi_fir_tx_progress, app);
    if(!app->tx_cancel) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WiFirCustomEventTxFinished);
    }
    return 0;
}

/* Cancel (if running) and reap the TX thread — safe to call multiple times */
static void wi_fir_stop_tx(WiFirApp* app) {
    if(app->tx_thread) {
        app->tx_cancel = true;
        furi_thread_join(app->tx_thread);
        furi_thread_free(app->tx_thread);
        app->tx_thread = NULL;
    }
}

static void wi_fir_start_tx(WiFirApp* app) {
    wi_fir_stop_ack_listener(app);
    wi_fir_stop_tx(app);
    notification_message(app->notifications, &sequence_blink_stop);

    memset(&app->tx_creds, 0, sizeof(app->tx_creds));
    strncpy(app->tx_creds.ssid, app->ssid, WFR_SSID_MAX_LEN);
    strncpy(app->tx_creds.password, app->password, WFR_PASS_MAX_LEN);
    app->tx_creds.security = app->security_type;
    app->tx_creds.hidden = app->hidden;
    app->tx_cancel = false;
    app->tx_ok = false;
    app->dismiss_ticks = 0;
    retry_allowed = false;

    wfr_status_view_set_transmit(app->status, 0, 0, wi_fir_proto_label(app));
    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewStatus);

    notification_message(app->notifications, &sequence_blink_start_magenta);

    app->tx_thread =
        furi_thread_alloc_ex("InfraFiTx", TX_THREAD_STACK, wi_fir_tx_worker, app);
    furi_thread_start(app->tx_thread);
}

/* ---------------------------------------------------------------- results */

static void wi_fir_show_failure(
    WiFirApp* app,
    WfrStatusBadge badge,
    const char* title,
    const char* line1,
    const char* line2,
    const char* retry_label) {
    retry_allowed = true;
    app->dismiss_ticks = 0;
    wfr_status_view_set_result(
        app->status, &I_server_20x18, badge, title, line1, line2, "Menu", retry_label);
}

static void wi_fir_on_tx_finished(WiFirApp* app) {
    wi_fir_stop_tx(app);
    notification_message(app->notifications, &sequence_blink_stop);

    if(!app->tx_ok) {
        notification_message(app->notifications, &sequence_error);
        wi_fir_show_failure(
            app, WfrStatusBadgeCross, "Send failed", "IR transmit error", "Try again", "Retry");
        return;
    }

    /* Auto-save credentials to SD card */
    wfr_storage_save(app->storage, &app->tx_creds);
    notification_message(app->notifications, &sequence_success);

    if(app->ack_enabled) {
        wi_fir_start_ack_listener(app);
    } else {
        wfr_status_view_set_result(
            app->status,
            &I_server_20x18,
            WfrStatusBadgeCheck,
            "Sent!",
            "Credentials sent",
            "over infrared",
            NULL,
            NULL);
        app->dismiss_ticks = DISMISS_SENT_TICKS;
    }
}

static void wi_fir_status_callback(void* context, InputKey key) {
    WiFirApp* app = context;
    uint32_t event = (key == InputKeyLeft)  ? WiFirCustomEventStatusLeft :
                     (key == InputKeyRight) ? WiFirCustomEventStatusRight :
                                              WiFirCustomEventStatusOk;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void wi_fir_dismiss(WiFirApp* app) {
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, WiFirSceneMainMenu);
}

/* ---------------------------------------------------------------- scene */

void wi_fir_scene_transmit_on_enter(void* context) {
    WiFirApp* app = context;
    wfr_status_view_set_callback(app->status, wi_fir_status_callback, app);
    wi_fir_start_tx(app);
}

bool wi_fir_scene_transmit_on_event(void* context, SceneManagerEvent event) {
    WiFirApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case WiFirCustomEventTxFinished:
            wi_fir_on_tx_finished(app);
            break;

        case WiFirCustomEventAckSuccess:
            wi_fir_stop_ack_listener(app);
            notification_message(app->notifications, &sequence_blink_stop);
            notification_message(app->notifications, &sequence_success);
            retry_allowed = false;
            wfr_status_view_set_connected(app->status, app->ack_result_text);
            app->dismiss_ticks = DISMISS_CONNECTED;
            break;

        case WiFirCustomEventAckFail:
            wi_fir_stop_ack_listener(app);
            notification_message(app->notifications, &sequence_blink_stop);
            notification_message(app->notifications, &sequence_error);
            wi_fir_show_failure(
                app, WfrStatusBadgeCross, "Join failed", "Server rejected", "the credentials", "Retry");
            break;

        case WiFirCustomEventAckTimeout:
            wi_fir_stop_ack_listener(app);
            notification_message(app->notifications, &sequence_blink_stop);
            wi_fir_show_failure(
                app, WfrStatusBadgeQuestion, "No reply", "Credentials sent,", "server is silent", "Resend");
            break;

        case WiFirCustomEventStatusLeft:
        case WiFirCustomEventStatusOk:
            wi_fir_dismiss(app);
            break;

        case WiFirCustomEventStatusRight:
            if(retry_allowed) wi_fir_start_tx(app);
            break;

        default:
            consumed = false;
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(app->dismiss_ticks > 0 && --app->dismiss_ticks == 0) {
            wi_fir_dismiss(app);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        /* Back cancels an in-flight transmission or wait */
        wi_fir_dismiss(app);
        consumed = true;
    }

    return consumed;
}

void wi_fir_scene_transmit_on_exit(void* context) {
    WiFirApp* app = context;
    wi_fir_stop_tx(app);
    wi_fir_stop_ack_listener(app);
    app->dismiss_ticks = 0;
    notification_message(app->notifications, &sequence_blink_stop);
}
