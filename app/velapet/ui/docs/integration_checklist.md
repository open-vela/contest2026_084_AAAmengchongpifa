# 组员三对接清单

## 项目负责人

- 确认 `velapet_ui_init()` 在主流程中的调用位置。
- 确认主页作为应用入口。
- 确认新增 UI 源文件已加入工程编译。

## 组员一

- 提供屏幕分辨率和触摸驱动可用状态。
- 确认按钮尺寸、滚动列表和页面切换在真机上可用。
- 确认后续图片资源可被 LVGL 解码显示。

## 组员二

需要提供：

```c
int vela_pet_init(void);
PetProfile *vela_pet_get_profile(void);
Task *vela_pet_get_tasks(int *count);
int vela_pet_get_daily_progress(void);
int vela_pet_claim_task(int task_index);
int vela_pet_checkin(void);
int vela_pet_switch_skin(const char *skin_name);
const char *vela_pet_get_levelup_message(void);
```

验收点：

- 主页能显示等级、经验、皮肤和今日进度。
- 任务页能显示任务列表并领奖。
- 签到后连续天数刷新。
- 皮肤切换后主页和皮肤页刷新。

## 组员四

需要对接：

```c
void velapet_ui_chat_set_reply(const char *reply);
void velapet_ui_chat_set_loading(int loading);
void velapet_ui_chat_set_error(const char *error_msg);
void velapet_ui_pet_image_set_state(int state, const char *asset_path);
```

验收点：

- AI 回复不超过小屏显示长度，建议 50 字以内。
- 图片资源同步到 `/data/agent/assets/pet/`。
- 加载失败时能通知 UI 显示默认形象。
