#include "pet.h"

#include "json_utils.h"
#include "reward.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  const char *id;
  const char *path;
} PetSkinDef;

static const PetSkinDef g_skin_defs[] =
{
  { "default", VELAPET_SKIN_ASSET_PREFIX "default" },
  { "sport_blue", VELAPET_SKIN_ASSET_PREFIX "sport_blue" },
  { "flame_red", VELAPET_SKIN_ASSET_PREFIX "flame_red" },
  { "star_purple", VELAPET_SKIN_ASSET_PREFIX "star_purple" },
  { "rainbow", VELAPET_SKIN_ASSET_PREFIX "rainbow" }
};

static void pet_copy_string(char *dst, size_t dst_len, const char *src)
{
  if (dst == NULL || dst_len == 0)
    {
      return;
    }

  if (src == NULL)
    {
      dst[0] = '\0';
      return;
    }

  snprintf(dst, dst_len, "%s", src);
}

const char *pet_normalize_skin_id(const char *skin_name_or_path)
{
  if (skin_name_or_path == NULL || skin_name_or_path[0] == '\0')
    {
      return NULL;
    }

  for (size_t i = 0; i < sizeof(g_skin_defs) / sizeof(g_skin_defs[0]); i++)
    {
      if (strcmp(skin_name_or_path, g_skin_defs[i].id) == 0 ||
          strcmp(skin_name_or_path, g_skin_defs[i].path) == 0)
        {
          return g_skin_defs[i].id;
        }
    }

  return NULL;
}

const char *pet_get_skin_asset_path(const char *skin_id)
{
  const char *normalized = pet_normalize_skin_id(skin_id);

  if (normalized == NULL)
    {
      return NULL;
    }

  for (size_t i = 0; i < sizeof(g_skin_defs) / sizeof(g_skin_defs[0]); i++)
    {
      if (strcmp(normalized, g_skin_defs[i].id) == 0)
        {
          return g_skin_defs[i].path;
        }
    }

  return NULL;
}

PetProfile *pet_create_default(void)
{
  PetProfile *pet = (PetProfile *)calloc(1, sizeof(PetProfile));
  if (pet == NULL)
    {
      return NULL;
    }

  pet_copy_string(pet->name, sizeof(pet->name), "Vela");
  pet->level = 1;
  pet->exp = 0;
  pet->exp_to_next = pet_exp_required(1);
  pet_copy_string(pet->current_skin, sizeof(pet->current_skin),
                  VELAPET_DEFAULT_SKIN);
  pet_copy_string(pet->personality, sizeof(pet->personality), "cheerful");
  pet->total_steps = 0;
  pet->consecutive_days = 0;
  pet->last_active = time(NULL);
  pet->last_task_refresh = 0;
  pet->unlocked_skin_count = 0;
  pet_unlock_skin(pet, VELAPET_DEFAULT_SKIN);
  return pet;
}

void pet_free(PetProfile *pet)
{
  free(pet);
}

PetProfile *pet_load(const char *path)
{
  cJSON *root = json_load_from_file(path);
  if (root == NULL)
    {
      return pet_create_default();
    }

  PetProfile *pet = pet_from_json(root);
  json_free(root);
  if (pet == NULL)
    {
      return pet_create_default();
    }

  if (pet->level < 1)
    {
      pet->level = 1;
    }

  if (pet->exp_to_next <= 0)
    {
      pet->exp_to_next = pet_exp_required(pet->level);
    }

  return pet;
}

int pet_save(PetProfile *pet, const char *path)
{
  if (pet == NULL || path == NULL)
    {
      return -1;
    }

  cJSON *root = pet_to_json(pet);
  if (root == NULL)
    {
      return -1;
    }

  int ret = json_save_to_file(path, root);
  json_free(root);
  return ret;
}

int pet_add_exp(PetProfile *pet, int exp)
{
  if (pet == NULL || exp <= 0)
    {
      return -1;
    }

  pet->exp += exp;
  pet->last_active = time(NULL);
  return pet_check_level_up(pet);
}

int pet_check_level_up(PetProfile *pet)
{
  int levels_gained = 0;

  if (pet == NULL)
    {
      return -1;
    }

  if (pet->exp_to_next <= 0)
    {
      pet->exp_to_next = pet_exp_required(pet->level);
    }

  while (pet->exp >= pet->exp_to_next)
    {
      int reward_count = 0;
      LevelReward rewards[4];

      pet->exp -= pet->exp_to_next;
      pet->level++;
      levels_gained++;
      pet->exp_to_next = pet_exp_required(pet->level);

      if (reward_get_by_level(pet->level, rewards,
                              (int)(sizeof(rewards) / sizeof(rewards[0])),
                              &reward_count) < 0)
        {
          reward_count = 0;
        }

      for (int i = 0; i < reward_count; i++)
        {
          reward_apply(pet, &rewards[i]);
        }
    }

  return levels_gained;
}

int pet_exp_required(int level)
{
  if (level <= 1)
    {
      return 100;
    }

  return level * 100 + 50;
}

int pet_is_skin_unlocked(const PetProfile *pet, const char *skin_name)
{
  const char *skin_id = pet_normalize_skin_id(skin_name);

  if (pet == NULL || skin_name == NULL || skin_name[0] == '\0')
    {
      return 0;
    }

  if (skin_id == NULL)
    {
      return 0;
    }

  if (strcmp(skin_id, VELAPET_DEFAULT_SKIN) == 0)
    {
      return 1;
    }

  for (int i = 0; i < pet->unlocked_skin_count && i < VELAPET_MAX_SKINS; i++)
    {
      if (strcmp(pet->unlocked_skins[i], skin_id) == 0)
        {
          return 1;
        }
    }

  return 0;
}

int pet_unlock_skin(PetProfile *pet, const char *skin_name)
{
  const char *skin_id = pet_normalize_skin_id(skin_name);

  if (pet == NULL || skin_name == NULL || skin_name[0] == '\0')
    {
      return -1;
    }

  if (skin_id == NULL)
    {
      return -1;
    }

  if (pet->unlocked_skin_count < 0)
    {
      pet->unlocked_skin_count = 0;
    }

  for (int i = 0; i < pet->unlocked_skin_count && i < VELAPET_MAX_SKINS; i++)
    {
      if (strcmp(pet->unlocked_skins[i], skin_id) == 0)
        {
          return 0;
        }
    }

  if (pet->unlocked_skin_count >= VELAPET_MAX_SKINS)
    {
      return -1;
    }

  pet_copy_string(pet->unlocked_skins[pet->unlocked_skin_count],
                  sizeof(pet->unlocked_skins[pet->unlocked_skin_count]),
                  skin_id);
  pet->unlocked_skin_count++;
  return 0;
}

int pet_change_skin(PetProfile *pet, const char *skin_name)
{
  const char *skin_id = pet_normalize_skin_id(skin_name);

  if (pet == NULL || skin_name == NULL || skin_name[0] == '\0')
    {
      return -1;
    }

  if (skin_id == NULL)
    {
      return -1;
    }

  if (!pet_is_skin_unlocked(pet, skin_name))
    {
      return -1;
    }

  pet_copy_string(pet->current_skin, sizeof(pet->current_skin), skin_id);
  pet->last_active = time(NULL);
  return 0;
}

const char *pet_get_title(int level)
{
  if (level <= 5)
    {
      return "egg_baby";
    }
  else if (level <= 10)
    {
      return "young_pet";
    }
  else if (level <= 20)
    {
      return "sport_star";
    }

  return "vela_legend";
}
