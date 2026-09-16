#include "ui_internal.h"
#ifndef VELAPET_UI_USE_MOCK
#include <velapet_ai_bridge.h>
#endif
#include <stdio.h>

#ifndef VELAPET_UI_DEFAULT_IMAGE_SOURCE_PATH
#define VELAPET_UI_DEFAULT_IMAGE_SOURCE_PATH "/data/agent/assets/pet/source_pet.png"
#endif

static lv_obj_t *s_state_label;
static lv_obj_t *s_face_label;
static lv_obj_t *s_pet_image_img;
static lv_obj_t *s_pet_image_fallback;
static UiPetImageState s_state = UI_PET_IMAGE_DEFAULT;
static char s_asset_path[256] = "";

static void reload_cb(lv_event_t *e)
{
    (void)e;
#ifdef VELAPET_UI_USE_MOCK
    /* Simulate the final path returned by the AI image service. */
    velapet_ui_pet_image_set_state(UI_PET_IMAGE_READY,
                                   "/data/agent/assets/pet/home_pet.png");
    ui_show_toast("Mock pet image ready~");
#else
    /* The production app/upload flow should provide the user's real source
     * image path. This configurable path is only an integration fallback. */
    const char *source_path = VELAPET_UI_DEFAULT_IMAGE_SOURCE_PATH;
    velapet_ui_pet_image_set_state(UI_PET_IMAGE_LOADING, NULL);
    if (velapet_ai_request_pet_image(source_path) != 0) {
        /* The AI image service is not available (no transport yet).  Fall
         * back to the bundled image instead of only the drawn placeholder. */
        velapet_ui_pet_image_set_state(UI_PET_IMAGE_READY,
                                       VELAPET_UI_DEFAULT_ASSET);
        ui_show_toast("AI busy, default image shown");
    }
#endif
}

void velapet_ui_pet_image_set_state(int state, const char *asset_path)
{
    if (state < UI_PET_IMAGE_DEFAULT || state > UI_PET_IMAGE_ERROR) {
        state = UI_PET_IMAGE_ERROR;
    }
    s_state = (UiPetImageState)state;
    if (asset_path != NULL && asset_path[0] != '\0') {
        snprintf(s_asset_path, sizeof(s_asset_path), "%s", asset_path);
    } else if (s_state == UI_PET_IMAGE_READY || s_state == UI_PET_IMAGE_DEFAULT) {
        s_asset_path[0] = '\0';
    }

    if (g_ui_page == UI_PAGE_PET_IMAGE && s_state_label != NULL && s_pet_image_img != NULL) {
        ui_pet_image_refresh();
    } else if (g_ui_page == UI_PAGE_HOME && ui_home_is_created()) {
        ui_home_refresh();
    }
}

void ui_pet_image_create(void)
{
    const VelaPetTheme *theme = ui_get_current_theme();
    g_ui_root = ui_create_page();
    ui_create_title(g_ui_root, "Pet Image", 1);

    lv_obj_t *image_card = ui_create_card(g_ui_root, LV_PCT(86), 200);
    lv_obj_set_style_bg_color(image_card, theme->bg_grad_color, 0);
    lv_obj_set_flex_flow(image_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(image_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *pet_stage = lv_obj_create(image_card);
    lv_obj_remove_style_all(pet_stage);
    lv_obj_set_size(pet_stage, 190, 150);
    lv_obj_clear_flag(pet_stage, LV_OBJ_FLAG_SCROLLABLE);
    s_pet_image_fallback = ui_create_pet_avatar(pet_stage, 190, 150, "u");
    lv_obj_center(s_pet_image_fallback);
    s_pet_image_img = lv_img_create(pet_stage);
    lv_obj_center(s_pet_image_img);
    lv_obj_add_flag(s_pet_image_img, LV_OBJ_FLAG_HIDDEN);

    s_face_label = lv_label_create(image_card);
    lv_label_set_text(s_face_label, "Default pet");
    ui_style_label(s_face_label, theme->text_color);

    lv_obj_t *state_card = ui_create_card(g_ui_root, LV_PCT(92), 78);
    s_state_label = lv_label_create(state_card);
    lv_obj_set_width(s_state_label, LV_PCT(100));
    lv_label_set_long_mode(s_state_label, LV_LABEL_LONG_WRAP);
    ui_style_label(s_state_label, theme->text_color);

    ui_create_button(g_ui_root, "Reload", reload_cb, NULL);
    ui_pet_image_refresh();
}

void ui_pet_image_refresh(void)
{
    if (s_state_label == NULL || s_pet_image_img == NULL) return;

    char buf[320];
    const char *state_text = "Default pet";
    const char *face_text = "Default pet";
    const char *source_text = "Image source: default avatar";
    const char *src = ui_resolve_img_src(s_asset_path);
    int image_ok = s_state == UI_PET_IMAGE_READY &&
                   ui_show_image_src(s_pet_image_img, src, 190, 150);
    if (s_state == UI_PET_IMAGE_LOADING) {
        state_text = "Generating...";
        face_text = "Making a new look...";
        source_text = "Image source: waiting for generated image";
    }
    if (s_state == UI_PET_IMAGE_READY) {
        state_text = image_ok ? "Ready" : "Load failed";
        face_text = image_ok ? "Pet image ready" : "Use default pet";
        if (s_asset_path[0] != '\0') source_text = NULL;
    }
    if (s_state == UI_PET_IMAGE_ERROR) {
        state_text = "Load failed";
        face_text = "Use default pet";
        source_text = "Image source: failed, fallback to default";
    }

    if (image_ok) lv_obj_clear_flag(s_pet_image_img, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_pet_image_img, LV_OBJ_FLAG_HIDDEN);
    if (s_pet_image_fallback != NULL) {
        if (image_ok) lv_obj_add_flag(s_pet_image_fallback, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(s_pet_image_fallback, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_face_label != NULL) {
        lv_label_set_text(s_face_label, face_text);
    }
    if (source_text != NULL) {
        snprintf(buf, sizeof(buf), "%s\n%s", state_text, source_text);
    } else {
        snprintf(buf, sizeof(buf), "%s\nImage source: %s", state_text, s_asset_path);
    }
    lv_label_set_text(s_state_label, buf);
}

void ui_pet_image_on_destroy(void)
{
    s_state_label = NULL;
    s_face_label = NULL;
    s_pet_image_img = NULL;
    s_pet_image_fallback = NULL;
}

UiPetImageState ui_pet_image_get_state(void) { return s_state; }
const char *ui_pet_image_get_asset_path(void) { return s_asset_path[0] ? s_asset_path : NULL; }
