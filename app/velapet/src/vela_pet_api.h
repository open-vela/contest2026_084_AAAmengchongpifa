#ifndef VELAPET_API_H
#define VELAPET_API_H

#include "pet.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

int vela_pet_init(void);
int vela_pet_deinit(void);

/* Returns a read-only snapshot copy. Mutating the returned pointer does not
 * update the stored pet profile.
 */
PetProfile *vela_pet_get_profile(void);
Task *vela_pet_get_tasks(int *count);
int vela_pet_get_daily_progress(void);

/* Returns gained EXP. Returns 0 when the task is not claimable or has already
 * been claimed, and <0 on invalid input.
 */
int vela_pet_claim_task(int task_index);

/* Returns 1 when today's check-in is available, otherwise 0. */
int vela_pet_can_checkin(void);

/* Returns consecutive check-in days on success. Returns 0 when already checked
 * in today, and <0 when the module is not initialized.
 */
int vela_pet_checkin(void);
const char *vela_pet_get_current_skin_id(void);
const char *vela_pet_get_current_skin_asset_path(void);
const char *vela_pet_get_skin_asset_path(const char *skin_id);
int vela_pet_is_skin_unlocked(const char *skin_id);

/* Accepts either a skin id, such as "sport_blue", or a full asset path, such
 * as "/data/agent/assets/pet/sport_blue". Returns 0 on success.
 */
int vela_pet_switch_skin(const char *skin_name);
const char *vela_pet_get_levelup_message(void);

/* Returns a dynamically allocated JSON string for the AI module. The caller
 * must release it with vela_pet_free_ai_context().
 */
char *vela_pet_get_ai_context(void);
void vela_pet_free_ai_context(char *ptr);
int vela_pet_on_ai_encouragement(void);
const char *vela_pet_get_daily_summary(void);
int vela_pet_update_steps(int steps);
int vela_pet_update_exercise_time(int minutes);
int vela_pet_on_social_interaction(void);
int vela_pet_on_sit_reminder(void);
int vela_pet_tick(void);

#ifdef __cplusplus
}
#endif

#endif
