#ifdef VELAPET_UI_USE_MOCK

#include "ui_internal.h"
#include <stdio.h>
#include <string.h>

static int s_checked_in_today = 0;

static PetProfile s_pet = {
    .name = "Vela",
    .level = 1,
    .exp = 60,
    .exp_to_next = 100,
    .current_skin = "default",
    .total_steps = 2400,
    .consecutive_days = 2,
    .unlocked_skins = {"default", "sport_blue", "flame_red", "star_purple", "rainbow"},
    .unlocked_skin_count = 5,
};

static Task s_tasks[] = {
    {TASK_TYPE_DAILY_STEPS, "Walk 3000 steps today", 3000, 2400, TASK_STATUS_IN_PROGRESS, 20, 0},
    {TASK_TYPE_CONTINUOUS_WALK, "Walk continuously for 10 minutes", 10, 10, TASK_STATUS_COMPLETED, 30, 0},
    {TASK_TYPE_CHECKIN, "Daily check-in", 1, 0, TASK_STATUS_NOT_STARTED, 10, 0},
};

int vela_pet_init(void)
{
    return 0;
}

PetProfile *vela_pet_get_profile(void)
{
    return &s_pet;
}

Task *vela_pet_get_tasks(int *count)
{
    if (count != NULL) {
        *count = (int)(sizeof(s_tasks) / sizeof(s_tasks[0]));
    }
    return s_tasks;
}

int vela_pet_get_daily_progress(void)
{
    return 67;
}

int vela_pet_claim_task(int task_index)
{
    int count = 0;
    Task *tasks = vela_pet_get_tasks(&count);
    if (task_index < 0 || task_index >= count) {
        return 0;
    }
    if (tasks[task_index].status != TASK_STATUS_COMPLETED) {
        return 0;
    }

    tasks[task_index].status = TASK_STATUS_REWARD_CLAIMED;
    s_pet.exp += tasks[task_index].exp_reward;
    if (s_pet.exp >= s_pet.exp_to_next) {
        s_pet.level += 1;
        s_pet.exp -= s_pet.exp_to_next;
    }
    return tasks[task_index].exp_reward;
}

int vela_pet_can_checkin(void)
{
    return s_checked_in_today ? 0 : 1;
}

int vela_pet_checkin(void)
{
    if (!vela_pet_can_checkin()) {
        return 0;
    }

    s_checked_in_today = 1;
    s_pet.consecutive_days += 1;
    s_tasks[2].current_progress = 1;
    s_tasks[2].status = TASK_STATUS_COMPLETED;
    return s_pet.consecutive_days;
}

int vela_pet_switch_skin(const char *skin_name)
{
    for (int i = 0; i < s_pet.unlocked_skin_count; ++i) {
        if (strcmp(s_pet.unlocked_skins[i], skin_name) == 0) {
            snprintf(s_pet.current_skin, sizeof(s_pet.current_skin), "%s", s_pet.unlocked_skins[i]);
            return 0;
        }
    }
    return -1;
}

const char *vela_pet_get_levelup_message(void)
{
    if (s_pet.level > 1) {
        return "Level up! New skin progress unlocked.";
    }
    return "";
}

#endif
