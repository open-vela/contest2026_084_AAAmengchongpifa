#include "velapet_ai_bridge.h"

#include "vela_pet_api.h"
#include "velapet_ui.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef enum
{
  BRIDGE_REQUEST_NONE = 0,
  BRIDGE_REQUEST_CHAT,
  BRIDGE_REQUEST_IMAGE
} BridgeRequestType;

typedef struct
{
  pthread_mutex_t lock;
  pthread_t worker;
  int initialized;
  int worker_active;
  int worker_done;
  BridgeRequestType request_type;
  VelapetAiClient client;
  int image_poll_interval_seconds;
  int image_timeout_seconds;
  int reward_cooldown_seconds;
  time_t last_reward_time;
  char prompt_type[24];
  char message[241];
  char source_path[VELAPET_AI_PATH_LEN];
  VelapetAiReply reply;
  VelapetAiAssetJob asset_job;
  int result;
} BridgeState;

static BridgeState g_bridge = {
  .lock = PTHREAD_MUTEX_INITIALIZER,
};

static void copy_text(char *dst, size_t dst_size, const char *src)
{
  snprintf(dst, dst_size, "%s", src == NULL ? "" : src);
}

static double monotonic_seconds(void)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (double)now.tv_sec + now.tv_nsec / 1000000000.0;
}

static VelapetAiAsset *find_home_asset(VelapetAiAssetJob *job)
{
  VelapetAiAsset *fallback = NULL;
  for (int i = 0; i < job->asset_count && i < VELAPET_AI_MAX_ASSETS; i++)
    {
      if (fallback == NULL)
        {
          fallback = &job->assets[i];
        }
      if (strcmp(job->assets[i].kind, "home") == 0)
        {
          return &job->assets[i];
        }
    }
  return fallback;
}

static int run_chat(void)
{
  char *context = vela_pet_get_ai_context();
  int ret;

  if (context == NULL)
    {
      return VELAPET_AI_ERR_JSON;
    }
  ret = velapet_ai_client_chat(&g_bridge.client, g_bridge.prompt_type,
                               g_bridge.message, context, &g_bridge.reply);
  vela_pet_free_ai_context(context);
  return ret;
}

static int run_image(void)
{
  double deadline = monotonic_seconds() + g_bridge.image_timeout_seconds;
  int ret = velapet_ai_client_submit_asset(
      &g_bridge.client, g_bridge.source_path, "soft cartoon mascot",
      "avatar,home,skin", &g_bridge.asset_job);
  if (ret != VELAPET_AI_OK)
    {
      return ret;
    }

  while (monotonic_seconds() < deadline)
    {
      if (strcmp(g_bridge.asset_job.status, "succeeded") == 0)
        {
          if (find_home_asset(&g_bridge.asset_job) == NULL)
            return VELAPET_AI_ERR_JSON;
          for (int i = 0; i < g_bridge.asset_job.asset_count; i++)
            {
              if (monotonic_seconds() >= deadline)
                return VELAPET_AI_ERR_TRANSPORT;
              ret = velapet_ai_client_download_asset(&g_bridge.client,
                                        &g_bridge.asset_job.assets[i]);
              if (ret != VELAPET_AI_OK) return ret;
            }
          return VELAPET_AI_OK;
        }
      if (strcmp(g_bridge.asset_job.status, "failed") == 0)
        {
          return VELAPET_AI_ERR_HTTP;
        }

      sleep((unsigned int)g_bridge.image_poll_interval_seconds);
      ret = velapet_ai_client_get_asset_job(
          &g_bridge.client, g_bridge.asset_job.job_id, &g_bridge.asset_job);
      if (ret != VELAPET_AI_OK)
        {
          return ret;
        }
    }
  return VELAPET_AI_ERR_TRANSPORT;
}

static void *worker_main(void *arg)
{
  BridgeRequestType type = (BridgeRequestType)(intptr_t)arg;
  int result = type == BRIDGE_REQUEST_CHAT ? run_chat() : run_image();

  pthread_mutex_lock(&g_bridge.lock);
  g_bridge.result = result;
  g_bridge.worker_done = 1;
  pthread_mutex_unlock(&g_bridge.lock);
  return NULL;
}

static int start_request(BridgeRequestType type, const char *arg1,
                         const char *arg2)
{
  int ret;
  pthread_attr_t attr;

  pthread_mutex_lock(&g_bridge.lock);
  if (!g_bridge.initialized || g_bridge.worker_active)
    {
      pthread_mutex_unlock(&g_bridge.lock);
      return -1;
    }
  g_bridge.worker_active = 1;
  g_bridge.worker_done = 0;
  g_bridge.request_type = type;
  if (type == BRIDGE_REQUEST_CHAT)
    {
      copy_text(g_bridge.prompt_type, sizeof(g_bridge.prompt_type), arg1);
      copy_text(g_bridge.message, sizeof(g_bridge.message), arg2);
    }
  else
    {
      copy_text(g_bridge.source_path, sizeof(g_bridge.source_path), arg1);
    }
  memset(&g_bridge.reply, 0, sizeof(g_bridge.reply));
  memset(&g_bridge.asset_job, 0, sizeof(g_bridge.asset_job));
  /* JSON response alone occupies 8 KB; NuttX default stacks can be smaller. */
  ret = pthread_attr_init(&attr);
  if (ret == 0)
    {
      ret = pthread_attr_setstacksize(&attr, 64 * 1024);
      if (ret == 0)
        ret = pthread_create(&g_bridge.worker, &attr, worker_main,
                             (void *)(intptr_t)type);
      pthread_attr_destroy(&attr);
    }
  if (ret != 0)
    {
      g_bridge.worker_active = 0;
      g_bridge.request_type = BRIDGE_REQUEST_NONE;
    }
  pthread_mutex_unlock(&g_bridge.lock);
  return ret == 0 ? 0 : -1;
}

int velapet_ai_bridge_init(const VelapetAiBridgeConfig *config)
{
  /* Lifecycle and request functions are serialized on the UI thread. */
  if (g_bridge.initialized || g_bridge.worker_active) return -1;
  if (config == NULL ||
      velapet_ai_client_init(&g_bridge.client, &config->client) != 0)
    {
      return -1;
    }

  pthread_mutex_lock(&g_bridge.lock);
  g_bridge.image_poll_interval_seconds =
      config->image_poll_interval_seconds > 0
          ? config->image_poll_interval_seconds
          : 2;
  g_bridge.image_timeout_seconds =
      config->image_timeout_seconds > 0 ? config->image_timeout_seconds : 120;
  g_bridge.reward_cooldown_seconds =
      config->reward_cooldown_seconds > 0 ? config->reward_cooldown_seconds
                                          : 300;
  g_bridge.initialized = 1;
  pthread_mutex_unlock(&g_bridge.lock);
  return 0;
}

void velapet_ai_bridge_deinit(void)
{
  pthread_mutex_lock(&g_bridge.lock);
  if (g_bridge.worker_active)
    {
      pthread_mutex_unlock(&g_bridge.lock);
      pthread_join(g_bridge.worker, NULL);
      pthread_mutex_lock(&g_bridge.lock);
    }
  g_bridge.initialized = 0;
  g_bridge.worker_active = 0;
  g_bridge.worker_done = 0;
  pthread_mutex_unlock(&g_bridge.lock);
}

int velapet_ai_request_reply(const char *prompt_type)
{
  if (g_bridge.worker_active) return -1;
  velapet_ui_chat_set_loading(1);
  if (start_request(BRIDGE_REQUEST_CHAT,
                    prompt_type == NULL ? "cheer" : prompt_type, "") != 0)
    {
      velapet_ui_chat_set_error("AI request is busy. Please try again.");
      velapet_ui_chat_set_loading(0);
      return -1;
    }
  return 0;
}

int velapet_ai_request_custom_reply(const char *message)
{
  if (g_bridge.worker_active) return -1;
  if (message == NULL || strlen(message) >= sizeof(g_bridge.message)) return -1;
  velapet_ui_chat_set_loading(1);
  if (start_request(BRIDGE_REQUEST_CHAT, "custom", message) != 0)
    {
      velapet_ui_chat_set_error("AI request is busy. Please try again.");
      velapet_ui_chat_set_loading(0);
      return -1;
    }
  return 0;
}

int velapet_ai_request_pet_image(const char *source_path)
{
  if (g_bridge.worker_active) return -1;
  if (source_path == NULL || source_path[0] == '\0')
    {
      return -1;
    }
  if (strlen(source_path) >= sizeof(g_bridge.source_path)) return -1;
  velapet_ui_pet_image_set_state(1, NULL);
  if (start_request(BRIDGE_REQUEST_IMAGE, source_path, NULL) != 0)
    {
      velapet_ui_pet_image_set_state(3, NULL);
      return -1;
    }
  return 0;
}

void velapet_ai_bridge_poll_ui(void)
{
  BridgeRequestType type;
  int result;
  int done;

  pthread_mutex_lock(&g_bridge.lock);
  done = g_bridge.worker_active && g_bridge.worker_done;
  pthread_mutex_unlock(&g_bridge.lock);
  if (!done)
    {
      return;
    }

  pthread_join(g_bridge.worker, NULL);
  pthread_mutex_lock(&g_bridge.lock);
  type = g_bridge.request_type;
  result = g_bridge.result;
  g_bridge.worker_active = 0;
  g_bridge.worker_done = 0;
  g_bridge.request_type = BRIDGE_REQUEST_NONE;
  pthread_mutex_unlock(&g_bridge.lock);

  if (type == BRIDGE_REQUEST_CHAT)
    {
      if (result == VELAPET_AI_OK)
        {
          velapet_ui_chat_set_reply(g_bridge.reply.reply);
          if (g_bridge.reply.reward_eligible)
            {
              time_t now = time(NULL);
              if (g_bridge.last_reward_time == 0 ||
                  now - g_bridge.last_reward_time >=
                      g_bridge.reward_cooldown_seconds)
                {
                  vela_pet_on_ai_encouragement();
                  g_bridge.last_reward_time = now;
                }
            }
        }
      else
        {
          velapet_ui_chat_set_error("Network busy. Vela is still with you.");
        }
      velapet_ui_chat_set_loading(0);
    }
  else if (type == BRIDGE_REQUEST_IMAGE)
    {
      if (result == VELAPET_AI_OK)
        {
          VelapetAiAsset *asset = find_home_asset(&g_bridge.asset_job);
          velapet_ui_pet_image_set_state(
              2, asset == NULL ? NULL : asset->device_path);
        }
      else
        {
          velapet_ui_pet_image_set_state(3, NULL);
        }
    }
}
