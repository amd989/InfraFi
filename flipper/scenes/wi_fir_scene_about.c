#include "../wi_fir.h"
#include "../protocol/version.h"

void wi_fir_scene_about_on_enter(void* context) {
    WiFirApp* app = context;
    wfr_status_view_set_about(app->status, INFRAFI_VERSION);
    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewStatus);
}

bool wi_fir_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wi_fir_scene_about_on_exit(void* context) {
    UNUSED(context);
}
