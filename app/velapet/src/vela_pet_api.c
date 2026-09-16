#include "vela_pet_api.h"

#include "json_utils.h"
#include "storage.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;
static PetProfile *g_pet;
static TaskLog *g_task;
static char g_levelup_message[128];
static char g_daily_summary[192];
static char g_current_skin_id[VELAPET_SKIN_LEN];
static PetProfile g_pet_snapshot;
static Task g_task_snapshot[VELAPET_MAX_TASKS];

static int daily_progress_locked(void)
{
  int progress_sum = 0;
  int max_sum = 0;

  if (g_task == NULL)
    {
      return 0;
    }

  for (int i = 0; i < g_task->task_count && i < VELAPET_MAX_TASKS; i++)
    {
      progress_sum += g_task->tasks[i].current_progress;
      max_sum += g_task->tasks[i].target_value;
    }

  return max_sum > 0 ? progress_sum * 100 / max_sum : 0;
}

static void set_levelup_message(int levels_gained)
{
  if (levels_gained > 0 && g_pet != NULL)
    {
      snprintf(g_levelup_message, sizeof(g_levelup_message),
               "Level %d reached. Title: %s", g_pet->level,
               pet_get_title(g_pet->level));
    }
}

static int add_exp_locked(int exp)
{
  int levels = pet_add_exp(g_pet, exp);
  set_levelup_message(levels);
  return levels;
}

int vela_pet_init(void)
{
  int ret = 0;

  pthread_mutex_lock(&g_state_lock);
  if (g_pet != NULL && g_task != NULL)
    {
      pthread_mutex_unlock(&g_state_lock);
      return 0;
    }

  ret = vela_pet_restore_state(&g_pet, &g_task);
  snprintf(g_levelup_message, sizeof(g_levelup_message), "%s", "");
  pthread_mutex_unlock(&g_state_lock);
  return ret;
}

int vela_pet_deinit(void)
{
  pthread_mutex_lock(&g_state_lock);
  if (g_pet != NULL && g_task != NULL)
    {
      vela_pet_save_state(g_pet, g_task);
    }

  pet_free(g_pet);
  task_log_free(g_task);
  g_pet = NULL;
  g_task = NULL;
  pthread_mutex_unlock(&g_state_lock);
  return 0;
}

PetProfile *vela_pet_get_profile(void)
{
  pthread_mutex_lock(&g_state_lock);
  if (g_pet != NULL)
    {
      g_pet_snapshot = *g_pet;
      pthread_mutex_unlock(&g_state_lock);
      return &g_pet_snapshot;
    }

  pthread_mutex_unlock(&g_state_lock);
  return NULL;
}

Task *vela_pet_get_tasks(int *count)
{
  pthread_mutex_lock(&g_state_lock);
  if (g_task != NULL)
    {
      int n = g_task->task_count;
      if (n > VELAPET_MAX_TASKS)
        {
          n = VELAPET_MAX_TASKS;
        }

      memcpy(g_task_snapshot, g_task->tasks, sizeof(Task) * n);
      if (count != NULL)
        {
          *count = n;
        }

      pthread_mutex_unlock(&g_state_lock);
      return g_task_snapshot;
    }

  if (count != NULL)
    {
      *count = 0;
    }

  pthread_mutex_unlock(&g_state_lock);
  return NULL;
}

int vela_pet_get_daily_progress(void)
{
  int progress = 0;

  pthread_mutex_lock(&g_state_lock);
  progress = daily_progress_locked();
  pthread_mutex_unlock(&g_state_lock);
  return progress;
}

int vela_pet_claim_task(int task_index)
{
  int exp = 0;
  int old_level = 0;

  pthread_mutex_lock(&g_state_lock);
  if (g_pet == NULL || g_task == NULL)
    {
      pthread_mutex_unlock(&g_state_lock);
      return -1;
    }

  old_level = g_pet->level;
  exp = task_claim_reward(g_task, task_index, g_pet);
  if (exp > 0)
    {
      set_levelup_message(g_pet->level - old_level);
      vela_pet_save_state(g_pet, g_task);
    }

  pthread_mutex_unlock(&g_state_lock);
  return exp;
}

int vela_pet_can_checkin(void)
{
  int can_checkin = 0;

  pthread_mutex_lock(&g_state_lock);
  can_checkin = task_can_checkin(g_task);
  pthread_mutex_unlock(&g_state_lock);
  return can_checkin;
}

int vela_pet_checkin(void)
{
  int days = 0;

  pthread_mutex_lock(&g_state_lock);
  if (g_pet == NULL || g_task == NULL)
    {
      pthread_mutex_unlock(&g_state_lock);
      return -1;
    }

  days = task_checkin(g_task, g_pet);
  if (days > 0)
    {
      add_exp_locked(5);
      vela_pet_save_state(g_pet, g_task);
    }

  pthread_mutex_unlock(&g_state_lock);
  return days;
}

const char *vela_pet_get_current_skin_id(void)
{
  pthread_mutex_lock(&g_state_lock);
  if (g_pet == NULL)
    {
      g_current_skin_id[0] = '\0';
    }
  else
    {
      snprintf(g_current_skin_id, sizeof(g_current_skin_id), "%s",
               g_pet->current_skin);
    }

  pthread_mutex_unlock(&g_state_lock);
  return g_current_skin_id;
}

const char *vela_pet_get_current_skin_asset_path(void)
{
  const char *skin_id = vela_pet_get_current_skin_id();
  return vela_pet_get_skin_asset_path(skin_id);
}

const char *vela_pet_get_skin_asset_path(const char *skin_id)
{
  return pet_get_skin_asset_path(skin_id);
}

int vela_pet_is_skin_unlocked(const char *skin_id)
{
  int unlocked = 0;

  pthread_mutex_lock(&g_state_lock);
  unlocked = pet_is_skin_unlocked(g_pet, skin_id);
  pthread_mutex_unlock(&g_state_lock);
  return unlocked;
}

int vela_pet_switch_skin(const char *skin_name)
{
  int ret = 0;

  pthread_mutex_lock(&g_state_lock);
  ret = pet_change_skin(g_pet, skin_name);
  if (ret == 0)
    {
      vela_pet_save_state(g_pet, g_task);
    }

  pthread_mutex_unlock(&g_state_lock);
  return ret;
}

const char *vela_pet_get_levelup_message(void)
{
  return g_levelup_message;
}

char *vela_pet_get_ai_context(void)
{
  cJSON *root = NULL;
  cJSON *tasks = NULL;
  char *text = NULL;
  const char *skin_path = NULL;

  pthread_mutex_lock(&g_state_lock);
  if (g_pet == NULL || g_task == NULL)
    {
      pthread_mutex_unlock(&g_state_lock);
      return NULL;
    }

  root = cJSON_CreateObject();
  if (root == NULL)
    {
      pthread_mutex_unlock(&g_state_lock);
      return NULL;
    }

  cJSON_AddNumberToObject(root, "level", g_pet->level);
  cJSON_AddNumberToObject(root, "exp", g_pet->exp);
  cJSON_AddNumberToObject(root, "exp_to_next", g_pet->exp_to_next);
  cJSON_AddStringToObject(root, "pet_name", g_pet->name);
  cJSON_AddStringToObject(root, "title", pet_get_title(g_pet->level));
  cJSON_AddStringToObject(root, "skin", g_pet->current_skin);
  cJSON_AddStringToObject(root, "current_skin", g_pet->current_skin);
  skin_path = pet_get_skin_asset_path(g_pet->current_skin);
  if (skin_path == NULL)
    {
      skin_path = pet_get_skin_asset_path(VELAPET_DEFAULT_SKIN);
    }

  cJSON_AddStringToObject(root, "current_skin_asset_path", skin_path);
  cJSON_AddNumberToObject(root, "total_steps", g_pet->total_steps);
  cJSON_AddNumberToObject(root, "consecutive_days", g_pet->consecutive_days);
  cJSON_AddNumberToObject(root, "daily_progress", daily_progress_locked());

  {
    cJSON *skins = cJSON_AddArrayToObject(root, "unlocked_skins");
    if (skins != NULL)
      {
        for (int i = 0; i < g_pet->unlocked_skin_count &&
             i < VELAPET_MAX_SKINS; i++)
          {
            cJSON *skin = cJSON_CreateString(g_pet->unlocked_skins[i]);
            if (skin != NULL)
              {
                cJSON_AddItemToArray(skins, skin);
              }
          }
      }
  }

  tasks = cJSON_AddArrayToObject(root, "tasks");
  if (tasks != NULL)
    {
      for (int i = 0; i < g_task->task_count && i < VELAPET_MAX_TASKS; i++)
        {
          cJSON *task = cJSON_CreateObject();
          if (task == NULL)
            {
              continue;
            }

          cJSON_AddStringToObject(task, "description",
                                  g_task->tasks[i].description);
          cJSON_AddNumberToObject(task, "target",
                                  g_task->tasks[i].target_value);
          cJSON_AddNumberToObject(task, "progress",
                                  g_task->tasks[i].current_progress);
          cJSON_AddNumberToObject(task, "status", g_task->tasks[i].status);
          cJSON_AddItemToArray(tasks, task);
        }
    }

  text = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  pthread_mutex_unlock(&g_state_lock);
  return text;
}

void vela_pet_free_ai_context(char *ptr)
{
  if (ptr != NULL)
    {
      cJSON_free(ptr);
    }
}

int vela_pet_on_ai_encouragement(void)
{
  int levels = 0;

  pthread_mutex_lock(&g_state_lock);
  if (g_pet == NULL)
    {
      pthread_mutex_unlock(&g_state_lock);
      return -1;
    }

  levels = add_exp_locked(3);
  vela_pet_save_state(g_pet, g_task);
  pthread_mutex_unlock(&g_state_lock);
  return levels;
}

const char *vela_pet_get_daily_summary(void)
{
  pthread_mutex_lock(&g_state_lock);
  if (g_pet == NULL || g_task == NULL)
    {
      snprintf(g_daily_summary, sizeof(g_daily_summary),
               "VelaPet is not initialized.");
    }
  else
    {
      snprintf(g_daily_summary, sizeof(g_daily_summary),
               "Today progress %d%%, completed %d/%d tasks, total steps %d.",
               daily_progress_locked(), task_get_completed_count(g_task),
               g_task->task_count, g_pet->total_steps);
    }

  pthread_mutex_unlock(&g_state_lock);
  return g_daily_summary;
}

int vela_pet_update_steps(int steps)
{
  int ret = 0;

  pthread_mutex_lock(&g_state_lock);
  if (g_pet == NULL || g_task == NULL || steps <= 0)
    {
      pthread_mutex_unlock(&g_state_lock);
      return -1;
    }

  g_pet->total_steps += steps;
  g_pet->last_active = time(NULL);
  ret = task_update_progress(g_task, TASK_TYPE_DAILY_STEPS, steps);
  vela_pet_save_state(g_pet, g_task);
  pthread_mutex_unlock(&g_state_lock);
  return ret;
}

int vela_pet_update_exercise_time(int minutes)
{
  int ret = 0;

  pthread_mutex_lock(&g_state_lock);
  if (g_task == NULL || minutes <= 0)
    {
      pthread_mutex_unlock(&g_state_lock);
      return -1;
    }

  ret = task_update_progress(g_task, TASK_TYPE_CONTINUOUS_WALK, minutes);
  vela_pet_save_state(g_pet, g_task);
  pthread_mutex_unlock(&g_state_lock);
  return ret;
}

int vela_pet_on_social_interaction(void)
{
  int ret = 0;

  pthread_mutex_lock(&g_state_lock);
  if (g_pet == NULL || g_task == NULL)
    {
      pthread_mutex_unlock(&g_state_lock);
      return -1;
    }

  ret = task_update_progress(g_task, TASK_TYPE_SOCIAL, 1);
  add_exp_locked(15);
  vela_pet_save_state(g_pet, g_task);
  pthread_mutex_unlock(&g_state_lock);
  return ret;
}

int vela_pet_on_sit_reminder(void)
{
  pthread_mutex_lock(&g_state_lock);
  if (g_pet != NULL)
    {
      g_pet->last_active = time(NULL);
    }

  pthread_mutex_unlock(&g_state_lock);
  return 0;
}

int vela_pet_tick(void)
{
  pthread_mutex_lock(&g_state_lock);
  if (g_pet == NULL || g_task == NULL)
    {
      pthread_mutex_unlock(&g_state_lock);
      return -1;
    }

  task_log_refresh_daily(g_task);
  g_pet->last_task_refresh = g_task->last_refresh_time;
  vela_pet_save_state(g_pet, g_task);
  pthread_mutex_unlock(&g_state_lock);
  return 0;
}
