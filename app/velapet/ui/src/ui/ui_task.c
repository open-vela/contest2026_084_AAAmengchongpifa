#include "ui_internal.h"
#include <stdio.h>
#include <stdint.h>

static lv_obj_t *s_list;
static lv_obj_t *s_checkin_btn = NULL;

static const char *task_icon_for(int index)
{
    static const char *icons[] = {"STEP", "MOVE", "STAR", "GO"};
    return icons[index % 4];
}

static void claim_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    int exp = vela_pet_claim_task(index);
    if (exp > 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "+%d EXP! Vela is happy~", exp);
        ui_show_toast(msg);
        const char *level_msg = vela_pet_get_levelup_message();
        if (level_msg != NULL && level_msg[0] != '\0') {
            ui_show_toast("Level up!");
        }
        ui_task_refresh();
    } else {
        ui_show_toast("Task is not ready");
    }
}

static void refresh_checkin_button(void)
{
    if (s_checkin_btn == NULL) {
        return;
    }

    lv_obj_t *label = lv_obj_get_child(s_checkin_btn, 0);
    if (vela_pet_can_checkin()) {
        lv_obj_clear_state(s_checkin_btn, LV_STATE_DISABLED);
        if (label != NULL) {
            lv_label_set_text(label, "Check in");
        }
    } else {
        lv_obj_add_state(s_checkin_btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(s_checkin_btn, lv_color_hex(0xd0d5dd), LV_STATE_DISABLED);
        if (label != NULL) {
            lv_label_set_text(label, "Checked today");
        }
    }
}

static void checkin_cb(lv_event_t *e)
{
    (void)e;

    if (!vela_pet_can_checkin()) {
        ui_show_toast("Already checked in today~");
        ui_task_refresh();
        return;
    }

    int days = vela_pet_checkin();

    if (days > 0) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Check-in %d days!", days);
        ui_show_toast(msg);

        const char *level_msg = vela_pet_get_levelup_message();
        if (level_msg != NULL && level_msg[0] != '\0') {
            ui_show_toast("Level up!");
        }
    } else {
        ui_show_toast("Already checked in today~");
    }

    ui_task_refresh();
}

static void add_task_card(Task *task, int index)
{
    const VelaPetTheme *theme = ui_get_current_theme();
    char line[128];
    lv_obj_t *card = ui_create_card(s_list, LV_PCT(96), 126);
    lv_obj_set_style_bg_color(card, theme->card_color, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);

    lv_obj_t *top = lv_obj_create(card);
    lv_obj_remove_style_all(top);
    lv_obj_set_size(top, LV_PCT(100), 34);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *icon = lv_obj_create(top);
    lv_obj_set_size(icon, 44, 30);
    lv_obj_set_style_radius(icon, 14, 0);
    lv_obj_set_style_bg_color(icon, theme->bg_grad_color, 0);
    lv_obj_set_style_border_width(icon, 1, 0);
    lv_obj_set_style_border_color(icon, theme->accent_color, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon_label = lv_label_create(icon);
    lv_label_set_text(icon_label, task_icon_for(index));
    ui_style_label(icon_label, theme->primary_color);
    lv_obj_center(icon_label);

    snprintf(line, sizeof(line), "%s", task->description[0] ? task->description : "Daily task");
    lv_obj_t *title = lv_label_create(top);
    lv_label_set_text(title, line);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, LV_PCT(78));
    ui_style_label(title, theme->text_color);

    snprintf(line, sizeof(line), "%d / %d    +%d EXP    %s",
             task->current_progress, task->target_value, task->exp_reward,
             ui_task_status_text(task->status));
    lv_obj_t *meta = lv_label_create(card);
    lv_label_set_text(meta, line);
    ui_style_label(meta, theme->text_color);

    lv_obj_t *bar = ui_create_exp_bar(card, task->current_progress, task->target_value);
    lv_obj_set_width(bar, LV_PCT(100));

    if (task->status == TASK_STATUS_COMPLETED) {
        ui_create_button(card, "Claim", claim_cb, (void *)(intptr_t)index);
    } else if (task->status == TASK_STATUS_REWARD_CLAIMED) {
        lv_obj_t *claimed = lv_label_create(card);
        lv_label_set_text(claimed, "Claimed");
        ui_style_label(claimed, lv_color_hex(0x98a2b3));
    }
}

void ui_task_create(void)
{
    g_ui_root = ui_create_page();
    ui_create_title(g_ui_root, "Today Tasks", 1);

    s_list = lv_obj_create(g_ui_root);
    lv_obj_set_size(s_list, LV_PCT(100), LV_PCT(70));
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 10, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);

    s_checkin_btn = ui_create_button(g_ui_root, "Check in", checkin_cb, NULL);
    ui_task_refresh();
}

void ui_task_refresh(void)
{
    if (s_list == NULL) return;
    lv_obj_clean(s_list);

    int count = 0;
    Task *tasks = vela_pet_get_tasks(&count);
    if (tasks == NULL || count <= 0) {
        lv_obj_t *empty = lv_label_create(s_list);
        lv_label_set_text(empty, "No task data");
        ui_style_label(empty, lv_color_hex(0x667085));
        refresh_checkin_button();
        return;
    }

    for (int i = 0; i < count; ++i) {
        add_task_card(&tasks[i], i);
    }

    refresh_checkin_button();
}
