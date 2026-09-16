#ifndef VELAPET_TASK_H
#define VELAPET_TASK_H

#include "pet.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VELAPET_MAX_TASKS 8
#define VELAPET_TASK_DESC_LEN 128

typedef enum
{
  TASK_STATUS_NOT_STARTED = 0,
  TASK_STATUS_IN_PROGRESS,
  TASK_STATUS_COMPLETED,
  TASK_STATUS_REWARD_CLAIMED
} TaskStatus;

typedef enum
{
  TASK_TYPE_DAILY_STEPS = 0,
  TASK_TYPE_CONTINUOUS_WALK,
  TASK_TYPE_CHECKIN,
  TASK_TYPE_SOCIAL
} TaskType;

typedef struct
{
  TaskType type;
  char description[VELAPET_TASK_DESC_LEN];
  int target_value;
  int current_progress;
  TaskStatus status;
  int exp_reward;
  time_t deadline;
} Task;

typedef struct
{
  Task tasks[VELAPET_MAX_TASKS];
  int task_count;
  int consecutive_days;
  time_t last_checkin_time;
  time_t last_refresh_time;
} TaskLog;

TaskLog *task_log_create_default(void);
void task_log_free(TaskLog *log);
TaskLog *task_log_load(const char *path);
int task_log_save(TaskLog *log, const char *path);
int task_log_refresh_daily(TaskLog *log);
int task_update_progress(TaskLog *log, TaskType type, int delta);
int task_check_completion(TaskLog *log, int task_index);
int task_claim_reward(TaskLog *log, int task_index, PetProfile *pet);
int task_can_checkin(TaskLog *log);
int task_checkin(TaskLog *log, PetProfile *pet);
int task_get_completed_count(TaskLog *log);
int task_all_completed(TaskLog *log);

#ifdef __cplusplus
}
#endif

#endif
