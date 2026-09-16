#include "ui_internal.h"
#include <stdio.h>
#include <string.h>

/* Simulator default. The production target must override this with the
 * letter registered by its LVGL file-system driver, for example "A:". */
#ifndef VELAPET_UI_LVGL_FS_PREFIX
#define VELAPET_UI_LVGL_FS_PREFIX "S:"
#endif

#define UI_COLOR_BG        0xf8f1ff
#define UI_COLOR_CARD      0xffffff
#define UI_COLOR_CARD_SOFT 0xfff7fb
#define UI_COLOR_PRIMARY   0xff8fc7
#define UI_COLOR_ACCENT    0x8bd8c7
#define UI_COLOR_TEXT      0x344054
#define UI_COLOR_MUTED     0x667085
#define UI_COLOR_LINE      0xf4cddd
#define UI_COLOR_CREAM     0xfff8df
#define UI_COLOR_PET       0xffb6d8

static VelaPetTheme g_themes[5];
static int g_themes_ready = 0;
static const VelaPetTheme *g_current_theme = NULL;

static void back_event_cb(lv_event_t *e)
{
    (void)e;
    ui_router_show_home();
}

static void toast_close_cb(lv_timer_t *timer)
{
    lv_obj_t *toast = (lv_obj_t *)timer->user_data;
    if (toast != NULL) {
        lv_obj_del_async(toast);
    }
    lv_timer_del(timer);
}

static void init_themes(void)
{
    if (g_themes_ready) return;

    g_themes[0] = (VelaPetTheme){
        "default",
        lv_color_hex(0xf8f1ff),
        lv_color_hex(0xfff7fb),
        lv_color_hex(0xffffff),
        lv_color_hex(0xff8fc7),
        lv_color_hex(0x8bd8c7),
        lv_color_hex(0x344054),
        lv_color_hex(0xffb6d8),
    };
    g_themes[1] = (VelaPetTheme){
        "sport_blue",
        lv_color_hex(0xeff8ff),
        lv_color_hex(0xe0f2fe),
        lv_color_hex(0xf8fbff),
        lv_color_hex(0x4aa3ff),
        lv_color_hex(0x6bd3ff),
        lv_color_hex(0x243b53),
        lv_color_hex(0x8bd3ff),
    };
    g_themes[2] = (VelaPetTheme){
        "flame_red",
        lv_color_hex(0xfff3ee),
        lv_color_hex(0xffe3d7),
        lv_color_hex(0xfffbf8),
        lv_color_hex(0xff8a6b),
        lv_color_hex(0xffc078),
        lv_color_hex(0x4a2f2a),
        lv_color_hex(0xffaa8f),
    };
    g_themes[3] = (VelaPetTheme){
        "star_purple",
        lv_color_hex(0xf5f0ff),
        lv_color_hex(0xebe1ff),
        lv_color_hex(0xfdfbff),
        lv_color_hex(0xa98cff),
        lv_color_hex(0xd5b8ff),
        lv_color_hex(0x352b4b),
        lv_color_hex(0xc4a8ff),
    };
    g_themes[4] = (VelaPetTheme){
        "rainbow",
        lv_color_hex(0xfffbf0),
        lv_color_hex(0xf1fff8),
        lv_color_hex(0xffffff),
        lv_color_hex(0xff9ecb),
        lv_color_hex(0x79d7c8),
        lv_color_hex(0x344054),
        lv_color_hex(0xffd36e),
    };

    g_current_theme = &g_themes[0];
    g_themes_ready = 1;
}

const VelaPetTheme *ui_get_theme_by_skin(const char *skin_name)
{
    init_themes();
    if (skin_name != NULL) {
        for (unsigned i = 0; i < sizeof(g_themes) / sizeof(g_themes[0]); ++i) {
            const char *name = g_themes[i].skin_name;
            if (ui_skin_name_matches(skin_name, name)) {
                return &g_themes[i];
            }
        }
    }
    return &g_themes[0];
}

int ui_skin_name_matches(const char *skin_value, const char *skin_id)
{
    if (skin_value == NULL || skin_id == NULL) {
        return 0;
    }

    if (strcmp(skin_value, skin_id) == 0) {
        return 1;
    }

    if (strstr(skin_value, skin_id) != NULL) {
        return 1;
    }

    return 0;
}

const VelaPetTheme *ui_get_current_theme(void)
{
    init_themes();
    return g_current_theme ? g_current_theme : &g_themes[0];
}

void ui_apply_theme_by_skin(const char *skin_name)
{
    g_current_theme = ui_get_theme_by_skin(skin_name);
}

void ui_apply_theme_to_page(lv_obj_t *page)
{
    if (page == NULL) return;
    const VelaPetTheme *theme = ui_get_current_theme();
    lv_obj_set_style_bg_color(page, theme->bg_color, 0);
    lv_obj_set_style_bg_grad_color(page, theme->bg_grad_color, 0);
    lv_obj_set_style_bg_grad_dir(page, LV_GRAD_DIR_VER, 0);
}

lv_obj_t *ui_create_page(void)
{
    const VelaPetTheme *theme = ui_get_current_theme();
    lv_obj_t *page = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(page, theme->bg_color, 0);
    lv_obj_set_style_bg_grad_color(page, theme->bg_grad_color, 0);
    lv_obj_set_style_bg_grad_dir(page, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(page, 9, 0);
    lv_obj_set_style_pad_row(page, 6, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return page;
}

lv_obj_t *ui_create_title(lv_obj_t *parent, const char *title, int with_back)
{
    const VelaPetTheme *theme = ui_get_current_theme();
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), 28);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (with_back) {
        ui_create_button(row, "<", back_event_cb, NULL);
    }

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, theme->text_color, 0);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);

    if (with_back) {
        lv_obj_t *space = lv_obj_create(row);
        lv_obj_remove_style_all(space);
        lv_obj_set_size(space, 38, 28);
    }
    return label;
}

lv_obj_t *ui_create_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data)
{
    const VelaPetTheme *theme = ui_get_current_theme();
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 76, 34);
    lv_obj_set_style_radius(btn, 17, 0);
    lv_obj_set_style_bg_color(btn, theme->primary_color, 0);
    lv_obj_set_style_bg_color(btn, theme->accent_color, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0xe9a8c5), 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
    lv_obj_set_style_pad_hor(btn, 8, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
    lv_obj_center(label);

    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    return btn;
}

lv_obj_t *ui_create_card(lv_obj_t *parent, lv_coord_t width, lv_coord_t height)
{
    const VelaPetTheme *theme = ui_get_current_theme();
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, width, height);
    lv_obj_set_style_radius(card, 22, 0);
    lv_obj_set_style_bg_color(card, theme->card_color, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_80, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, theme->accent_color, 0);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0xe9d5ff), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    return card;
}

lv_obj_t *ui_create_chip(lv_obj_t *parent, const char *label, const char *value)
{
    const VelaPetTheme *theme = ui_get_current_theme();
    char text[64];
    snprintf(text, sizeof(text), "%s\n%s", label ? label : "", value ? value : "");

    lv_obj_t *chip = lv_obj_create(parent);
    lv_obj_set_size(chip, 78, 42);
    lv_obj_set_style_radius(chip, 18, 0);
    lv_obj_set_style_bg_color(chip, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_70, 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_set_style_border_color(chip, theme->accent_color, 0);
    lv_obj_set_style_pad_all(chip, 5, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *txt = lv_label_create(chip);
    lv_label_set_text(txt, text);
    lv_obj_set_width(txt, LV_PCT(100));
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
    ui_style_label(txt, theme->text_color);
    lv_obj_center(txt);
    return chip;
}

static lv_obj_t *pet_part(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, lv_coord_t h, lv_color_t color, int radius)
{
    lv_obj_t *part = lv_obj_create(parent);
    lv_obj_remove_style_all(part);
    lv_obj_set_pos(part, x, y);
    lv_obj_set_size(part, w, h);
    lv_obj_set_style_radius(part, radius, 0);
    lv_obj_set_style_bg_color(part, color, 0);
    lv_obj_set_style_bg_opa(part, LV_OPA_COVER, 0);
    return part;
}

lv_obj_t *ui_create_pet_avatar(lv_obj_t *parent, lv_coord_t width, lv_coord_t height, const char *mood)
{
    const VelaPetTheme *theme = ui_get_current_theme();
    const char *face = mood ? mood : "^";
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, width, height);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_coord_t cx = width / 2;
    lv_coord_t head = height * 44 / 100;
    if (head < 56) head = 56;
    lv_coord_t body_w = width * 48 / 100;
    lv_coord_t body_h = height * 28 / 100;
    lv_coord_t head_x = cx - head / 2;
    lv_coord_t head_y = height * 16 / 100;

    pet_part(box, head_x - 8, head_y + 2, 26, 34, theme->pet_color, LV_RADIUS_CIRCLE);
    pet_part(box, head_x + head - 18, head_y + 2, 26, 34, theme->pet_color, LV_RADIUS_CIRCLE);
    pet_part(box, head_x, head_y, head, head, theme->pet_color, LV_RADIUS_CIRCLE);
    pet_part(box, cx - body_w / 2, head_y + head - 4, body_w, body_h, theme->pet_color, LV_RADIUS_CIRCLE);

    pet_part(box, cx - 18, head_y + head / 2 - 4, 8, 8, theme->text_color, LV_RADIUS_CIRCLE);
    pet_part(box, cx + 10, head_y + head / 2 - 4, 8, 8, theme->text_color, LV_RADIUS_CIRCLE);
    pet_part(box, cx - 34, head_y + head / 2 + 10, 14, 8, theme->bg_grad_color, LV_RADIUS_CIRCLE);
    pet_part(box, cx + 20, head_y + head / 2 + 10, 14, 8, theme->bg_grad_color, LV_RADIUS_CIRCLE);

    lv_obj_t *mouth = lv_label_create(box);
    lv_label_set_text(mouth, face);
    lv_obj_set_style_text_color(mouth, theme->text_color, 0);
    lv_obj_align(mouth, LV_ALIGN_CENTER, 0, -2);
    return box;
}

lv_obj_t *ui_create_exp_bar(lv_obj_t *parent, int exp, int exp_to_next)
{
    const VelaPetTheme *theme = ui_get_current_theme();
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, LV_PCT(82), 12);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, ui_percent(exp, exp_to_next), LV_ANIM_ON);
    lv_obj_set_style_radius(bar, 12, 0);
    lv_obj_set_style_radius(bar, 12, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, theme->bg_grad_color, 0);
    lv_obj_set_style_bg_color(bar, theme->accent_color, LV_PART_INDICATOR);
    return bar;
}

void ui_style_label(lv_obj_t *label, lv_color_t color)
{
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);
}

void ui_show_toast(const char *msg)
{
    lv_obj_t *toast = lv_obj_create(lv_layer_top());
    lv_obj_set_size(toast, LV_PCT(82), 42);
    lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_style_radius(toast, 20, 0);
    lv_obj_set_style_bg_color(toast, lv_color_hex(0x5a5066), 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_90, 0);
    lv_obj_set_style_border_width(toast, 0, 0);
    lv_obj_set_style_shadow_width(toast, 8, 0);

    lv_obj_t *label = lv_label_create(toast);
    lv_label_set_text(label, msg ? msg : "");
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
    lv_obj_center(label);

    lv_timer_t *timer = lv_timer_create(toast_close_cb, 1400, toast);
    lv_timer_set_repeat_count(timer, 1);
}

void ui_show_levelup_dialog(const char *msg)
{
    if (msg != NULL && msg[0] != '\0') {
        ui_show_toast(msg);
    }
}

const char *ui_task_status_text(int status)
{
    switch (status) {
    case TASK_STATUS_NOT_STARTED:
        return "Not started";
    case TASK_STATUS_IN_PROGRESS:
        return "In progress";
    case TASK_STATUS_COMPLETED:
        return "Claim";
    case TASK_STATUS_REWARD_CLAIMED:
        return "Claimed";
    default:
        return "Unknown";
    }
}

int ui_percent(int value, int total)
{
    if (total <= 0) {
        return 0;
    }
    int pct = (value * 100) / total;
    if (pct < 0) return 0;
    if (pct > 100) return 100;
    return pct;
}

const char *ui_resolve_img_src(const char *asset_path)
{
    static char resolved[256];
    static const char prefix[] = "/data/agent/assets/pet/";
    const char *name;

    if (asset_path == NULL || asset_path[0] == '\0') return NULL;
    if (asset_path[0] != '/' || strncmp(asset_path, prefix, sizeof(prefix) - 1) != 0) {
        return asset_path;
    }

    name = asset_path + sizeof(prefix) - 1;
    if (name[0] == '\0') return NULL;
    snprintf(resolved, sizeof(resolved), "%s/%s",
             VELAPET_UI_LVGL_FS_PREFIX, name);
    return resolved;
}

int ui_show_image_src(lv_obj_t *img, const char *src, lv_coord_t max_width, lv_coord_t max_height)
{
    lv_image_header_t header;
    uint32_t zoom = 256;

    if (img == NULL || src == NULL || src[0] == '\0') return 0;
    lv_image_cache_drop(src);
    if (lv_image_decoder_get_info(src, &header) != LV_RES_OK || header.w == 0 || header.h == 0) return 0;

    if (header.w > (uint32_t)max_width) zoom = ((uint32_t)max_width * 256U) / header.w;
    if (((uint32_t)header.h * zoom) / 256U > (uint32_t)max_height) {
        zoom = ((uint32_t)max_height * 256U) / header.h;
    }
    lv_img_set_src(img, src);
    lv_img_set_zoom(img, (uint16_t)zoom);
    return 1;
}
