#ifndef VELAPET_AI_CLIENT_H
#define VELAPET_AI_CLIENT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VELAPET_AI_TEXT_LEN 256
#define VELAPET_AI_ID_LEN 65
#define VELAPET_AI_URL_LEN 384
#define VELAPET_AI_PATH_LEN 160
#define VELAPET_AI_HASH_LEN 65
#define VELAPET_AI_MAX_ASSETS 3

/* Every transport callback returns 0 when it completed the HTTP exchange.
 * The callback must set http_status. Authentication uses X-VelaPet-Token.
 */
typedef int (*VelapetHttpPostJsonFn)(
    const char *url, const char *device_token, const char *json_body,
    char *response, size_t response_size, int *http_status, void *user_data);

typedef int (*VelapetHttpPostImageFn)(
    const char *url, const char *device_token, const char *file_path,
    const char *device_id, const char *style, const char *asset_kinds,
    char *response, size_t response_size, int *http_status, void *user_data);

typedef int (*VelapetHttpGetJsonFn)(
    const char *url, const char *device_token, char *response,
    size_t response_size, int *http_status, void *user_data);

typedef int (*VelapetHttpDownloadFn)(
    const char *url, const char *device_token, const char *destination_path,
    const char *expected_sha256, int expected_size, int *http_status,
    void *user_data);

typedef struct
{
  const char *base_url;
  const char *device_id;
  const char *device_token;
  const char *locale;
  VelapetHttpPostJsonFn post_json;
  VelapetHttpPostImageFn post_image;
  VelapetHttpGetJsonFn get_json;
  VelapetHttpDownloadFn download;
  void *transport_user_data;
} VelapetAiClientConfig;

typedef struct
{
  VelapetAiClientConfig config;
} VelapetAiClient;

typedef struct
{
  char request_id[VELAPET_AI_ID_LEN];
  char reply[VELAPET_AI_TEXT_LEN];
  char intent[32];
  char emotion[32];
  char source[32];
  char error_code[64];
  int reward_eligible;
  char reward_event_id[VELAPET_AI_ID_LEN];
} VelapetAiReply;

typedef struct
{
  char kind[16];
  char version[32];
  char sha256[VELAPET_AI_HASH_LEN];
  char download_url[VELAPET_AI_URL_LEN];
  char device_path[VELAPET_AI_PATH_LEN];
  int byte_size;
  int width;
  int height;
} VelapetAiAsset;

typedef struct
{
  char job_id[VELAPET_AI_ID_LEN];
  char status[24];
  char error_code[64];
  char error_message[VELAPET_AI_TEXT_LEN];
  VelapetAiAsset assets[VELAPET_AI_MAX_ASSETS];
  int asset_count;
} VelapetAiAssetJob;

enum
{
  VELAPET_AI_OK = 0,
  VELAPET_AI_ERR_ARGUMENT = -1,
  VELAPET_AI_ERR_TRANSPORT = -2,
  VELAPET_AI_ERR_HTTP = -3,
  VELAPET_AI_ERR_JSON = -4,
  VELAPET_AI_ERR_BUFFER = -5
};

int velapet_ai_client_init(VelapetAiClient *client,
                           const VelapetAiClientConfig *config);

int velapet_ai_client_chat(VelapetAiClient *client, const char *prompt_type,
                           const char *message, const char *context_json,
                           VelapetAiReply *out_reply);

int velapet_ai_client_submit_asset(VelapetAiClient *client,
                                  const char *source_path,
                                  const char *style,
                                  const char *asset_kinds,
                                  VelapetAiAssetJob *out_job);

int velapet_ai_client_get_asset_job(VelapetAiClient *client,
                                   const char *job_id,
                                   VelapetAiAssetJob *out_job);

int velapet_ai_client_download_asset(VelapetAiClient *client,
                                    const VelapetAiAsset *asset);

#ifdef __cplusplus
}
#endif

#endif
