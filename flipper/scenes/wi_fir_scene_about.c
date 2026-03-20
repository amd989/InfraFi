#include "../wi_fir.h"

void wi_fir_scene_about_on_enter(void* context) {
    WiFirApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, "InfraFi v1.0", 64, 4, AlignCenter, AlignTop);
    popup_set_text(
        app->popup,
        "WiFi over Infrared\n"
        "NFC + Manual + Saved\n\n"
        "github.com/amd989",
        64,
        28,
        AlignCenter,
        AlignCenter);
    popup_disable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewPopup);
}

bool wi_fir_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wi_fir_scene_about_on_exit(void* context) {
    WiFirApp* app = context;
    popup_reset(app->popup);
}
