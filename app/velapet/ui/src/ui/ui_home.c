#include "ui_internal.h"
#include <stdio.h>

static lv_obj_t *s_name;
static lv_obj_t *s_level;
static lv_obj_t *s_exp_bar;
static lv_obj_t *s_exp_text;
static lv_obj_t *s_today_chip;
static lv_obj_t *s_steps_chip;
static lv_obj_t *s_checkin_chip;
static lv_obj_t *s_home_pet_img;
static lv_obj_t *s_home_pet_fallback;

static void task_cb(lv_event_t *e) { (void)e; ui_router_show_task(); }
static void chat_cb(lv_event_t *e) { (void)e; ui_router_show_chat(); }
static void image_cb(lv_event_t *e) { (void)e; ui_router_show_pet_image(); }
static void skin_cb(lv_event_t *e) { (void)e; ui_router_show_skin(); }

static void update_chip(lv_obj_t *chip, const char *label, const char *value)
{
    if (chip == NULL) return;
    lv_obj_t *txt = lv_obj_get_child(chip, 0);
    if (txt == NULL) return;

    char line[64];
    snprintf(line, sizeof(line), "%s\n%s", label ? label : "", value ? value : "");
    lv_label_set_text(txt, line);
}

static const char *skin_display_name(const char *skin)
{
    if (ui_skin_name_matches(skin, "sport_blue")) return "sport_blue";
    if (ui_skin_name_matches(skin, "flame_red")) return "flame_red";
    if (ui_skin_name_matches(skin, "star_purple")) return "star_purple";
    if (ui_skin_name_matches(skin, "rainbow")) return "rainbow";
    return "default";
}

void ui_home_create(void)
{
    const VelaPetTheme *theme = ui_get_current_theme();
    g_ui_root = ui_create_page();
    ui_create_title(g_ui_root, "VelaPet", 0);

    lv_obj_t *hero = lv_obj_create(g_ui_root);
    lv_obj_remove_style_all(hero);
    lv_obj_set_size(hero, LV_PCT(100), 190);
    lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *pet_stage = lv_obj_create(hero);
    lv_obj_remove_style_all(pet_stage);
    lv_obj_set_size(pet_stage, 220, 176);
    lv_obj_clear_flag(pet_stage, LV_OBJ_FLAG_SCROLLABLE);
    s_home_pet_fallback = ui_create_pet_avatar(pet_stage, 220, 176, "w");
    lv_obj_center(s_home_pet_fallback);
    s_home_pet_img = lv_img_create(pet_stage);
    lv_obj_center(s_home_pet_img);
    lv_obj_add_flag(s_home_pet_img, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *info = lv_obj_create(g_ui_root);
    lv_obj_remove_style_all(info);
    lv_obj_set_size(info, LV_PCT(100), 72);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_name = lv_label_create(info);
    ui_style_label(s_name, theme->text_color);

    s_level = lv_label_create(info);
    ui_style_label(s_level, theme->text_color);

    s_exp_text = lv_label_create(info);
    ui_style_label(s_exp_text, theme->text_color);

    s_exp_bar = ui_create_exp_bar(info, 0, 100);

    lv_obj_t *stats = lv_obj_create(g_ui_root);
    lv_obj_remove_style_all(stats);
    lv_obj_set_size(stats, LV_PCT(96), 50);
    lv_obj_set_flex_flow(stats, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stats, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_today_chip = ui_create_chip(stats, "Today", "0%");
    s_steps_chip = ui_create_chip(stats, "Steps", "0");
    s_checkin_chip = ui_create_chip(stats, "Check-in", "0d");

    lv_obj_t *grid = lv_obj_create(g_ui_root);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, LV_PCT(96), 72);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui_create_button(grid, "Task", task_cb, NULL);
    ui_create_button(grid, "AI", chat_cb, NULL);
    ui_create_button(grid, "Pet", image_cb, NULL);
    ui_create_button(grid, "Skin", skin_cb, NULL);

    ui_home_refresh();
}

void ui_home_refresh(void)
{
    PetProfile *pet = vela_pet_get_profile();
    int daily = vela_pet_get_daily_progress();
    char buf[96];
    char daily_buf[16];
    char steps_buf[24];
    char checkin_buf[16];
    const char *image_src = ui_resolve_img_src(ui_pet_image_get_asset_path());
    int image_ok = ui_pet_image_get_state() == UI_PET_IMAGE_READY &&
                   ui_show_image_src(s_home_pet_img, image_src, 220, 176);

    if (s_home_pet_img != NULL) {
        if (image_ok) lv_obj_clear_flag(s_home_pet_img, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_home_pet_img, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_home_pet_fallback != NULL) {
        if (image_ok) lv_obj_add_flag(s_home_pet_fallback, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(s_home_pet_fallback, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_name == NULL || s_level == NULL || s_exp_text == NULL || s_exp_bar == NULL) return;

    if (pet == NULL) {
        lv_label_set_text(s_name, "Vela");
        lv_label_set_text(s_level, "Lv.1");
        lv_label_set_text(s_exp_text, "EXP 0 / 100");
        lv_bar_set_value(s_exp_bar, 0, LV_ANIM_OFF);
        update_chip(s_today_chip, "Today", "0%");
        update_chip(s_steps_chip, "Steps", "0");
        update_chip(s_checkin_chip, "Check-in", "0d");
        return;
    }

    snprintf(buf, sizeof(buf), "%s", pet->name[0] ? pet->name : "Vela");
    lv_label_set_text(s_name, buf);

    snprintf(buf, sizeof(buf), "Lv.%d  Skin %s", pet->level, skin_display_name(pet->current_skin));
    lv_label_set_text(s_level, buf);

    snprintf(buf, sizeof(buf), "EXP %d / %d", pet->exp, pet->exp_to_next);
    lv_label_set_text(s_exp_text, buf);
    lv_bar_set_value(s_exp_bar, ui_percent(pet->exp, pet->exp_to_next), LV_ANIM_ON);

    snprintf(daily_buf, sizeof(daily_buf), "%d%%", daily);
    snprintf(steps_buf, sizeof(steps_buf), "%d", pet->total_steps);
    snprintf(checkin_buf, sizeof(checkin_buf), "%dd", pet->consecutive_days);
    update_chip(s_today_chip, "Today", daily_buf);
    update_chip(s_steps_chip, "Steps", steps_buf);
    update_chip(s_checkin_chip, "Check-in", checkin_buf);
}

void ui_home_on_destroy(void)
{
    s_name = NULL;
    s_level = NULL;
    s_exp_bar = NULL;
    s_exp_text = NULL;
    s_today_chip = NULL;
    s_steps_chip = NULL;
    s_checkin_chip = NULL;
    s_home_pet_img = NULL;
    s_home_pet_fallback = NULL;
}

int ui_home_is_created(void) { return s_home_pet_img != NULL; }
