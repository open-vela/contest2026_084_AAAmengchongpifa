# VelaPet 组员三前端接口说明

## 1. 前端对外入口

```c
int velapet_ui_init(void);
void velapet_ui_show_home(void);
void velapet_ui_show_task(void);
void velapet_ui_show_chat(void);
void velapet_ui_show_pet_image(void);
void velapet_ui_show_skin(void);
void velapet_ui_refresh_all(void);
```

主程序启动 LVGL 后调用 `velapet_ui_init()`。其他模块发生数据变化时调用 `velapet_ui_refresh_all()`。

## 2. 业务数据依赖

主页读取：

```c
PetProfile *vela_pet_get_profile(void);
int vela_pet_get_daily_progress(void);
```

展示字段：`name`、`level`、`exp`、`exp_to_next`、`current_skin`、`total_steps`、`consecutive_days`。

任务页读取：

```c
Task *vela_pet_get_tasks(int *count);
int vela_pet_claim_task(int task_index);
int vela_pet_checkin(void);
const char *vela_pet_get_levelup_message(void);
```

展示字段：`description`、`target_value`、`current_progress`、`status`、`exp_reward`。

皮肤页读取：

```c
PetProfile *vela_pet_get_profile(void);
int vela_pet_switch_skin(const char *skin_name);
```

展示字段：`current_skin`、`unlocked_skins`、`unlocked_skin_count`。

## 3. AI 聊天预留接口

```c
void velapet_ui_chat_set_reply(const char *reply);
void velapet_ui_chat_set_loading(int loading);
void velapet_ui_chat_set_error(const char *error_msg);
```

组员四的 AI 后端返回文本后调用 `velapet_ui_chat_set_reply()`。请求中调用 `velapet_ui_chat_set_loading(1)`，请求完成后调用 `velapet_ui_chat_set_loading(0)`。

## 4. 个性化形象预留接口

```c
void velapet_ui_pet_image_set_state(int state, const char *asset_path);
```

状态建议：

- `0`：默认形象。
- `1`：生成中。
- `2`：加载成功。
- `3`：加载失败，使用默认形象。

资源路径建议统一为：

```text
/data/agent/assets/pet/default
/data/agent/assets/pet/avatar.png
/data/agent/assets/pet/home.png
/data/agent/assets/pet/skin_sport.png
```
