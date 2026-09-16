# VelaPet 设定与联调说明

本文档列出当前端侧业务逻辑中的所有游戏化设定，包括表宠默认状态、经验曲线、任务、奖励、皮肤、称号、数据字段和前端页面建议。其他组员如果要改数值或设计页面，可以按这里找到对应文件。

## 1. 表宠默认设定

当前默认表宠用于首次启动、存档损坏恢复、无本地数据时初始化。

| 设定项 | 当前值 | 修改位置 |
|---|---:|---|
| 表宠名称 | `Vela` | `src/pet.c` 的 `pet_create_default()`；默认 JSON 在 `data/default_pet.json` |
| 初始等级 | `1` | `src/pet.c` 的 `pet_create_default()`；默认 JSON 在 `data/default_pet.json` |
| 初始经验 | `0` | `src/pet.c` 的 `pet_create_default()`；默认 JSON 在 `data/default_pet.json` |
| 初始升级所需经验 | `100` | `src/pet.c` 的 `pet_exp_required()` |
| 初始皮肤 | `default` | `src/pet.c` 的 `pet_create_default()`；默认 JSON 在 `data/default_pet.json` |
| 初始已解锁皮肤 | 默认皮肤 1 个 | `src/pet.c` 的 `pet_create_default()`；默认 JSON 在 `data/default_pet.json` |
| 默认性格 | `cheerful` | `src/pet.c` 的 `pet_create_default()`；默认 JSON 在 `data/default_pet.json` |
| 初始累计步数 | `0` | `src/pet.c` 的 `pet_create_default()`；默认 JSON 在 `data/default_pet.json` |
| 初始连续签到天数 | `0` | `src/pet.c` 的 `pet_create_default()`；默认 JSON 在 `data/default_pet.json` |

前端主页需要展示：

- 名称：`PetProfile.name`
- 等级：`PetProfile.level`
- 当前经验：`PetProfile.exp`
- 升级所需经验：`PetProfile.exp_to_next`
- 经验条百分比：`exp / exp_to_next`
- 当前皮肤 ID：`PetProfile.current_skin`
- 当前皮肤资源路径：调用 `vela_pet_get_current_skin_asset_path()`
- 已解锁皮肤列表：`PetProfile.unlocked_skins`
- 称号：调用 `pet_get_title(level)` 或使用 AI context 里的 `title`
- 累计步数：`PetProfile.total_steps`
- 连续签到：`PetProfile.consecutive_days`

## 2. 经验曲线

当前升级经验规则在 `src/pet.c` 的 `pet_exp_required(int level)` 中。

| 等级区间 | 当前规则 |
|---|---|
| 1 升 2 | `100 EXP` |
| 2 级及以后 | `level * 100 + 50` |

示例：

| 当前等级 | 升到下一级需要 |
|---:|---:|
| 1 | 100 |
| 2 | 250 |
| 3 | 350 |
| 4 | 450 |
| 5 | 550 |

要改升级速度，修改 `src/pet.c`：

```c
int pet_exp_required(int level)
```

## 3. 称号设定

当前称号在 `src/pet.c` 的 `pet_get_title(int level)` 中。

| 等级 | 称号 ID | 建议中文展示 |
|---|---|---|
| 1-5 | `egg_baby` | 蛋宝宝 |
| 6-10 | `young_pet` | 幼崽 |
| 11-20 | `sport_star` | 运动新星 |
| 21+ | `vela_legend` | Vela 传说 |

如果前端需要中文文案，可以在 UI 层做 ID 到中文的映射，或者把 `pet_get_title()` 的返回值改为中文字符串。

## 4. 每日任务设定

每日任务在 `src/task.c` 的 `task_log_refresh_daily()` 中创建。当前每天最多 4 个任务。

| 任务类型 | 类型枚举 | 描述 | 目标 | 奖励经验 | 进度来源 | 前端页面/组件 |
|---|---|---|---:|---:|---|---|
| 每日步数 | `TASK_TYPE_DAILY_STEPS` | `Walk 3000 steps today` | 3000 步 | 20 EXP | 硬件调用 `vela_pet_update_steps(step_delta)` | 今日任务卡、步数进度条 |
| 连续运动 | `TASK_TYPE_CONTINUOUS_WALK` | `Walk continuously for 10 minutes` | 10 分钟 | 30 EXP | 硬件调用 `vela_pet_update_exercise_time(minutes)` | 运动任务卡、计时/分钟进度 |
| 每日签到 | `TASK_TYPE_CHECKIN` | `Daily check-in` | 1 次 | 10 EXP | UI 调用 `vela_pet_checkin()` | 签到按钮、连续天数展示 |
| 好友碰一碰 | `TASK_TYPE_SOCIAL` | `Tap with a friend` | 1 次 | 15 EXP | 社交模块调用 `vela_pet_on_social_interaction()` | 社交反馈页/弹窗 |

要修改任务数量、描述、目标值、奖励经验，修改 `src/task.c`：

```c
set_task(&log->tasks[0], TASK_TYPE_DAILY_STEPS, "Walk 3000 steps today", 3000, 20);
set_task(&log->tasks[1], TASK_TYPE_CONTINUOUS_WALK, "Walk continuously for 10 minutes", 10, 30);
set_task(&log->tasks[2], TASK_TYPE_CHECKIN, "Daily check-in", 1, 10);
set_task(&log->tasks[3], TASK_TYPE_SOCIAL, "Tap with a friend", 1, 15);
```

如果要增加任务：

- `src/task.h` 中 `VELAPET_MAX_TASKS` 当前是 `8`。
- `TaskType` 枚举需要增加新类型。
- `task_log_refresh_daily()` 中增加 `set_task()`。
- 硬件/UI/AI 侧需要调用对应的进度更新接口，或新增 API。

## 5. 任务状态

任务状态定义在 `src/task.h`。

| 状态枚举 | 含义 | 前端建议 |
|---|---|---|
| `TASK_STATUS_NOT_STARTED` | 未开始 | 灰色/未激活任务 |
| `TASK_STATUS_IN_PROGRESS` | 进行中 | 显示进度条 |
| `TASK_STATUS_COMPLETED` | 已完成但未领奖 | 显示“领取”按钮和完成反馈 |
| `TASK_STATUS_REWARD_CLAIMED` | 已领取奖励 | 显示已完成/已领取 |

前端领取任务奖励调用：

```c
int exp = vela_pet_claim_task(task_index);
```

返回值 `> 0` 表示成功获得经验。

签到按钮建议先调用：

```c
int can = vela_pet_can_checkin();
```

返回 `1` 表示今天还没签到，可以点亮按钮；返回 `0` 表示今天已经签过，按钮应置灰或显示“今日已签到”。

执行签到调用：

```c
int days = vela_pet_checkin();
```

返回 `> 0` 表示本次签到成功，返回值是当前连续签到天数；返回 `0` 表示今天已经签到过，不能重复获得签到经验，也不会重复推进签到任务。

## 6. 奖励与皮肤设定

等级奖励在 `src/reward.c` 的 `g_level_rewards[]` 中。

| 触发等级 | 奖励类型 | 奖励名称 | 皮肤路径 | 前端设计需求 |
|---:|---|---|---|---|
| Level 2 | `skin` | `sport_blue` | `/data/agent/assets/pet/sport_blue` | 运动蓝皮肤素材、解锁弹窗 |
| Level 5 | `skin` | `flame_red` | `/data/agent/assets/pet/flame_red` | 火焰红皮肤素材、解锁弹窗 |
| Level 10 | `skin` | `star_purple` | `/data/agent/assets/pet/star_purple` | 星辰紫皮肤素材、解锁弹窗 |

连续签到奖励在 `src/task.c` 的 `task_checkin()` 中。

| 触发条件 | 奖励类型 | 奖励名称 | 皮肤路径 | 前端设计需求 |
|---|---|---|---|---|
| 连续签到 3 天 | `skin` | `rainbow` | `/data/agent/assets/pet/rainbow` | 彩虹皮肤素材、签到奖励弹窗 |

要修改等级奖励，修改 `src/reward.c`：

```c
static LevelReward g_level_rewards[] = { ... };
```

要修改连续签到奖励，修改 `src/task.c`：

```c
if (log->consecutive_days == 3)
```

以及下面的 `reward_name` 和 `reward_data`。

当前代码已经维护“已解锁皮肤列表”：

- 字段：`PetProfile.unlocked_skins`
- 数量：`PetProfile.unlocked_skin_count`
- 上限：`VELAPET_MAX_SKINS`，当前为 `8`
- 默认皮肤始终可用。
- 奖励皮肤触发后会追加到 `unlocked_skins`，并自动切换为该皮肤。
- `pet_change_skin()` 会校验皮肤是否已解锁，未解锁皮肤会返回失败。

如果前端要做皮肤仓库页面，应读取 `vela_pet_get_profile()` 返回快照中的 `unlocked_skins` 和 `current_skin`。这两个字段统一保存皮肤短名，例如 `sport_blue`；图片资源路径通过 `vela_pet_get_skin_asset_path("sport_blue")` 获取。

## 7. 额外经验来源

除任务领奖外，还有一些事件会直接加经验。

| 行为 | 调用接口 | 当前奖励 | 修改位置 |
|---|---|---:|---|
| UI 签到成功后额外奖励 | `vela_pet_checkin()` | 5 EXP | `src/vela_pet_api.c` 的 `vela_pet_checkin()` |
| AI 鼓励语触发 | `vela_pet_on_ai_encouragement()` | 3 EXP | `src/vela_pet_api.c` 的 `vela_pet_on_ai_encouragement()` |
| 好友碰一碰互动 | `vela_pet_on_social_interaction()` | 15 EXP | `src/vela_pet_api.c` 的 `vela_pet_on_social_interaction()` |

注意：好友碰一碰当前有两层奖励：

- 社交任务完成后，用户领奖可获得 `15 EXP`。
- `vela_pet_on_social_interaction()` 事件本身也会立即给 `15 EXP`。

如果产品上不想重复奖励，可以删掉 `src/vela_pet_api.c` 中 `vela_pet_on_social_interaction()` 里的直接加经验逻辑，只保留任务领奖。

## 8. 数据文件与字段

本地数据默认存储在 `/data/agent/memory`，配置在 `src/storage.h`。

| 文件 | 用途 | 修改位置 |
|---|---|---|
| `/data/agent/memory/pet_profile.json` | 表宠状态 | `src/pet.c`、`src/json_utils.c` |
| `/data/agent/memory/task_log.json` | 任务状态 | `src/task.c`、`src/json_utils.c` |
| `data/default_pet.json` | 默认表宠配置参考 | `data/default_pet.json` |

`PetProfile` 字段定义在 `src/pet.h`：

| 字段 | UI/AI 用途 |
|---|---|
| `name` | 表宠名称 |
| `level` | 等级展示 |
| `exp` | 当前经验 |
| `exp_to_next` | 经验条最大值 |
| `current_skin` | 当前皮肤 ID，例如 `default`、`sport_blue` |
| `personality` | AI 个性化回复参考 |
| `total_steps` | 累计步数展示 |
| `consecutive_days` | 连续签到展示 |
| `last_active` | AI/提醒策略参考 |
| `last_task_refresh` | 任务刷新状态 |
| `unlocked_skins` | 已解锁皮肤 ID 列表，供皮肤仓库页使用 |
| `unlocked_skin_count` | 已解锁皮肤数量 |

`Task` 字段定义在 `src/task.h`：

| 字段 | UI/AI 用途 |
|---|---|
| `type` | 判断任务类型和图标 |
| `description` | 任务文案 |
| `target_value` | 目标值 |
| `current_progress` | 当前进度 |
| `status` | 按钮状态和视觉状态 |
| `exp_reward` | 奖励经验展示 |
| `deadline` | 截止时间展示 |

## 9. 前端页面建议

前端组员至少需要参考以下页面/组件。

| 页面/组件 | 需要的数据/API | 设计重点 |
|---|---|---|
| 表宠主页 | `vela_pet_get_profile()`、`vela_pet_get_daily_progress()` | 皮肤、等级、称号、经验条、今日进度 |
| 今日任务页 | `vela_pet_get_tasks(&count)` | 任务列表、进度条、状态、奖励经验 |
| 任务完成/领奖弹窗 | `vela_pet_claim_task(index)` | 完成反馈、获得经验、升级提示 |
| 签到页/签到按钮 | `vela_pet_checkin()` | 连续签到天数、3 天彩虹皮肤奖励 |
| 皮肤切换页 | `vela_pet_get_profile()`、`vela_pet_is_skin_unlocked()`、`vela_pet_get_skin_asset_path()`、`vela_pet_switch_skin(skin_name)` | 当前皮肤、已解锁状态、皮肤资源预览 |
| 升级提示弹窗 | `vela_pet_get_levelup_message()` | 新等级、新称号、新皮肤奖励 |
| 社交碰一碰反馈 | `vela_pet_on_social_interaction()` | 好友互动成功、社交任务进度 |
| AI 提醒/鼓励展示 | `vela_pet_get_ai_context()`、`vela_pet_get_daily_summary()` | 个性化运动提醒、任务摘要 |
| 久坐提醒反馈 | `vela_pet_on_sit_reminder()` | 提醒动效或震动后的提示 |

前端资源需要至少准备：

- 默认皮肤：`/data/agent/assets/pet/default`
- 运动蓝：`/data/agent/assets/pet/sport_blue`
- 火焰红：`/data/agent/assets/pet/flame_red`
- 星辰紫：`/data/agent/assets/pet/star_purple`
- 彩虹：`/data/agent/assets/pet/rainbow`

## 10. AI 模块可用上下文

AI 模块调用：

```c
char *context = vela_pet_get_ai_context();
vela_pet_free_ai_context(context);
```

返回 JSON 包含：

| 字段 | 含义 |
|---|---|
| `level` | 当前等级 |
| `exp` | 当前经验 |
| `exp_to_next` | 升级经验 |
| `title` | 当前称号 ID |
| `skin` | 当前皮肤 ID，兼容旧字段名 |
| `current_skin` | 当前皮肤 ID |
| `current_skin_asset_path` | 当前皮肤资源路径 |
| `unlocked_skins` | 已解锁皮肤 ID 列表 |
| `total_steps` | 累计步数 |
| `consecutive_days` | 连续签到天数 |
| `daily_progress` | 今日任务总体进度百分比 |
| `tasks[]` | 今日任务列表 |

AI 模块也可以调用：

```c
const char *summary = vela_pet_get_daily_summary();
```

用于生成运动提醒、鼓励语、任务提醒。

## 11. 硬件/传感器组联调点

| 事件 | 调用接口 | 影响 |
|---|---|---|
| 步数增加 | `vela_pet_update_steps(step_delta)` | 更新累计步数和每日步数任务 |
| 连续运动分钟数增加 | `vela_pet_update_exercise_time(minutes)` | 更新连续运动任务 |
| 碰一碰成功 | `vela_pet_on_social_interaction()` | 更新社交任务并给互动经验 |
| 久坐提醒触发 | `vela_pet_on_sit_reminder()` | 更新活跃时间，可供 UI/AI 做提醒 |

## 12. 常见修改入口速查

| 想修改的内容 | 文件 | 函数/位置 |
|---|---|---|
| 默认名字/皮肤/性格 | `src/pet.c`、`data/default_pet.json` | `pet_create_default()` |
| 已解锁皮肤上限 | `src/pet.h` | `VELAPET_MAX_SKINS` |
| 皮肤解锁和切换校验 | `src/pet.c`、`src/reward.c` | `pet_unlock_skin()`、`pet_change_skin()`、`reward_apply()` |
| 升级经验曲线 | `src/pet.c` | `pet_exp_required()` |
| 等级称号 | `src/pet.c` | `pet_get_title()` |
| 每日任务内容 | `src/task.c` | `task_log_refresh_daily()` |
| 任务状态枚举 | `src/task.h` | `TaskStatus` |
| 任务类型枚举 | `src/task.h` | `TaskType` |
| 等级奖励皮肤 | `src/reward.c` | `g_level_rewards[]` |
| 连续签到奖励 | `src/task.c` | `task_checkin()` |
| AI 鼓励经验 | `src/vela_pet_api.c` | `vela_pet_on_ai_encouragement()` |
| 社交互动经验 | `src/vela_pet_api.c` | `vela_pet_on_social_interaction()` |
| 签到额外经验 | `src/vela_pet_api.c` | `vela_pet_checkin()` |
| 存储路径 | `src/storage.h` | `CONFIG_VELAPET_DATA_PATH` / `VELAPET_MEMORY_DIR` |
| 后台保存间隔 | `src/vela_pet_service.c` | `sleep(60)` |

## 13. 部署前提

VelaPet 默认把存档写到 `/data/agent/memory`，所以 `/data` 必须已挂载，否则存档无法写入。

**目标板现状（2026-09-14）**：`/data` 由板级 bring-up 挂载（`vendor/sifli/boards/sf32lb52/sf32lb52_devkit_lcd/src/sifli_ap.c`）：
先尝试用 littlefs 挂 NOR 上的数据分区，失败则退回 **tmpfs**，并创建
`/data/agent/memory` 与 `/data/agent/assets/pet`。目前实际落到 tmpfs —— **复位后进度会丢失**。
原因和后续排查入口见 `project-handoff.md` §11。

如果 `/data` 完全不可用，`vela_pet_restore_state()` 会打印错误并继续用默认数据：

```text
ERROR: VelaPet data path is unavailable: /data/agent/memory. Check that /data is mounted.
```
