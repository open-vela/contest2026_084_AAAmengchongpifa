#ifndef VELAPET_PET_H
#define VELAPET_PET_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VELAPET_NAME_LEN 32
#define VELAPET_SKIN_LEN 64
#define VELAPET_PERSONALITY_LEN 32
#define VELAPET_MAX_SKINS 8
#define VELAPET_SKIN_ASSET_PREFIX "/data/agent/assets/pet/"
#define VELAPET_DEFAULT_SKIN "default"

typedef struct
{
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

PetProfile *pet_create_default(void);
void pet_free(PetProfile *pet);
PetProfile *pet_load(const char *path);
int pet_save(PetProfile *pet, const char *path);
int pet_add_exp(PetProfile *pet, int exp);
int pet_check_level_up(PetProfile *pet);
int pet_exp_required(int level);
const char *pet_normalize_skin_id(const char *skin_name_or_path);
const char *pet_get_skin_asset_path(const char *skin_id);
int pet_is_skin_unlocked(const PetProfile *pet, const char *skin_name);
int pet_unlock_skin(PetProfile *pet, const char *skin_name);
int pet_change_skin(PetProfile *pet, const char *skin_name);
const char *pet_get_title(int level);

#ifdef __cplusplus
}
#endif

#endif
