#ifndef VELAPET_STORAGE_H
#define VELAPET_STORAGE_H

#include "pet.h"
#include "task.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_VELAPET_DATA_PATH
#define CONFIG_VELAPET_DATA_PATH "/data/agent/memory"
#endif

#define VELAPET_MEMORY_DIR CONFIG_VELAPET_DATA_PATH
#define VELAPET_PET_PROFILE_PATH VELAPET_MEMORY_DIR "/pet_profile.json"
#define VELAPET_TASK_LOG_PATH VELAPET_MEMORY_DIR "/task_log.json"

int storage_ensure_dir(const char *path);
int storage_read_all(const char *path, char **out_buffer);
int storage_write_all(const char *path, const char *data, size_t len);
int vela_pet_restore_state(PetProfile **pet, TaskLog **task);
int vela_pet_save_state(PetProfile *pet, TaskLog *task);

#ifdef __cplusplus
}
#endif

#endif
