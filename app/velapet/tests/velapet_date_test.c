/****************************************************************************
 * Date-handling test for boards whose clock restarts at boot.
 *
 * The DevKit-LCD has no RTC backup battery, so every power cycle resets the
 * clock (the firmware seeds it from the build time).  A saved task log can
 * therefore hold timestamps that are in the future relative to the freshly
 * booted clock.  The daily logic must not lock up in that case: the daily
 * tasks have to refresh and check-in has to stay available.
 ****************************************************************************/

#include "../src/pet.h"
#include "../src/task.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>

#define TWO_DAYS (2 * 24 * 60 * 60)

static void test_clock_jumped_backwards(void)
{
  TaskLog *log = task_log_create_default();
  time_t now = time(NULL);

  assert(log != NULL);

  /* Simulate state saved before a power cycle: both timestamps sit in the
   * future relative to the clock we just booted with. */

  log->last_refresh_time = now + TWO_DAYS;
  log->last_checkin_time = now + TWO_DAYS;

  int refreshed = task_log_refresh_daily(log);
  int can_checkin = task_can_checkin(log);

  assert(refreshed == 1);
  assert(can_checkin == 1);

  printf("clock jumped backwards: refresh=%d can_checkin=%d\n",
         refreshed, can_checkin);

  task_log_free(log);
}

static void test_normal_same_day(void)
{
  TaskLog *log = task_log_create_default();
  time_t now = time(NULL);

  assert(log != NULL);

  /* Same day: no refresh, and check-in is already done for today. */

  log->last_refresh_time = now;
  log->last_checkin_time = now;

  assert(task_log_refresh_daily(log) == 0);
  assert(task_can_checkin(log) == 0);

  task_log_free(log);
}

static void test_checkin_still_works(void)
{
  TaskLog *log = task_log_create_default();
  PetProfile *pet = pet_create_default();
  time_t now = time(NULL);

  assert(log != NULL && pet != NULL);

  /* A check-in recorded a day ago still increments the streak. */

  log->last_checkin_time = now - 24 * 60 * 60;
  log->consecutive_days = 2;

  assert(task_can_checkin(log) == 1);
  assert(task_checkin(log, pet) == 3);

  /* And immediately after, today's check-in is done. */

  assert(task_can_checkin(log) == 0);

  task_log_free(log);
  pet_free(pet);
}

int main(void)
{
  test_clock_jumped_backwards();
  test_normal_same_day();
  test_checkin_still_works();

  printf("VelaPet date test passed\n");
  return 0;
}
