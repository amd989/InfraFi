#include "../wi_fir.h"
#include "../wfr_storage.h"
#include <stdio.h>

static void wi_fir_scene_confirm_delete_callback(DialogExResult result, void* context) {
    WiFirApp* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventDeleteConfirmed);
    } else {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventDeleteCancelled);
    }
}

static void wi_fir_scene_confirm_callback(void* context, WfrConfirmResult result) {
    WiFirApp* app = context;
    switch(result) {
    case WfrConfirmResultSend:
        view_dispatcher_send_custom_event(app->view_dispatcher, WiFirCustomEventConfirmSend);
        break;
    case WfrConfirmResultBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, WiFirCustomEventConfirmBack);
        break;
    case WfrConfirmResultDelete:
        view_dispatcher_send_custom_event(app->view_dispatcher, WiFirCustomEventConfirmDelete);
        break;
    }
}

void wi_fir_scene_confirm_on_enter(void* context) {
    WiFirApp* app = context;

    wfr_confirm_view_set_data(
        app->confirm,
        app->ssid,
        app->security_type,
        app->password[0] != '\0',
        app->hidden,
        app->selected_saved_file[0] != '\0'); /* delete only for saved networks */
    wfr_confirm_view_set_callback(app->confirm, wi_fir_scene_confirm_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewConfirm);
}

static void wi_fir_scene_confirm_show_delete_dialog(WiFirApp* app) {
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Delete?", 64, 0, AlignCenter, AlignTop);
    snprintf(
        app->confirm_text,
        sizeof(app->confirm_text),
        "Remove \"%s\"\nfrom saved networks?",
        app->ssid);
    dialog_ex_set_text(app->dialog_ex, app->confirm_text, 64, 28, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "No");
    dialog_ex_set_right_button_text(app->dialog_ex, "Yes");
    dialog_ex_set_result_callback(app->dialog_ex, wi_fir_scene_confirm_delete_callback);
    dialog_ex_set_context(app->dialog_ex, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewDialogEx);
}

bool wi_fir_scene_confirm_on_event(void* context, SceneManagerEvent event) {
    WiFirApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WiFirCustomEventConfirmSend) {
            scene_manager_next_scene(app->scene_manager, WiFirSceneTransmit);
            consumed = true;
        } else if(event.event == WiFirCustomEventConfirmBack) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == WiFirCustomEventConfirmDelete) {
            if(app->selected_saved_file[0] != '\0') {
                wi_fir_scene_confirm_show_delete_dialog(app);
            }
            consumed = true;
        } else if(event.event == WiFirCustomEventDeleteConfirmed) {
            wfr_storage_delete(app->storage, app->selected_saved_file);
            app->selected_saved_file[0] = '\0';
            /* Go back to saved list */
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == WiFirCustomEventDeleteCancelled) {
            view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewConfirm);
            consumed = true;
        }
    }

    return consumed;
}

void wi_fir_scene_confirm_on_exit(void* context) {
    WiFirApp* app = context;
    dialog_ex_reset(app->dialog_ex);
}
