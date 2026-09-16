#ifndef VELAPET_REWARD_H
#define VELAPET_REWARD_H

#include "pet.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  int level;
  char reward_type[32];
  char reward_name[64];
  char reward_data[128];
} LevelReward;

int reward_get_by_level(int level, LevelReward *out_buffer, int max_count,
                        int *actual_count);
int reward_apply(PetProfile *pet, const LevelReward *reward);

#ifdef __cplusplus
}
#endif

#endif
