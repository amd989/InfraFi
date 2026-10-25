#include "../wi_fir.h"

enum {
    MainMenuIndexCredentials,
    MainMenuIndexScanNfc,
    MainMenuIndexSaved,
    MainMenuIndexSettings,
    MainMenuIndexAbout,
};

static void wi_fir_scene_main_menu_callback(void* context, uint32_t index) {
    WiFirApp* app = context;
    switch(index) {
    case MainMenuIndexCredentials:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventMainMenuCredentials);
        break;
    case MainMenuIndexScanNfc:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventMainMenuScanNfc);
        break;
    case MainMenuIndexSaved:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventMainMenuSaved);
        break;
    case MainMenuIndexSettings:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventMainMenuSettings);
        break;
    case MainMenuIndexAbout:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventMainMenuAbout);
        break;
    }
}

void wi_fir_scene_main_menu_on_enter(void* context) {
    WiFirApp* app = context;

    /* Order must match the MainMenuIndex enum */
    wfr_menu_view_reset(app->menu);
    wfr_menu_view_add_item(app->menu, "Send Credentials", &I_menu_keyboard_22x18);
    wfr_menu_view_add_item(app->menu, "Scan NFC Tag", &I_menu_nfc_20x16);
    wfr_menu_view_add_item(app->menu, "Saved Networks", &I_menu_saved_20x20);
    wfr_menu_view_add_item(app->menu, "Settings", &I_menu_settings_24x24);
    wfr_menu_view_add_item(app->menu, "About", &I_menu_about_20x20);
    wfr_menu_view_set_callback(app->menu, wi_fir_scene_main_menu_callback, app);

    /* Restore selected item when returning to this scene */
    wfr_menu_view_set_selected(
        app->menu, scene_manager_get_scene_state(app->scene_manager, WiFirSceneMainMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewMenu);
}

bool wi_fir_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    WiFirApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case WiFirCustomEventMainMenuCredentials:
            scene_manager_set_scene_state(
                app->scene_manager, WiFirSceneMainMenu, MainMenuIndexCredentials);
            scene_manager_next_scene(app->scene_manager, WiFirSceneEditSsid);
            consumed = true;
            break;
        case WiFirCustomEventMainMenuScanNfc:
            scene_manager_set_scene_state(
                app->scene_manager, WiFirSceneMainMenu, MainMenuIndexScanNfc);
            scene_manager_next_scene(app->scene_manager, WiFirSceneScanNfc);
            consumed = true;
            break;
        case WiFirCustomEventMainMenuSaved:
            scene_manager_set_scene_state(
                app->scene_manager, WiFirSceneMainMenu, MainMenuIndexSaved);
            scene_manager_next_scene(app->scene_manager, WiFirSceneSaved);
            consumed = true;
            break;
        case WiFirCustomEventMainMenuSettings:
            scene_manager_set_scene_state(
                app->scene_manager, WiFirSceneMainMenu, MainMenuIndexSettings);
            scene_manager_next_scene(app->scene_manager, WiFirSceneSettings);
            consumed = true;
            break;
        case WiFirCustomEventMainMenuAbout:
            scene_manager_set_scene_state(
                app->scene_manager, WiFirSceneMainMenu, MainMenuIndexAbout);
            scene_manager_next_scene(app->scene_manager, WiFirSceneAbout);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void wi_fir_scene_main_menu_on_exit(void* context) {
    UNUSED(context);
}
