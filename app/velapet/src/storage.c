#include "storage.h"

#include "reward.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int mkdir_if_missing(const char *path)
{
  struct stat st;

#ifdef _WIN32
  if (mkdir(path) == 0 || errno == EEXIST)
#else
  if (mkdir(path, 0777) == 0 || errno == EEXIST)
#endif
    {
      if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        {
          return 0;
        }
    }

  return -1;
}

int storage_ensure_dir(const char *path)
{
  char temp[160];
  size_t len = 0;

  if (path == NULL || path[0] == '\0')
    {
      return -1;
    }

  snprintf(temp, sizeof(temp), "%s", path);
  len = strlen(temp);
  if (len == 0)
    {
      return -1;
    }

  for (size_t i = 1; i < len; i++)
    {
      if (temp[i] == '/')
        {
          temp[i] = '\0';
          if (strlen(temp) > 0 && mkdir_if_missing(temp) < 0)
            {
              return -1;
            }

          temp[i] = '/';
        }
    }

  return mkdir_if_missing(temp);
}

int storage_read_all(const char *path, char **out_buffer)
{
  FILE *fp = NULL;
  long size = 0;
  char *buffer = NULL;
  size_t read_len = 0;

  if (path == NULL || out_buffer == NULL)
    {
      return -1;
    }

  *out_buffer = NULL;
  fp = fopen(path, "rb");
  if (fp == NULL)
    {
      return -1;
    }

  if (fseek(fp, 0, SEEK_END) != 0)
    {
      fclose(fp);
      return -1;
    }

  size = ftell(fp);
  if (size < 0)
    {
      fclose(fp);
      return -1;
    }

  rewind(fp);
  buffer = (char *)calloc((size_t)size + 1, 1);
  if (buffer == NULL)
    {
      fclose(fp);
      return -1;
    }

  read_len = fread(buffer, 1, (size_t)size, fp);
  fclose(fp);
  if (read_len != (size_t)size)
    {
      free(buffer);
      return -1;
    }

  *out_buffer = buffer;
  return 0;
}

int storage_write_all(const char *path, const char *data, size_t len)
{
  FILE *fp = NULL;
  const char *last_slash = NULL;
  char dir[160];
  char temp[192];
  size_t written = 0;

  if (path == NULL || data == NULL)
    {
      return -1;
    }

  last_slash = strrchr(path, '/');
  if (last_slash != NULL && last_slash != path)
    {
      size_t dir_len = (size_t)(last_slash - path);
      if (dir_len >= sizeof(dir))
        {
          return -1;
        }

      memcpy(dir, path, dir_len);
      dir[dir_len] = '\0';
      if (storage_ensure_dir(dir) < 0)
        {
          return -1;
        }
    }

  /* Write to a temporary file and rename it over the target.  A reset or
   * power loss in the middle of a save then leaves the previous state file
   * intact instead of a half-written one.
   */

  if (snprintf(temp, sizeof(temp), "%s.tmp", path) >= (int)sizeof(temp))
    {
      return -1;
    }

  fp = fopen(temp, "wb");
  if (fp == NULL)
    {
      return -1;
    }

  written = fwrite(data, 1, len, fp);
  if (written != len || fflush(fp) != 0)
    {
      fclose(fp);
      unlink(temp);
      return -1;
    }

#ifndef _WIN32
  /* Best effort: the rename below is what guarantees the old file survives. */

  fsync(fileno(fp));
#endif

  if (fclose(fp) != 0)
    {
      unlink(temp);
      return -1;
    }

  if (rename(temp, path) != 0)
    {
      unlink(temp);
      return -1;
    }

  return 0;
}

int vela_pet_restore_state(PetProfile **pet, TaskLog **task)
{
  if (pet == NULL || task == NULL)
    {
      return -1;
    }

  *pet = NULL;
  *task = NULL;
  if (storage_ensure_dir(VELAPET_MEMORY_DIR) < 0)
    {
      printf("ERROR: VelaPet data path is unavailable: %s. "
             "Check that /data is mounted.\n", VELAPET_MEMORY_DIR);
    }

  *pet = pet_load(VELAPET_PET_PROFILE_PATH);
  if (*pet == NULL)
    {
      *pet = pet_create_default();
      if (*pet == NULL)
        {
          return -1;
        }
    }

  *task = task_log_load(VELAPET_TASK_LOG_PATH);
  if (*task == NULL)
    {
      *task = task_log_create_default();
      if (*task == NULL)
        {
          pet_free(*pet);
          *pet = NULL;
          return -1;
        }
    }

  if (*pet == NULL || *task == NULL)
    {
      pet_free(*pet);
      task_log_free(*task);
      *pet = NULL;
      *task = NULL;
      return -1;
    }

  for (int level = 1; level <= (*pet)->level; level++)
    {
      LevelReward rewards[4];
      int reward_count = 0;

      if (reward_get_by_level(level, rewards,
                              (int)(sizeof(rewards) / sizeof(rewards[0])),
                              &reward_count) == 0)
        {
          for (int i = 0; i < reward_count; i++)
            {
              if (strcmp(rewards[i].reward_type, "skin") == 0)
                {
                  pet_unlock_skin(*pet, rewards[i].reward_data);
                }
            }
        }
    }

  if ((*pet)->consecutive_days >= 3)
    {
      pet_unlock_skin(*pet, "rainbow");
    }

  task_log_refresh_daily(*task);
  (*pet)->last_task_refresh = (*task)->last_refresh_time;
  vela_pet_save_state(*pet, *task);
  return 0;
}

int vela_pet_save_state(PetProfile *pet, TaskLog *task)
{
  int ret1 = 0;
  int ret2 = 0;

  if (pet == NULL || task == NULL)
    {
      return -1;
    }

  storage_ensure_dir(VELAPET_MEMORY_DIR);
  ret1 = pet_save(pet, VELAPET_PET_PROFILE_PATH);
  ret2 = task_log_save(task, VELAPET_TASK_LOG_PATH);
  return ret1 == 0 && ret2 == 0 ? 0 : -1;
}
