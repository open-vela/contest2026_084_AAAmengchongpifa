#include "ui/velapet_ui.h"

/*
 * Example only. In the real openvela app, call this after LVGL, display,
 * touch, and business modules have been initialized.
 */
int velapet_app_ui_start(void)
{
    return velapet_ui_init();
}

void velapet_app_on_step_or_task_changed(void)
{
    velapet_ui_refresh_all();
}

void velapet_app_on_ai_reply(const char *reply)
{
    velapet_ui_chat_set_loading(0);
    velapet_ui_chat_set_reply(reply);
}

void velapet_app_on_ai_error(const char *error_msg)
{
    velapet_ui_chat_set_loading(0);
    velapet_ui_chat_set_error(error_msg);
}

void velapet_app_on_pet_asset_changed(int state, const char *asset_path)
{
    velapet_ui_pet_image_set_state(state, asset_path);
}
