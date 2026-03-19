#include "../wi_fir.h"

static const char* security_names[] = {"Open", "WPA/WPA2", "WEP", "WPA3 (SAE)"};
#define SECURITY_COUNT 4

static void wi_fir_scene_edit_security_change_callback(VariableItem* item) {
    WiFirApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->security_type = index;
    variable_item_set_current_value_text(item, security_names[index]);
}

static void wi_fir_scene_edit_security_enter_callback(void* context, uint32_t index) {
    UNUSED(index);
    WiFirApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WiFirCustomEventSecurityDone);
}

void wi_fir_scene_edit_security_on_enter(void* context) {
    WiFirApp* app = context;

    variable_item_list_reset(app->variable_item_list);

    VariableItem* item = variable_item_list_add(
        app->variable_item_list,
        "Security",
        SECURITY_COUNT,
        wi_fir_scene_edit_security_change_callback,
        app);

    variable_item_set_current_value_index(item, app->security_type);
    variable_item_set_current_value_text(item, security_names[app->security_type]);

    /* Pressing OK/center on the item advances to confirm */
    variable_item_list_set_enter_callback(
        app->variable_item_list, wi_fir_scene_edit_security_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewVariableItemList);
}

bool wi_fir_scene_edit_security_on_event(void* context, SceneManagerEvent event) {
    WiFirApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WiFirCustomEventSecurityDone) {
            scene_manager_next_scene(app->scene_manager, WiFirSceneConfirm);
            consumed = true;
        }
    }

    return consumed;
}

void wi_fir_scene_edit_security_on_exit(void* context) {
    WiFirApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
