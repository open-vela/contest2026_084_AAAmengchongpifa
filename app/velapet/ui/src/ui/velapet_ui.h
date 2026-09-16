#ifndef VELAPET_UI_H
#define VELAPET_UI_H

#ifdef __cplusplus
extern "C" {
#endif

int velapet_ui_init(void);
void velapet_ui_show_home(void);
void velapet_ui_show_task(void);
void velapet_ui_show_chat(void);
void velapet_ui_show_pet_image(void);
void velapet_ui_show_skin(void);
void velapet_ui_refresh_all(void);

void velapet_ui_chat_set_reply(const char *reply);
void velapet_ui_chat_set_loading(int loading);
void velapet_ui_chat_set_error(const char *error_msg);
void velapet_ui_pet_image_set_state(int state, const char *asset_path);

#ifdef __cplusplus
}
#endif

#endif
