#include "vela_pet_service.h"

#include "vela_pet_api.h"

#include <pthread.h>
#include <unistd.h>

static pthread_t g_service_tid;
static volatile int g_service_running;

static void *vela_pet_service_thread(void *arg)
{
  (void)arg;

  while (g_service_running)
    {
      sleep(60);
      vela_pet_tick();
    }

  return NULL;
}

int vela_pet_service_start(void)
{
  if (g_service_running)
    {
      return 0;
    }

  g_service_running = 1;
  if (pthread_create(&g_service_tid, NULL, vela_pet_service_thread, NULL) != 0)
    {
      g_service_running = 0;
      return -1;
    }

  pthread_detach(g_service_tid);
  return 0;
}

int vela_pet_service_stop(void)
{
  g_service_running = 0;
  return 0;
}
