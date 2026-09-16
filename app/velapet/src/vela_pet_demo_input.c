#include <nuttx/config.h>

#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

#include <nuttx/input/buttons.h>

#include <lvgl/lvgl.h>

#include "vela_pet_api.h"
#include "velapet_ui.h"

#include "vela_pet_demo_input.h"

/* Simulated activity.  Ambient steps walk the 3000-step daily task in about a
 * minute; one KEY2 press is worth a burst of walking plus exercise minutes.
 *
 * Only one button is wired on this board: sf32lb52_buttons.c implements KEY2
 * (PA11) and reports it as bit 0, so KEY1 is not available here.
 */

#define DEMO_POLL_MS            200
#define DEMO_TICKS_PER_STEP     10   /* x200ms => ambient tick every 2s */
#define DEMO_STEPS_PER_TICK     100
#define DEMO_STEPS_PER_PRESS    500
#define DEMO_MINUTES_PER_PRESS  2
#define DEMO_BUTTON_KEY2        (1u << 0)

static volatile int g_dirty;
static volatile int g_running;
static pthread_t g_thread;

static void *demo_worker(void *arg)
{
  int fd;
  int previous = 0;
  int ticks = 0;

  (void)arg;

  fd = open("/dev/buttons", O_RDONLY);

  while (g_running)
    {
      int current;

      usleep(DEMO_POLL_MS * 1000);

      /* btn_read() returns the current button state immediately. */

      current = -1;
      if (fd >= 0)
        {
          btn_buttonset_t set = 0;

          if (read(fd, &set, sizeof(set)) == (ssize_t)sizeof(set))
            {
              current = (int)set;
            }
        }

      if (current >= 0)
        {
          if ((current & DEMO_BUTTON_KEY2) && !(previous & DEMO_BUTTON_KEY2))
            {
              vela_pet_update_steps(DEMO_STEPS_PER_PRESS);
              vela_pet_update_exercise_time(DEMO_MINUTES_PER_PRESS);
              g_dirty = 1;
            }

          previous = current;
        }

      /* Ambient steps.
       *
       * vela_pet_update_steps() saves the state on every call.  That is
       * cheap while /data is a tmpfs, but once /data moves to littlefs on
       * NOR flash this rate needs batching to avoid erase/write wear.
       */

      if (++ticks >= DEMO_TICKS_PER_STEP)
        {
          ticks = 0;
          vela_pet_update_steps(DEMO_STEPS_PER_TICK);
          g_dirty = 1;
        }
    }

  if (fd >= 0)
    {
      close(fd);
    }

  return NULL;
}

static void demo_refresh_timer(lv_timer_t *timer)
{
  (void)timer;

  if (g_dirty)
    {
      g_dirty = 0;
      velapet_ui_refresh_all();
    }
}

int velapet_demo_input_start(void)
{
  if (g_running)
    {
      return 0;
    }

  g_running = 1;

  if (pthread_create(&g_thread, NULL, demo_worker, NULL) != 0)
    {
      g_running = 0;
      return -1;
    }

  pthread_detach(g_thread);

  /* Created from the LVGL thread: the worker only raises the dirty flag. */

  lv_timer_create(demo_refresh_timer, DEMO_POLL_MS, NULL);
  return 0;
}
