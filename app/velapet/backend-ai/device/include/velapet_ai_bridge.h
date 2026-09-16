#ifndef VELAPET_AI_BRIDGE_H
#define VELAPET_AI_BRIDGE_H

#include "velapet_ai_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  VelapetAiClientConfig client;
  int image_poll_interval_seconds;
  int image_timeout_seconds;
  int reward_cooldown_seconds;
} VelapetAiBridgeConfig;

/* All public functions must be called from the same UI thread.
 * Config strings and transport_user_data must outlive the bridge.
 * Deinit joins the worker; disable the polling timer before deinit.
 * Each HTTP callback MUST enforce a finite timeout.
 * Initialize once after vela_pet_init() and the network stack are ready. */
int velapet_ai_bridge_init(const VelapetAiBridgeConfig *config);
void velapet_ai_bridge_deinit(void);

/* Called by UI button callbacks. Returns 0 when the worker was started. */
int velapet_ai_request_reply(const char *prompt_type);
int velapet_ai_request_custom_reply(const char *message);
int velapet_ai_request_pet_image(const char *source_path);

/* Must be called periodically from the LVGL/UI thread. It is the only bridge
 * function that calls velapet_ui_* result setters.
 */
void velapet_ai_bridge_poll_ui(void);

#ifdef __cplusplus
}
#endif

#endif
