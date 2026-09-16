#ifndef TEST_CJSON_H
#define TEST_CJSON_H

#include <stddef.h>

typedef struct cJSON
{
  struct cJSON *next;
  struct cJSON *prev;
  struct cJSON *child;
  int type;
  char *valuestring;
  int valueint;
  double valuedouble;
  char *string;
} cJSON;

cJSON *cJSON_Parse(const char *value);
void cJSON_Delete(cJSON *item);
cJSON *cJSON_CreateObject(void);
cJSON *cJSON_AddStringToObject(cJSON *object, const char *name,
                               const char *value);
cJSON *cJSON_AddNumberToObject(cJSON *object, const char *name,
                               double number);
void cJSON_AddItemToObject(cJSON *object, const char *name, cJSON *item);
cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *object,
                                        const char *name);
char *cJSON_PrintUnformatted(const cJSON *item);
void cJSON_free(void *object);
int cJSON_IsObject(const cJSON *item);
int cJSON_IsString(const cJSON *item);
int cJSON_IsNumber(const cJSON *item);
int cJSON_IsTrue(const cJSON *item);

#define cJSON_ArrayForEach(element, array)                                  \
  for ((element) = ((array) != NULL) ? (array)->child : NULL;               \
       (element) != NULL; (element) = (element)->next)

#endif

