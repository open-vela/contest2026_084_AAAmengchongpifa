#include "../src/pet.h"
#include "../src/task.h"

#include <assert.h>
#include <stdio.h>

static int find_task(TaskLog *log, TaskType type)
{
  for (int i = 0; i < log->task_count; i++)
    {
      if (log->tasks[i].type == type)
        {
          return i;
        }
    }

  return -1;
}

int main(void)
{
  PetProfile *pet = pet_create_default();
  TaskLog *log = task_log_create_default();
  int daily_steps = find_task(log, TASK_TYPE_DAILY_STEPS);
  int walk = find_task(log, TASK_TYPE_CONTINUOUS_WALK);
  int checkin = find_task(log, TASK_TYPE_CHECKIN);
  int social = find_task(log, TASK_TYPE_SOCIAL);

  assert(pet != NULL);
  assert(log != NULL);
  assert(pet_is_skin_unlocked(pet, "default") == 1);
  assert(pet_change_skin(pet, "sport_blue") < 0);
  assert(pet_unlock_skin(pet, "sport_blue") == 0);
  assert(pet_change_skin(pet, "sport_blue") == 0);
  assert(pet_change_skin(pet, "/data/agent/assets/pet/sport_blue") == 0);
  assert(pet_get_skin_asset_path(pet->current_skin) != NULL);
  assert(daily_steps >= 0);
  assert(walk >= 0);
  assert(checkin >= 0);
  assert(social >= 0);

  assert(task_update_progress(log, TASK_TYPE_DAILY_STEPS, 2999) == 1);
  assert(log->tasks[daily_steps].status == TASK_STATUS_IN_PROGRESS);
  assert(task_update_progress(log, TASK_TYPE_DAILY_STEPS, 1) == 1);
  assert(log->tasks[daily_steps].status == TASK_STATUS_COMPLETED);
  assert(task_claim_reward(log, daily_steps, pet) == 20);
  assert(pet->exp == 20);

  assert(task_update_progress(log, TASK_TYPE_CONTINUOUS_WALK, 10) == 1);
  assert(task_claim_reward(log, walk, pet) == 30);
  assert(pet->exp == 50);

  assert(task_checkin(log, pet) >= 1);
  assert(task_checkin(log, pet) == 0);
  assert(log->tasks[checkin].status == TASK_STATUS_COMPLETED);
  assert(task_claim_reward(log, checkin, pet) == 10);

  assert(task_update_progress(log, TASK_TYPE_SOCIAL, 1) == 1);
  assert(task_claim_reward(log, social, pet) == 15);
  assert(task_all_completed(log) == 1);
  assert(pet_change_skin(pet, "/data/agent/assets/pet/flame_red") == -1);

  assert(pet_add_exp(pet, 25) >= 1);
  assert(pet->level == 2);
  assert(pet->exp == 0);
  assert(pet->exp_to_next == 250);
  assert(pet_is_skin_unlocked(pet, "/data/agent/assets/pet/sport_blue") == 1);
  assert(pet_change_skin(pet, "/data/agent/assets/pet/default") == 0);
  assert(pet_change_skin(pet, "/data/agent/assets/pet/sport_blue") == 0);

  printf("VelaPet logic test passed: level=%d exp=%d skin=%s\n",
         pet->level, pet->exp, pet->current_skin);

  task_log_free(log);
  pet_free(pet);
  return 0;
}
