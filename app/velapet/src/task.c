#include "task.h"

#include "json_utils.h"
#include "reward.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SECONDS_PER_DAY (24 * 60 * 60)

/* Reward for the daily 3000-step task.
 *
 * While the demo step source is enabled (VELAPET_DEMO_REWARD) the reward is
 * raised so the level-up path can actually be reached in one session on
 * hardware: the other tasks plus the check-in bonus only add up to 45 EXP,
 * and the social task still has no on-device trigger.  Host builds and
 * production builds keep the tuned value.
 */
#ifdef VELAPET_DEMO_REWARD
#  define VELAPET_STEPS_TASK_REWARD 60
#else
#  define VELAPET_STEPS_TASK_REWARD 20
#endif

static int same_day(time_t a, time_t b)
{
  struct tm ta;
  struct tm tb;

  if (a == 0 || b == 0)
    {
      return 0;
    }

  localtime_r(&a, &ta);
  localtime_r(&b, &tb);
  return ta.tm_year == tb.tm_year && ta.tm_yday == tb.tm_yday;
}

static int is_yesterday(time_t earlier, time_t now)
{
  struct tm tm_now;
  time_t start_today;

  if (earlier == 0 || now == 0)
    {
      return 0;
    }

  localtime_r(&now, &tm_now);
  tm_now.tm_hour = 0;
  tm_now.tm_min = 0;
  tm_now.tm_sec = 0;
  start_today = mktime(&tm_now);
  return earlier >= start_today - SECONDS_PER_DAY && earlier < start_today;
}

static time_t end_of_today(void)
{
  time_t now = time(NULL);
  struct tm tm_now;

  localtime_r(&now, &tm_now);
  tm_now.tm_hour = 23;
  tm_now.tm_min = 59;
  tm_now.tm_sec = 59;
  return mktime(&tm_now);
}

static void set_task(Task *task, TaskType type, const char *desc, int target,
                     int reward)
{
  if (task == NULL)
    {
      return;
    }

  memset(task, 0, sizeof(*task));
  task->type = type;
  snprintf(task->description, sizeof(task->description), "%s", desc);
  task->target_value = target;
  task->current_progress = 0;
  task->status = TASK_STATUS_NOT_STARTED;
  task->exp_reward = reward;
  task->deadline = end_of_today();
}

TaskLog *task_log_create_default(void)
{
  TaskLog *log = (TaskLog *)calloc(1, sizeof(TaskLog));
  if (log == NULL)
    {
      return NULL;
    }

  task_log_refresh_daily(log);
  return log;
}

void task_log_free(TaskLog *log)
{
  free(log);
}

TaskLog *task_log_load(const char *path)
{
  cJSON *root = json_load_from_file(path);
  if (root == NULL)
    {
      return task_log_create_default();
    }

  TaskLog *log = task_from_json(root);
  json_free(root);
  if (log == NULL)
    {
      return task_log_create_default();
    }

  if (log->task_count <= 0 || log->task_count > VELAPET_MAX_TASKS)
    {
      task_log_refresh_daily(log);
    }

  return log;
}

int task_log_save(TaskLog *log, const char *path)
{
  if (log == NULL || path == NULL)
    {
      return -1;
    }

  cJSON *root = task_to_json(log);
  if (root == NULL)
    {
      return -1;
    }

  int ret = json_save_to_file(path, root);
  json_free(root);
  return ret;
}

int task_log_refresh_daily(TaskLog *log)
{
  time_t now = time(NULL);

  if (log == NULL)
    {
      return -1;
    }

  /* A board without an RTC battery restarts its clock at boot, so "now" can
   * jump backwards.  Treat that as a new day rather than never refreshing.
   */

  if (now >= log->last_refresh_time && same_day(log->last_refresh_time, now))
    {
      return 0;
    }

  memset(log->tasks, 0, sizeof(log->tasks));
  log->task_count = 4;
  set_task(&log->tasks[0], TASK_TYPE_DAILY_STEPS, "Walk 3000 steps today",
           3000, VELAPET_STEPS_TASK_REWARD);
  set_task(&log->tasks[1], TASK_TYPE_CONTINUOUS_WALK,
           "Walk continuously for 10 minutes", 10, 30);
  set_task(&log->tasks[2], TASK_TYPE_CHECKIN, "Daily check-in", 1, 10);
  set_task(&log->tasks[3], TASK_TYPE_SOCIAL, "Tap with a friend", 1, 15);
  log->last_refresh_time = now;
  return 1;
}

int task_update_progress(TaskLog *log, TaskType type, int delta)
{
  int updated = 0;

  if (log == NULL || delta <= 0)
    {
      return -1;
    }

  for (int i = 0; i < log->task_count && i < VELAPET_MAX_TASKS; i++)
    {
      Task *task = &log->tasks[i];
      if (task->type != type || task->status == TASK_STATUS_REWARD_CLAIMED)
        {
          continue;
        }

      if (task->status == TASK_STATUS_NOT_STARTED)
        {
          task->status = TASK_STATUS_IN_PROGRESS;
        }

      task->current_progress += delta;
      if (task->current_progress > task->target_value)
        {
          task->current_progress = task->target_value;
        }

      task_check_completion(log, i);
      updated++;
    }

  return updated;
}

int task_check_completion(TaskLog *log, int task_index)
{
  Task *task = NULL;

  if (log == NULL || task_index < 0 || task_index >= log->task_count ||
      task_index >= VELAPET_MAX_TASKS)
    {
      return -1;
    }

  task = &log->tasks[task_index];
  if (task->status != TASK_STATUS_REWARD_CLAIMED &&
      task->current_progress >= task->target_value)
    {
      task->status = TASK_STATUS_COMPLETED;
      return 1;
    }

  return 0;
}

int task_claim_reward(TaskLog *log, int task_index, PetProfile *pet)
{
  Task *task = NULL;

  if (log == NULL || pet == NULL || task_index < 0 ||
      task_index >= log->task_count || task_index >= VELAPET_MAX_TASKS)
    {
      return -1;
    }

  task = &log->tasks[task_index];
  if (task->status != TASK_STATUS_COMPLETED)
    {
      return 0;
    }

  task->status = TASK_STATUS_REWARD_CLAIMED;
  pet_add_exp(pet, task->exp_reward);
  return task->exp_reward;
}

int task_can_checkin(TaskLog *log)
{
  time_t now = time(NULL);

  if (log == NULL)
    {
      return 0;
    }

  /* As above: a clock that jumped backwards must not lock the user out of
   * checking in. */

  if (now < log->last_checkin_time)
    {
      return 1;
    }

  return !same_day(log->last_checkin_time, now);
}

int task_checkin(TaskLog *log, PetProfile *pet)
{
  time_t now = time(NULL);

  if (log == NULL || pet == NULL)
    {
      return -1;
    }

  if (!task_can_checkin(log))
    {
      return 0;
    }

  if (is_yesterday(log->last_checkin_time, now))
    {
      log->consecutive_days++;
    }
  else
    {
      log->consecutive_days = 1;
    }

  log->last_checkin_time = now;
  pet->consecutive_days = log->consecutive_days;
  pet->last_active = now;
  task_update_progress(log, TASK_TYPE_CHECKIN, 1);

  if (log->consecutive_days == 3)
    {
      LevelReward rainbow = {
        .level = 0,
        .reward_type = "skin",
        .reward_name = "rainbow",
        .reward_data = "rainbow"
      };
      reward_apply(pet, &rainbow);
    }

  return log->consecutive_days;
}

int task_get_completed_count(TaskLog *log)
{
  int count = 0;

  if (log == NULL)
    {
      return 0;
    }

  for (int i = 0; i < log->task_count && i < VELAPET_MAX_TASKS; i++)
    {
      if (log->tasks[i].status == TASK_STATUS_COMPLETED ||
          log->tasks[i].status == TASK_STATUS_REWARD_CLAIMED)
        {
          count++;
        }
    }

  return count;
}

int task_all_completed(TaskLog *log)
{
  if (log == NULL || log->task_count <= 0)
    {
      return 0;
    }

  return task_get_completed_count(log) == log->task_count;
}
