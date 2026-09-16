#ifndef VELAPET_JSON_UTILS_H
#define VELAPET_JSON_UTILS_H

#include "pet.h"
#include "task.h"

#if defined(__has_include)
#if __has_include(<cJSON.h>)
#include <cJSON.h>
#elif __has_include(<cjson/cJSON.h>)
#include <cjson/cJSON.h>
#else
#include "cJSON.h"
#endif
#else
#include <cJSON.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

cJSON *json_load_from_file(const char *path);
int json_save_to_file(const char *path, cJSON *root);
void json_free(cJSON *root);
PetProfile *pet_from_json(cJSON *root);
cJSON *pet_to_json(const PetProfile *pet);
TaskLog *task_from_json(cJSON *root);
cJSON *task_to_json(const TaskLog *log);

#ifdef __cplusplus
}
#endif

#endif
