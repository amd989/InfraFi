#include "../wi_fir.h"

enum {
    MainMenuIndexCredentials,
    MainMenuIndexAbout,
};

static void wi_fir_scene_main_menu_callback(void* context, uint32_t index) {
    WiFirApp* app = context;
    if(index == MainMenuIndexCredentials) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventMainMenuCredentials);
    } else if(index == MainMenuIndexAbout) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventMainMenuAbout);
    }
}

void wi_fir_scene_main_menu_on_enter(void* context) {
    WiFirApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Wi-Fir");
    submenu_add_item(
        app->submenu, "Send Credentials", MainMenuIndexCredentials,
        wi_fir_scene_main_menu_callback, app);
    submenu_add_item(
        app->submenu, "About", MainMenuIndexAbout,
        wi_fir_scene_main_menu_callback, app);

    /* Restore selected item when returning to this scene */
    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, WiFirSceneMainMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewSubmenu);
}

bool wi_fir_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    WiFirApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WiFirCustomEventMainMenuCredentials) {
            scene_manager_set_scene_state(
                app->scene_manager, WiFirSceneMainMenu, MainMenuIndexCredentials);
            scene_manager_next_scene(app->scene_manager, WiFirSceneEditSsid);
            consumed = true;
        } else if(event.event == WiFirCustomEventMainMenuAbout) {
            scene_manager_set_scene_state(
                app->scene_manager, WiFirSceneMainMenu, MainMenuIndexAbout);
            scene_manager_next_scene(app->scene_manager, WiFirSceneAbout);
            consumed = true;
        }
    }

    return consumed;
}

void wi_fir_scene_main_menu_on_exit(void* context) {
    WiFirApp* app = context;
    submenu_reset(app->submenu);
}
