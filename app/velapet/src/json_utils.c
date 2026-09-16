#include "json_utils.h"

#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const cJSON *object_get(const cJSON *root, const char *name)
{
  return cJSON_GetObjectItemCaseSensitive((cJSON *)root, name);
}

static int json_int(const cJSON *root, const char *name, int fallback)
{
  const cJSON *item = object_get(root, name);
  return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static time_t json_time(const cJSON *root, const char *name, time_t fallback)
{
  const cJSON *item = object_get(root, name);
  return cJSON_IsNumber(item) ? (time_t)item->valuedouble : fallback;
}

static void json_string(const cJSON *root, const char *name, char *dst,
                        size_t dst_len, const char *fallback)
{
  const cJSON *item = object_get(root, name);
  const char *value = cJSON_IsString(item) ? item->valuestring : fallback;

  if (dst != NULL && dst_len > 0)
    {
      snprintf(dst, dst_len, "%s", value == NULL ? "" : value);
    }
}

cJSON *json_load_from_file(const char *path)
{
  char *buffer = NULL;
  cJSON *root = NULL;

  if (storage_read_all(path, &buffer) < 0 || buffer == NULL)
    {
      return NULL;
    }

  root = cJSON_Parse(buffer);
  free(buffer);
  return root;
}

int json_save_to_file(const char *path, cJSON *root)
{
  char *text = NULL;
  int ret = -1;

  if (path == NULL || root == NULL)
    {
      return -1;
    }

  text = cJSON_PrintUnformatted(root);
  if (text == NULL)
    {
      return -1;
    }

  ret = storage_write_all(path, text, strlen(text));
  cJSON_free(text);
  return ret;
}

void json_free(cJSON *root)
{
  if (root != NULL)
    {
      cJSON_Delete(root);
    }
}

PetProfile *pet_from_json(cJSON *root)
{
  PetProfile *pet = NULL;
  const cJSON *skins = NULL;
  const cJSON *skin = NULL;
  const char *skin_id = NULL;

  if (!cJSON_IsObject(root))
    {
      return NULL;
    }

  pet = pet_create_default();
  if (pet == NULL)
    {
      return NULL;
    }

  json_string(root, "name", pet->name, sizeof(pet->name), pet->name);
  pet->level = json_int(root, "level", pet->level);
  pet->exp = json_int(root, "exp", pet->exp);
  pet->exp_to_next = json_int(root, "exp_to_next", pet->exp_to_next);
  json_string(root, "current_skin", pet->current_skin,
              sizeof(pet->current_skin), pet->current_skin);
  skin_id = pet_normalize_skin_id(pet->current_skin);
  snprintf(pet->current_skin, sizeof(pet->current_skin), "%s",
           skin_id == NULL ? VELAPET_DEFAULT_SKIN : skin_id);

  json_string(root, "personality", pet->personality,
              sizeof(pet->personality), pet->personality);
  pet->total_steps = json_int(root, "total_steps", pet->total_steps);
  pet->consecutive_days = json_int(root, "consecutive_days",
                                   pet->consecutive_days);
  pet->last_active = json_time(root, "last_active", pet->last_active);
  pet->last_task_refresh = json_time(root, "last_task_refresh",
                                     pet->last_task_refresh);

  skins = object_get(root, "unlocked_skins");
  if (cJSON_IsArray(skins))
    {
      memset(pet->unlocked_skins, 0, sizeof(pet->unlocked_skins));
      pet->unlocked_skin_count = 0;

      cJSON_ArrayForEach(skin, skins)
        {
          if (cJSON_IsString(skin) && skin->valuestring != NULL &&
              skin->valuestring[0] != '\0')
            {
              pet_unlock_skin(pet, skin->valuestring);
            }
        }
    }

  pet_unlock_skin(pet, VELAPET_DEFAULT_SKIN);
  pet_unlock_skin(pet, pet->current_skin);
  return pet;
}

cJSON *pet_to_json(const PetProfile *pet)
{
  cJSON *root = NULL;
  cJSON *skins = NULL;

  if (pet == NULL)
    {
      return NULL;
    }

  root = cJSON_CreateObject();
  if (root == NULL)
    {
      return NULL;
    }

  cJSON_AddStringToObject(root, "name", pet->name);
  cJSON_AddNumberToObject(root, "level", pet->level);
  cJSON_AddNumberToObject(root, "exp", pet->exp);
  cJSON_AddNumberToObject(root, "exp_to_next", pet->exp_to_next);
  cJSON_AddStringToObject(root, "current_skin", pet->current_skin);
  cJSON_AddStringToObject(root, "personality", pet->personality);
  cJSON_AddNumberToObject(root, "total_steps", pet->total_steps);
  cJSON_AddNumberToObject(root, "consecutive_days", pet->consecutive_days);
  cJSON_AddNumberToObject(root, "last_active", (double)pet->last_active);
  cJSON_AddNumberToObject(root, "last_task_refresh",
                          (double)pet->last_task_refresh);

  cJSON_AddNumberToObject(root, "unlocked_skin_count",
                          pet->unlocked_skin_count);
  skins = cJSON_AddArrayToObject(root, "unlocked_skins");
  if (skins == NULL)
    {
      cJSON_Delete(root);
      return NULL;
    }

  for (int i = 0; i < pet->unlocked_skin_count && i < VELAPET_MAX_SKINS; i++)
    {
      cJSON *skin = cJSON_CreateString(pet->unlocked_skins[i]);
      if (skin == NULL)
        {
          cJSON_Delete(root);
          return NULL;
        }

      cJSON_AddItemToArray(skins, skin);
    }

  return root;
}

TaskLog *task_from_json(cJSON *root)
{
  TaskLog *log = NULL;
  const cJSON *tasks = NULL;
  const cJSON *item = NULL;
  int index = 0;

  if (!cJSON_IsObject(root))
    {
      return NULL;
    }

  log = (TaskLog *)calloc(1, sizeof(TaskLog));
  if (log == NULL)
    {
      return NULL;
    }

  log->consecutive_days = json_int(root, "consecutive_days", 0);
  log->last_checkin_time = json_time(root, "last_checkin_time", 0);
  log->last_refresh_time = json_time(root, "last_refresh_time", 0);

  tasks = object_get(root, "tasks");
  if (!cJSON_IsArray(tasks))
    {
      task_log_refresh_daily(log);
      return log;
    }

  cJSON_ArrayForEach(item, tasks)
    {
      Task *task = NULL;

      if (index >= VELAPET_MAX_TASKS || !cJSON_IsObject(item))
        {
          continue;
        }

      task = &log->tasks[index];
      task->type = (TaskType)json_int(item, "type", TASK_TYPE_DAILY_STEPS);
      json_string(item, "description", task->description,
                  sizeof(task->description), "");
      task->target_value = json_int(item, "target_value", 0);
      task->current_progress = json_int(item, "current_progress", 0);
      task->status = (TaskStatus)json_int(item, "status",
                                          TASK_STATUS_NOT_STARTED);
      task->exp_reward = json_int(item, "exp_reward", 0);
      task->deadline = json_time(item, "deadline", 0);
      index++;
    }

  log->task_count = index;
  return log;
}

cJSON *task_to_json(const TaskLog *log)
{
  cJSON *root = NULL;
  cJSON *tasks = NULL;

  if (log == NULL)
    {
      return NULL;
    }

  root = cJSON_CreateObject();
  if (root == NULL)
    {
      return NULL;
    }

  cJSON_AddNumberToObject(root, "consecutive_days", log->consecutive_days);
  cJSON_AddNumberToObject(root, "last_checkin_time",
                          (double)log->last_checkin_time);
  cJSON_AddNumberToObject(root, "last_refresh_time",
                          (double)log->last_refresh_time);

  tasks = cJSON_AddArrayToObject(root, "tasks");
  if (tasks == NULL)
    {
      cJSON_Delete(root);
      return NULL;
    }

  for (int i = 0; i < log->task_count && i < VELAPET_MAX_TASKS; i++)
    {
      const Task *task = &log->tasks[i];
      cJSON *item = cJSON_CreateObject();
      if (item == NULL)
        {
          cJSON_Delete(root);
          return NULL;
        }

      cJSON_AddNumberToObject(item, "type", task->type);
      cJSON_AddStringToObject(item, "description", task->description);
      cJSON_AddNumberToObject(item, "target_value", task->target_value);
      cJSON_AddNumberToObject(item, "current_progress",
                              task->current_progress);
      cJSON_AddNumberToObject(item, "status", task->status);
      cJSON_AddNumberToObject(item, "exp_reward", task->exp_reward);
      cJSON_AddNumberToObject(item, "deadline", (double)task->deadline);
      cJSON_AddItemToArray(tasks, item);
    }

  return root;
}
