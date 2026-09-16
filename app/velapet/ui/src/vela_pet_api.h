#ifndef VELAPET_UI_MOCK_API_H
#define VELAPET_UI_MOCK_API_H

#ifndef VELAPET_UI_USE_MOCK
#error "ui/src/vela_pet_api.h is mock-only; add the apps VelaPet include directory for a production build"
#endif

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TASK_STATUS_NOT_STARTED = 0,
    TASK_STATUS_IN_PROGRESS,
    TASK_STATUS_COMPLETED,
    TASK_STATUS_REWARD_CLAIMED
} TaskStatus;

typedef enum {
    TASK_TYPE_DAILY_STEPS = 0,
    TASK_TYPE_CONTINUOUS_WALK,
    TASK_TYPE_CHECKIN,
    TASK_TYPE_SOCIAL
} TaskType;

#define VELAPET_NAME_LEN 32
#define VELAPET_SKIN_LEN 64
#define VELAPET_PERSONALITY_LEN 32
#define VELAPET_MAX_SKINS 8
#define VELAPET_TASK_DESC_LEN 128

typedef struct {
    TaskType type;
    char description[VELAPET_TASK_DESC_LEN];
    int target_value;
    int current_progress;
    TaskStatus status;
    int exp_reward;
    time_t deadline;
} Task;

typedef struct {
    char name[VELAPET_NAME_LEN];
    int level;
    int exp;
    int exp_to_next;
    char current_skin[VELAPET_SKIN_LEN];
    char personality[VELAPET_PERSONALITY_LEN];
    int total_steps;
    int consecutive_days;
    time_t last_active;
    time_t last_task_refresh;
    char unlocked_skins[VELAPET_MAX_SKINS][VELAPET_SKIN_LEN];
    int unlocked_skin_count;
} PetProfile;

int vela_pet_init(void);
PetProfile *vela_pet_get_profile(void);
Task *vela_pet_get_tasks(int *count);
int vela_pet_get_daily_progress(void);
int vela_pet_claim_task(int task_index);
int vela_pet_can_checkin(void);
int vela_pet_checkin(void);
int vela_pet_switch_skin(const char *skin_name);
const char *vela_pet_get_levelup_message(void);

#ifdef __cplusplus
}
#endif

#endif
