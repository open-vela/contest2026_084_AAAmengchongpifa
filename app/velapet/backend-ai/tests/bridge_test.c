/* Host regression test. Real pthread bridge, fake transport/core/UI. */
#include "velapet_ai_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int loading, replies, rewards, downloads, ready;
static int simulate_failure;
void velapet_ui_chat_set_loading(int n) { loading = n; }
void velapet_ui_chat_set_reply(const char *s) { assert(s && *s); replies++; }
void velapet_ui_chat_set_error(const char *s) { assert(s && *s); }
void velapet_ui_pet_image_set_state(int state, const char *s)
{ if (state == 2) { assert(s && strstr(s, "home_pet.png")); ready++; } }
char *vela_pet_get_ai_context(void) { char *s = malloc(3); strcpy(s, "{}"); return s; }
void vela_pet_free_ai_context(char *s) { free(s); }
int vela_pet_on_ai_encouragement(void) { rewards++; return 0; }

int velapet_ai_client_init(VelapetAiClient *c, const VelapetAiClientConfig *cfg)
{ c->config = *cfg; return 0; }
int velapet_ai_client_chat(VelapetAiClient *c, const char *p, const char *m,
                           const char *ctx, VelapetAiReply *r)
{
  (void)c; (void)m; (void)ctx;
  assert(strcmp(p, "cheer") == 0);
  usleep(10000);
  strcpy(r->reply, "Keep going!"); r->reward_eligible = 1;
  return simulate_failure ? -2 : 0;
}
int velapet_ai_client_submit_asset(VelapetAiClient *c, const char *s,
    const char *style, const char *kinds, VelapetAiAssetJob *j)
{
  (void)c; (void)s; (void)style; (void)kinds;
  strcpy(j->job_id, "test"); strcpy(j->status, "succeeded"); j->asset_count = 3;
  strcpy(j->assets[0].kind, "avatar");
  strcpy(j->assets[1].kind, "home");
  strcpy(j->assets[1].device_path, "/data/agent/assets/pet/home_pet.png");
  strcpy(j->assets[2].kind, "skin"); return 0;
}
int velapet_ai_client_get_asset_job(VelapetAiClient *c, const char *id, VelapetAiAssetJob *j)
{ (void)c; (void)id; (void)j; assert(0); return -1; }
int velapet_ai_client_download_asset(VelapetAiClient *c, const VelapetAiAsset *a)
{ (void)c; (void)a; downloads++; return 0; }

static void wait_result(void)
{ for (int i = 0; i < 200; i++) { usleep(1000); velapet_ai_bridge_poll_ui(); } }

int main(void)
{
  VelapetAiBridgeConfig cfg = {0};
  assert(velapet_ai_bridge_init(&cfg) == 0);
  assert(velapet_ai_bridge_init(&cfg) == -1);
  assert(velapet_ai_request_reply("cheer") == 0);
  assert(loading == 1);
  assert(velapet_ai_request_reply("task") == -1);
  assert(loading == 1); /* Busy request must not reset an active request. */
  wait_result(); assert(replies == 1 && rewards == 1 && !loading);
  assert(velapet_ai_request_reply("cheer") == 0);
  wait_result(); assert(replies == 2 && rewards == 1); /* Cooldown. */
  assert(velapet_ai_request_pet_image("reference.png") == 0);
  wait_result(); assert(downloads == 3 && ready == 1);
  simulate_failure = 1;
  assert(velapet_ai_request_reply("cheer") == 0);
  wait_result(); assert(!loading && rewards == 1);
  velapet_ai_bridge_deinit();
  puts("bridge: busy protection, cooldown, three downloads, failure and shutdown passed");
  return 0;
}
