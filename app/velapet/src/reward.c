#include "reward.h"

#include <stdio.h>
#include <string.h>

static LevelReward g_level_rewards[] =
{
  {
    .level = 2,
    .reward_type = "skin",
    .reward_name = "sport_blue",
    .reward_data = "sport_blue"
  },
  {
    .level = 5,
    .reward_type = "skin",
    .reward_name = "flame_red",
    .reward_data = "flame_red"
  },
  {
    .level = 10,
    .reward_type = "skin",
    .reward_name = "star_purple",
    .reward_data = "star_purple"
  }
};

int reward_get_by_level(int level, LevelReward *out_buffer, int max_count,
                        int *actual_count)
{
  int matched_count = 0;

  if (out_buffer == NULL || actual_count == NULL || max_count <= 0)
    {
      return -1;
    }

  *actual_count = 0;
  for (size_t i = 0; i < sizeof(g_level_rewards) / sizeof(g_level_rewards[0]);
       i++)
    {
      if (g_level_rewards[i].level == level && matched_count < max_count)
        {
          out_buffer[matched_count++] = g_level_rewards[i];
        }
    }

  *actual_count = matched_count;
  return 0;
}

int reward_apply(PetProfile *pet, const LevelReward *reward)
{
  if (pet == NULL || reward == NULL)
    {
      return -1;
    }

  if (strcmp(reward->reward_type, "skin") == 0)
    {
      if (pet_unlock_skin(pet, reward->reward_data) < 0)
        {
          return -1;
        }

      return pet_change_skin(pet, reward->reward_data);
    }

  if (strcmp(reward->reward_type, "title") == 0 ||
      strcmp(reward->reward_type, "action") == 0)
    {
      return 0;
    }

  return -1;
}
