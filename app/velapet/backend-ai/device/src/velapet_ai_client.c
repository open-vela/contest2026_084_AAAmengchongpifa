#include "velapet_ai_client.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VELAPET_AI_RESPONSE_LEN 8192

static void copy_text(char *dst, size_t dst_size, const char *src)
{
  if (dst == NULL || dst_size == 0)
    {
      return;
    }

  snprintf(dst, dst_size, "%s", src == NULL ? "" : src);
}

static int join_url(const char *base_url, const char *path, char *out,
                    size_t out_size)
{
  int written;
  size_t base_len;

  if (base_url == NULL || path == NULL || out == NULL || out_size == 0)
    {
      return VELAPET_AI_ERR_ARGUMENT;
    }

  base_len = strlen(base_url);
  written = snprintf(out, out_size, "%s%s%s", base_url,
                     base_len > 0 && base_url[base_len - 1] == '/' ? "" : "/",
                     path[0] == '/' ? path + 1 : path);
  return written >= 0 && (size_t)written < out_size ? VELAPET_AI_OK
                                                   : VELAPET_AI_ERR_BUFFER;
}

static const char *json_string(const cJSON *root, const char *name)
{
  const cJSON *item = cJSON_GetObjectItemCaseSensitive((cJSON *)root, name);
  return cJSON_IsString(item) && item->valuestring != NULL ? item->valuestring
                                                          : "";
}

static int json_int(const cJSON *root, const char *name, int fallback)
{
  const cJSON *item = cJSON_GetObjectItemCaseSensitive((cJSON *)root, name);
  return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static int http_ok(int status)
{
  return status >= 200 && status < 300;
}

static int parse_asset_job(const char *json, VelapetAiAssetJob *out_job)
{
  cJSON *root = NULL;
  cJSON *artifacts = NULL;
  cJSON *item = NULL;
  int index = 0;

  if (json == NULL || out_job == NULL)
    {
      return VELAPET_AI_ERR_ARGUMENT;
    }

  memset(out_job, 0, sizeof(*out_job));
  root = cJSON_Parse(json);
  if (!cJSON_IsObject(root))
    {
      cJSON_Delete(root);
      return VELAPET_AI_ERR_JSON;
    }

  copy_text(out_job->job_id, sizeof(out_job->job_id),
            json_string(root, "job_id"));
  copy_text(out_job->status, sizeof(out_job->status),
            json_string(root, "status"));
  copy_text(out_job->error_code, sizeof(out_job->error_code),
            json_string(root, "error_code"));
  copy_text(out_job->error_message, sizeof(out_job->error_message),
            json_string(root, "error_message"));

  artifacts = cJSON_GetObjectItemCaseSensitive(root, "artifacts");
  cJSON_ArrayForEach(item, artifacts)
    {
      VelapetAiAsset *asset;
      if (index >= VELAPET_AI_MAX_ASSETS || !cJSON_IsObject(item))
        {
          continue;
        }

      asset = &out_job->assets[index++];
      copy_text(asset->kind, sizeof(asset->kind), json_string(item, "kind"));
      copy_text(asset->version, sizeof(asset->version),
                json_string(item, "version"));
      copy_text(asset->sha256, sizeof(asset->sha256),
                json_string(item, "sha256"));
      copy_text(asset->download_url, sizeof(asset->download_url),
                json_string(item, "download_url"));
      copy_text(asset->device_path, sizeof(asset->device_path),
                json_string(item, "device_path"));
      asset->byte_size = json_int(item, "byte_size", 0);
      asset->width = json_int(item, "width", 0);
      asset->height = json_int(item, "height", 0);
    }

  out_job->asset_count = index;
  cJSON_Delete(root);
  return out_job->job_id[0] != '\0' ? VELAPET_AI_OK : VELAPET_AI_ERR_JSON;
}

int velapet_ai_client_init(VelapetAiClient *client,
                           const VelapetAiClientConfig *config)
{
  if (client == NULL || config == NULL || config->base_url == NULL ||
      config->device_id == NULL || config->post_json == NULL)
    {
      return VELAPET_AI_ERR_ARGUMENT;
    }

  memset(client, 0, sizeof(*client));
  client->config = *config;
  return VELAPET_AI_OK;
}

int velapet_ai_client_chat(VelapetAiClient *client, const char *prompt_type,
                           const char *message, const char *context_json,
                           VelapetAiReply *out_reply)
{
  char url[VELAPET_AI_URL_LEN];
  char response[VELAPET_AI_RESPONSE_LEN];
  cJSON *root = NULL;
  cJSON *context = NULL;
  cJSON *parsed = NULL;
  cJSON *reward = NULL;
  char *body = NULL;
  int http_status = 0;
  int ret;

  if (client == NULL || prompt_type == NULL || context_json == NULL ||
      out_reply == NULL)
    {
      return VELAPET_AI_ERR_ARGUMENT;
    }

  memset(out_reply, 0, sizeof(*out_reply));
  context = cJSON_Parse(context_json);
  if (!cJSON_IsObject(context))
    {
      cJSON_Delete(context);
      return VELAPET_AI_ERR_JSON;
    }

  root = cJSON_CreateObject();
  if (root == NULL)
    {
      cJSON_Delete(context);
      return VELAPET_AI_ERR_JSON;
    }

  cJSON_AddStringToObject(root, "device_id", client->config.device_id);
  cJSON_AddStringToObject(root, "prompt_type", prompt_type);
  cJSON_AddStringToObject(root, "message", message == NULL ? "" : message);
  cJSON_AddStringToObject(root, "locale",
                          client->config.locale == NULL ? "zh-CN"
                                                       : client->config.locale);
  cJSON_AddNumberToObject(root, "max_chars", 50);
  cJSON_AddItemToObject(root, "context", context);
  body = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (body == NULL)
    {
      return VELAPET_AI_ERR_JSON;
    }

  ret = join_url(client->config.base_url, "/api/v1/chat", url, sizeof(url));
  if (ret != VELAPET_AI_OK)
    {
      cJSON_free(body);
      return ret;
    }

  response[0] = '\0';
  ret = client->config.post_json(
      url, client->config.device_token, body, response, sizeof(response),
      &http_status, client->config.transport_user_data);
  cJSON_free(body);
  if (ret != 0)
    {
      return VELAPET_AI_ERR_TRANSPORT;
    }
  if (!http_ok(http_status))
    {
      return VELAPET_AI_ERR_HTTP;
    }

  parsed = cJSON_Parse(response);
  if (!cJSON_IsObject(parsed))
    {
      cJSON_Delete(parsed);
      return VELAPET_AI_ERR_JSON;
    }

  copy_text(out_reply->request_id, sizeof(out_reply->request_id),
            json_string(parsed, "request_id"));
  copy_text(out_reply->reply, sizeof(out_reply->reply),
            json_string(parsed, "reply"));
  copy_text(out_reply->intent, sizeof(out_reply->intent),
            json_string(parsed, "intent"));
  copy_text(out_reply->emotion, sizeof(out_reply->emotion),
            json_string(parsed, "emotion"));
  copy_text(out_reply->source, sizeof(out_reply->source),
            json_string(parsed, "source"));
  copy_text(out_reply->error_code, sizeof(out_reply->error_code),
            json_string(parsed, "error_code"));
  reward = cJSON_GetObjectItemCaseSensitive(parsed, "reward");
  if (cJSON_IsObject(reward))
    {
      const cJSON *eligible =
          cJSON_GetObjectItemCaseSensitive(reward, "eligible");
      out_reply->reward_eligible = cJSON_IsTrue(eligible) ? 1 : 0;
      copy_text(out_reply->reward_event_id, sizeof(out_reply->reward_event_id),
                json_string(reward, "event_id"));
    }

  cJSON_Delete(parsed);
  return out_reply->reply[0] != '\0' ? VELAPET_AI_OK : VELAPET_AI_ERR_JSON;
}

int velapet_ai_client_submit_asset(VelapetAiClient *client,
                                  const char *source_path,
                                  const char *style,
                                  const char *asset_kinds,
                                  VelapetAiAssetJob *out_job)
{
  char url[VELAPET_AI_URL_LEN];
  char response[VELAPET_AI_RESPONSE_LEN];
  int http_status = 0;
  int ret;

  if (client == NULL || source_path == NULL || out_job == NULL ||
      client->config.post_image == NULL)
    {
      return VELAPET_AI_ERR_ARGUMENT;
    }

  ret = join_url(client->config.base_url, "/api/v1/pet-assets/jobs", url,
                 sizeof(url));
  if (ret != VELAPET_AI_OK)
    {
      return ret;
    }

  response[0] = '\0';
  ret = client->config.post_image(
      url, client->config.device_token, source_path, client->config.device_id,
      style == NULL ? "soft cartoon mascot" : style,
      asset_kinds == NULL ? "avatar,home,skin" : asset_kinds, response,
      sizeof(response), &http_status, client->config.transport_user_data);
  if (ret != 0)
    {
      return VELAPET_AI_ERR_TRANSPORT;
    }
  if (!http_ok(http_status))
    {
      return VELAPET_AI_ERR_HTTP;
    }
  return parse_asset_job(response, out_job);
}

int velapet_ai_client_get_asset_job(VelapetAiClient *client,
                                   const char *job_id,
                                   VelapetAiAssetJob *out_job)
{
  char path[128];
  char url[VELAPET_AI_URL_LEN];
  char response[VELAPET_AI_RESPONSE_LEN];
  int http_status = 0;
  int ret;

  if (client == NULL || job_id == NULL || out_job == NULL ||
      client->config.get_json == NULL)
    {
      return VELAPET_AI_ERR_ARGUMENT;
    }

  snprintf(path, sizeof(path), "/api/v1/pet-assets/jobs/%s", job_id);
  ret = join_url(client->config.base_url, path, url, sizeof(url));
  if (ret != VELAPET_AI_OK)
    {
      return ret;
    }
  response[0] = '\0';
  ret = client->config.get_json(
      url, client->config.device_token, response, sizeof(response),
      &http_status, client->config.transport_user_data);
  if (ret != 0)
    {
      return VELAPET_AI_ERR_TRANSPORT;
    }
  if (!http_ok(http_status))
    {
      return VELAPET_AI_ERR_HTTP;
    }
  return parse_asset_job(response, out_job);
}

int velapet_ai_client_download_asset(VelapetAiClient *client,
                                    const VelapetAiAsset *asset)
{
  char url[VELAPET_AI_URL_LEN];
  int http_status = 0;
  int ret;

  if (client == NULL || asset == NULL || asset->download_url[0] == '\0' ||
      asset->device_path[0] == '\0' || client->config.download == NULL)
    {
      return VELAPET_AI_ERR_ARGUMENT;
    }

  /* Only accept our three agreed destinations, never an arbitrary server path. */
  if (strcmp(asset->device_path, "/data/agent/assets/pet/avatar.png") != 0 &&
      strcmp(asset->device_path, "/data/agent/assets/pet/home_pet.png") != 0 &&
      strcmp(asset->device_path, "/data/agent/assets/pet/skin_custom.png") != 0)
    return VELAPET_AI_ERR_ARGUMENT;
  if (asset->byte_size <= 0 || asset->byte_size > 1024 * 1024 ||
      strlen(asset->sha256) != 64) return VELAPET_AI_ERR_ARGUMENT;

  if (strncmp(asset->download_url, "http://", 7) == 0 ||
      strncmp(asset->download_url, "https://", 8) == 0)
    {
      size_t n = strlen(client->config.base_url);
      while (n > 0 && client->config.base_url[n - 1] == '/') n--;
      if (strncmp(asset->download_url, client->config.base_url, n) != 0 ||
          asset->download_url[n] != '/') return VELAPET_AI_ERR_ARGUMENT;
      copy_text(url, sizeof(url), asset->download_url);
    }
  else if (join_url(client->config.base_url, asset->download_url, url,
                    sizeof(url)) != VELAPET_AI_OK)
    {
      return VELAPET_AI_ERR_BUFFER;
    }

  ret = client->config.download(
      url, client->config.device_token, asset->device_path, asset->sha256,
      asset->byte_size, &http_status, client->config.transport_user_data);
  if (ret != 0)
    {
      return VELAPET_AI_ERR_TRANSPORT;
    }
  return http_ok(http_status) ? VELAPET_AI_OK : VELAPET_AI_ERR_HTTP;
}
