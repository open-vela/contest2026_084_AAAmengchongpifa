#ifndef VELAPET_UI_INTERNAL_H
#define VELAPET_UI_INTERNAL_H

#include "lvgl.h"
#include "velapet_ui.h"

#ifdef VELAPET_UI_USE_MOCK
#include "../vela_pet_api.h"
#else
#include <vela_pet_api.h>
#endif

/* Pet image shown while the AI image service has not produced one yet.  The
 * /data/agent/assets/pet/ prefix is rewritten to an LVGL drive letter by
 * ui_resolve_img_src(), which maps it onto the copy bundled in romfs.
 */
#ifndef VELAPET_UI_DEFAULT_ASSET
#define VELAPET_UI_DEFAULT_ASSET "/data/agent/assets/pet/home_pet.png"
#endif

typedef enum {
    UI_PAGE_HOME = 0,
    UI_PAGE_TASK,
    UI_PAGE_CHAT,
    UI_PAGE_PET_IMAGE,
    UI_PAGE_SKIN
} UiPage;

typedef enum {
    UI_PET_IMAGE_DEFAULT = 0,
    UI_PET_IMAGE_LOADING,
    UI_PET_IMAGE_READY,
    UI_PET_IMAGE_ERROR
} UiPetImageState;

typedef struct {
    const char *skin_name;
    lv_color_t bg_color;
    lv_color_t bg_grad_color;
    lv_color_t card_color;
    lv_color_t primary_color;
    lv_color_t accent_color;
    lv_color_t text_color;
    lv_color_t pet_color;
} VelaPetTheme;

const VelaPetTheme *ui_get_current_theme(void);
const VelaPetTheme *ui_get_theme_by_skin(const char *skin_name);
void ui_apply_theme_by_skin(const char *skin_name);
void ui_apply_theme_to_page(lv_obj_t *page);
int ui_skin_name_matches(const char *skin_value, const char *skin_id);

lv_obj_t *ui_create_page(void);
lv_obj_t *ui_create_title(lv_obj_t *parent, const char *title, int with_back);
lv_obj_t *ui_create_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data);
lv_obj_t *ui_create_card(lv_obj_t *parent, lv_coord_t width, lv_coord_t height);
lv_obj_t *ui_create_chip(lv_obj_t *parent, const char *label, const char *value);
lv_obj_t *ui_create_pet_avatar(lv_obj_t *parent, lv_coord_t width, lv_coord_t height, const char *mood);
lv_obj_t *ui_create_exp_bar(lv_obj_t *parent, int exp, int exp_to_next);
void ui_style_label(lv_obj_t *label, lv_color_t color);
void ui_show_toast(const char *msg);
void ui_show_levelup_dialog(const char *msg);
const char *ui_task_status_text(int status);
int ui_percent(int value, int total);
const char *ui_resolve_img_src(const char *asset_path);
int ui_show_image_src(lv_obj_t *img, const char *src, lv_coord_t max_width, lv_coord_t max_height);

void ui_router_show_home(void);
void ui_router_show_task(void);
void ui_router_show_chat(void);
void ui_router_show_pet_image(void);
void ui_router_show_skin(void);

void ui_home_create(void);
void ui_home_refresh(void);
void ui_home_on_destroy(void);
int ui_home_is_created(void);
void ui_task_create(void);
void ui_task_refresh(void);
void ui_chat_create(void);
void ui_chat_refresh(void);
void ui_chat_on_destroy(void);
void ui_pet_image_create(void);
void ui_pet_image_refresh(void);
void ui_pet_image_on_destroy(void);
UiPetImageState ui_pet_image_get_state(void);
const char *ui_pet_image_get_asset_path(void);
void ui_skin_create(void);
void ui_skin_refresh(void);

extern lv_obj_t *g_ui_root;
extern UiPage g_ui_page;

#endif
