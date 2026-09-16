#include "ui_internal.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *unlock_rule;
} SkinItem;

static const SkinItem k_skins[] = {
    {"default", "unlocked"},
    {"sport_blue", "Lv.2"},
    {"flame_red", "Lv.5"},
    {"star_purple", "Lv.10"},
    {"rainbow", "3-day check-in"},
};

static lv_obj_t *s_list;
static lv_obj_t *s_current_label;
static lv_obj_t *s_current_card;

static int is_unlocked(PetProfile *pet, const char *name)
{
    if (ui_skin_name_matches(name, "default")) return 1;
    if (pet == NULL) return 0;
    for (int i = 0; i < pet->unlocked_skin_count; ++i) {
        if (ui_skin_name_matches(pet->unlocked_skins[i], name)) {
            return 1;
        }
    }
    return 0;
}

static const char *find_canonical_skin_arg(PetProfile *pet, const char *skin_id)
{
    if (pet == NULL || skin_id == NULL) {
        return skin_id;
    }

    for (int i = 0; i < pet->unlocked_skin_count; ++i) {
        const char *value = pet->unlocked_skins[i];
        if (ui_skin_name_matches(value, skin_id)) {
            return value;
        }
    }

    return skin_id;
}

static void skin_cb(lv_event_t *e)
{
    const char *skin_id = (const char *)lv_event_get_user_data(e);
    PetProfile *pet = vela_pet_get_profile();
    const char *arg = find_canonical_skin_arg(pet, skin_id);

    if (vela_pet_switch_skin(arg) == 0) {
        ui_apply_theme_by_skin(arg);
        ui_apply_theme_to_page(g_ui_root);
        ui_show_toast("Theme changed!");
        ui_skin_refresh();
    } else {
        ui_show_toast("Skin is locked");
    }
}

static void add_skin_row(PetProfile *pet, const SkinItem *skin)
{
    const VelaPetTheme *theme = ui_get_current_theme();
    int unlocked = is_unlocked(pet, skin->name);
    int using_skin = 0;
    char line[96];

    if (pet != NULL && ui_skin_name_matches(pet->current_skin, skin->name)) {
        using_skin = 1;
    }

    const VelaPetTheme *skin_theme = ui_get_theme_by_skin(skin->name);
    lv_obj_t *row = ui_create_card(s_list, LV_PCT(96), 82);
    lv_obj_set_style_bg_color(row, using_skin ? theme->bg_grad_color : theme->card_color, 0);
    lv_obj_set_style_border_width(row, using_skin ? 3 : 1, 0);
    lv_obj_set_style_border_color(row, using_skin ? theme->primary_color : theme->accent_color, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *thumb = lv_obj_create(row);
    lv_obj_set_size(thumb, 42, 42);
    lv_obj_set_style_radius(thumb, 16, 0);
    lv_obj_set_style_bg_color(thumb, skin_theme->pet_color, 0);
    lv_obj_set_style_border_width(thumb, 1, 0);
    lv_obj_set_style_border_color(thumb, lv_color_hex(0xffffff), 0);
    lv_obj_clear_flag(thumb, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *thumb_face = lv_label_create(thumb);
    lv_label_set_text(thumb_face, "Pet");
    ui_style_label(thumb_face, skin_theme->text_color);
    lv_obj_center(thumb_face);

    snprintf(line, sizeof(line), "%s\n%s", skin->name, unlocked ? "Unlocked" : skin->unlock_rule);
    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, line);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, LV_PCT(48));
    ui_style_label(label, theme->text_color);

    lv_obj_t *btn = ui_create_button(row, using_skin ? "Using" : (unlocked ? "Use" : "Locked"),
                                     (unlocked && !using_skin) ? skin_cb : NULL,
                                     (void *)skin->name);
    if (!unlocked || using_skin) {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xd0d5dd), LV_STATE_DISABLED);
    }
}

void ui_skin_create(void)
{
    g_ui_root = ui_create_page();
    ui_create_title(g_ui_root, "Skin Gallery", 1);

    s_current_card = ui_create_card(g_ui_root, LV_PCT(92), 52);
    s_current_label = lv_label_create(s_current_card);
    lv_obj_set_width(s_current_label, LV_PCT(100));
    ui_style_label(s_current_label, ui_get_current_theme()->text_color);

    s_list = lv_obj_create(g_ui_root);
    lv_obj_set_size(s_list, LV_PCT(100), LV_PCT(68));
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 10, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);

    ui_skin_refresh();
}

void ui_skin_refresh(void)
{
    if (s_list == NULL) return;
    lv_obj_clean(s_list);

    PetProfile *pet = vela_pet_get_profile();
    const VelaPetTheme *theme = ui_get_current_theme();
    if (s_current_card != NULL) {
        lv_obj_set_style_bg_color(s_current_card, theme->bg_grad_color, 0);
        lv_obj_set_style_border_color(s_current_card, theme->primary_color, 0);
    }
    if (s_current_label != NULL) {
        const char *current = "default";
        if (pet != NULL && pet->current_skin[0] != '\0') {
            current = pet->current_skin;
        }
        char text[80];
        snprintf(text, sizeof(text), "Current skin: %s", current);
        lv_label_set_text(s_current_label, text);
        ui_style_label(s_current_label, theme->text_color);
    }

    for (unsigned i = 0; i < sizeof(k_skins) / sizeof(k_skins[0]); ++i) {
        add_skin_row(pet, &k_skins[i]);
    }
}
