#include "ui_internal.h"
#ifndef VELAPET_UI_USE_MOCK
#include <velapet_ai_bridge.h>
#endif
#include <stdio.h>
#include <string.h>

static lv_obj_t *s_reply;
static char s_reply_text[256] = "Hi, I am your VelaPet. Ready for today's mission?";
static int s_loading = 0;

#ifdef VELAPET_UI_USE_MOCK
static const char *mock_ai_reply(const char *type)
{
    if (strcmp(type, "task") == 0) {
        return "Finish one more task and I can level up!";
    }
    if (strcmp(type, "level") == 0) {
        return "You are getting closer to a new skin!";
    }
    return "Only a few steps left! I am cheering for you~";
}
#else
/* The AI transport is not implemented yet, so a request always fails.  Rather
 * than answer with an error, reply from the pet's own state: it is short and
 * still says something true.  See docs/project-handoff.md for why the AI
 * link is blocked.
 */

static void local_ai_reply(char *buf, size_t len, const char *type)
{
    PetProfile *pet = vela_pet_get_profile();
    int count = 0;
    Task *tasks = vela_pet_get_tasks(&count);
    int daily = vela_pet_get_daily_progress();
    int done = 0;

    for (int i = 0; tasks != NULL && i < count; i++) {
        if (tasks[i].status == TASK_STATUS_COMPLETED ||
            tasks[i].status == TASK_STATUS_REWARD_CLAIMED) {
            done++;
        }
    }

    if (strcmp(type, "level") == 0) {
        if (pet != NULL) {
            snprintf(buf, len, "Lv.%d, %d/%d EXP to the next skin.",
                     pet->level, pet->exp, pet->exp_to_next);
        } else {
            snprintf(buf, len, "Let's earn some EXP together!");
        }
        return;
    }

    if (strcmp(type, "task") == 0) {
        snprintf(buf, len, "%d of %d tasks done. %s", done, count,
                 done > 0 && done >= count ? "All clear!" : "One more?");
        return;
    }

    snprintf(buf, len, "%d%% of today's goals. %s", daily,
             daily >= 100 ? "You did it!" : "A few more steps!");
}
#endif

static void quick_cb(lv_event_t *e)
{
    const char *type = (const char *)lv_event_get_user_data(e);
#ifdef VELAPET_UI_USE_MOCK
    velapet_ui_chat_set_loading(1);
    velapet_ui_chat_set_reply(mock_ai_reply(type));
    velapet_ui_chat_set_loading(0);
    ui_show_toast("Vela replied!");
#else
    if (velapet_ai_request_reply(type) != 0) {
        char reply[128];

        local_ai_reply(reply, sizeof(reply), type);
        velapet_ui_chat_set_loading(0);
        velapet_ui_chat_set_reply(reply);
        ui_show_toast("AI offline, replying locally");
    }
#endif
}

void velapet_ui_chat_set_reply(const char *reply)
{
    if (reply == NULL) return;
    snprintf(s_reply_text, sizeof(s_reply_text), "%s", reply);
    if (g_ui_page == UI_PAGE_CHAT && s_reply != NULL) ui_chat_refresh();
}

void velapet_ui_chat_set_loading(int loading)
{
    s_loading = loading;
    if (g_ui_page == UI_PAGE_CHAT && s_reply != NULL) ui_chat_refresh();
}

void velapet_ui_chat_set_error(const char *error_msg)
{
    velapet_ui_chat_set_reply(error_msg ? error_msg : "AI is offline. Local reply is used.");
}

void ui_chat_create(void)
{
    const VelaPetTheme *theme = ui_get_current_theme();
    g_ui_root = ui_create_page();
    ui_create_title(g_ui_root, "Pet Chat", 1);

    lv_obj_t *avatar = ui_create_card(g_ui_root, 128, 104);
    lv_obj_set_style_bg_color(avatar, theme->bg_grad_color, 0);
    lv_obj_set_flex_flow(avatar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(avatar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_create_pet_avatar(avatar, 116, 88, "u");

    lv_obj_t *bubble_wrap = lv_obj_create(g_ui_root);
    lv_obj_remove_style_all(bubble_wrap);
    lv_obj_set_size(bubble_wrap, LV_PCT(94), 136);
    lv_obj_clear_flag(bubble_wrap, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bubble = ui_create_card(bubble_wrap, LV_PCT(96), 116);
    lv_obj_set_style_bg_color(bubble, theme->card_color, 0);
    lv_obj_align(bubble, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *tail = lv_obj_create(bubble_wrap);
    lv_obj_remove_style_all(tail);
    lv_obj_set_size(tail, 18, 18);
    lv_obj_set_style_radius(tail, 4, 0);
    lv_obj_set_style_bg_color(tail, theme->card_color, 0);
    lv_obj_set_style_bg_opa(tail, LV_OPA_80, 0);
    lv_obj_align_to(tail, bubble, LV_ALIGN_BOTTOM_LEFT, 34, -8);

    s_reply = lv_label_create(bubble);
    lv_obj_set_width(s_reply, LV_PCT(100));
    lv_label_set_long_mode(s_reply, LV_LABEL_LONG_WRAP);
    ui_style_label(s_reply, theme->text_color);

    ui_create_button(g_ui_root, "Cheer me", quick_cb, "cheer");
    ui_create_button(g_ui_root, "Today task", quick_cb, "task");
    ui_create_button(g_ui_root, "Level up?", quick_cb, "level");

    ui_chat_refresh();
}

void ui_chat_refresh(void)
{
    if (g_ui_page != UI_PAGE_CHAT || s_reply == NULL) return;
    lv_label_set_text(s_reply, s_loading ? "Vela is thinking..." : s_reply_text);
}

void ui_chat_on_destroy(void)
{
    s_reply = NULL;
}
