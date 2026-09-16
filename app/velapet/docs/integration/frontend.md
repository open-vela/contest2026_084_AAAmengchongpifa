# VelaPet 端侧给前端对接说明

## 1. 文档目的

本文档说明 VelaPet LVGL 前端与端侧核心、AI bridge 的正式对接方式。本文以当前工作区代码为准；旧版《VelaPet 组员三给组员四预留接口说明文档》中“聊天按钮仍使用 mock”“AI 请求接口尚未接入”等描述已经不再适用于正式构建。

## 2. 本次已完成的修改

本次修改了以下前端文件：

- `VelaPet_member3_interface_adapted_checked/member3/src/vela_pet_api.h`
  - 删除前端重复定义的 `PetProfile`、`Task` 和 `TaskStatus`。
  - 改为转发引用端侧核心正式头文件。
- `member3/src/ui/ui_chat.c`
  - 正式构建下，三个快捷按钮改为调用 `velapet_ai_request_reply()`。
  - AI 回复缓存由 160 字节调整为 256 字节。
  - 异步返回时只有当前处于 Chat 页面才更新 LVGL 控件，切页后仅保留缓存。
- `member3/src/ui/ui_pet_image.c`
  - 异步图片结果只有当前处于 Pet Image 页面才更新控件，降低切页后访问已删除控件的风险。
- `member3/src/ui/ui_skin.c`
  - 按端侧定长数组字段修正 `current_skin` 和 `unlocked_skins` 的读取方式。

## 3. 唯一正式业务 ABI

正式 ABI 只有一份：

```text
apps/examples/velapet/src/vela_pet_api.h
```

该文件引用真实的 `pet.h` 和 `task.h`。正式构建中禁止再次定义 `PetProfile`、`Task` 或 `TaskStatus`，否则即使函数能够链接，结构体字段偏移仍可能错误，导致乱码、越界或崩溃。

前端目录中的 `src/vela_pet_api.h` 现在只是当前工作区内的兼容转发文件，不是第二份 ABI。若把 UI 文件复制进正式主工程，应让 UI 直接通过 include path 找到端侧 `src/vela_pet_api.h`。

## 4. 正式构建与 Mock 构建

静态演示可以启用：

```c
VELAPET_UI_USE_MOCK
```

真实联调必须关闭该宏，同时关闭前端 CMake 选项：

```cmake
VELAPET_UI_BUILD_MOCK=OFF
```

Mock 结构体是简化版本，只适合页面演示，不能用于验证真实 ABI。

## 5. PetProfile 正式字段

前端通过 `vela_pet_get_profile()` 获取只读快照。主要字段如下：

| 字段 | 类型/含义 | 前端用途 |
| --- | --- | --- |
| `name` | 定长字符数组 | 宠物名称 |
| `level` | `int` | 当前等级 |
| `exp` | `int` | 当前经验 |
| `exp_to_next` | `int` | 当前级升级所需经验 |
| `current_skin` | 定长字符数组 | 当前皮肤短 ID |
| `personality` | 定长字符数组 | AI 性格上下文 |
| `total_steps` | `int` | 累计步数 |
| `consecutive_days` | `int` | 连续签到天数 |
| `last_active` | `time_t` | 最近活跃时间 |
| `last_task_refresh` | `time_t` | 最近任务刷新时间 |
| `unlocked_skins` | 二维定长字符数组 | 已解锁皮肤 ID |
| `unlocked_skin_count` | `int` | 有效皮肤数量 |

返回值是端侧维护的共享快照，前端不得修改或长期保存其地址。建议在一次 UI 刷新函数中读取并立即完成控件更新。

## 6. Task 正式字段

前端通过 `vela_pet_get_tasks(&count)` 获取任务快照：

| 字段 | 用途 |
| --- | --- |
| `type` | 任务类型及图标选择 |
| `description` | 任务文案，定长字符数组 |
| `target_value` | 目标值 |
| `current_progress` | 当前进度 |
| `status` | 未开始、进行中、待领奖、已领奖 |
| `exp_reward` | 奖励经验 |
| `deadline` | 当日截止时间 |

任务数组同样是共享快照，不能由 UI 修改或跨刷新周期长期持有。

## 7. AI 聊天对接

正式构建下快捷按钮分别调用：

```c
velapet_ai_request_reply("cheer");
velapet_ai_request_reply("task");
velapet_ai_request_reply("level");
```

返回 `0` 表示工作线程已经启动；返回非零表示 bridge 未初始化或当前已有请求。一次只允许一个 AI/图片任务。

AI 结果由 bridge 在 LVGL 线程中回填：

```c
velapet_ui_chat_set_loading(1);
velapet_ui_chat_set_reply(reply);
velapet_ui_chat_set_error(message);
velapet_ui_chat_set_loading(0);
```

前端不应在按钮回调中直接访问 HTTP，也不应从网络线程操作 LVGL。

## 8. LVGL 线程安全刷新

在 LVGL/UI 线程创建约 100 ms 的定时器，并在回调中调用：

```c
velapet_ai_bridge_poll_ui();
```

推荐流程：

```text
UI按钮 → 启动AI工作线程 → 网络线程写结果缓存
       → LVGL定时器poll → UI回填接口 → 更新控件
```

销毁应用时，应先停止 LVGL poll 定时器，再调用 `velapet_ai_bridge_deinit()`，最后销毁 UI 和端侧业务状态。普通页面返回不应调用 bridge deinit。

## 9. 图片资源刷新

AI 侧约定下载三个文件：

```text
/data/agent/assets/pet/avatar.png
/data/agent/assets/pet/home_pet.png
/data/agent/assets/pet/skin_custom.png
```

当前 `ui_pet_image.c` 已能接收 READY/ERROR 状态和路径，但还没有真正创建 `lv_img` 加载 PNG。前端下一步需要：

1. 确认 LVGL 已启用 PNG 解码器。
2. 确认 `/data` 对应的 LVGL 文件系统驱动及盘符。
3. 把 POSIX 路径转换为 LVGL 可识别路径。
4. 在 LVGL 线程使旧图片缓存失效。
5. 调用 `lv_img_set_src()` 显示新文件。
6. 解码失败时保留旧图或默认图。

`skin_custom.png` 当前只是个性化形象文件，还没有进入端侧皮肤解锁和持久化体系，不能直接写入 `current_skin`。

## 10. 联调验收清单

- 正式构建未定义 `VELAPET_UI_USE_MOCK`。
- UI 只引用端侧唯一 ABI。
- 主页正确显示 `Vela`、等级、经验和当前皮肤。
- 任务页正确读取真实 `Task` 数组和任务跨度。
- 三个聊天按钮会产生真实后端请求。
- 请求期间切换页面不会崩溃，返回页面后仍能显示缓存结果。
- 图片下载完成后由 LVGL 线程刷新，不由网络线程直接刷新。
- 同一路径图片更新后能够清除旧缓存并显示新内容。

## 11. 当前未完成项

- 主工程尚未创建 `velapet_ai_bridge_poll_ui()` 的 LVGL 定时器。
- 图片页尚未真正显示 PNG。
- 各页面销毁时的静态控件指针还建议进一步统一清空。
- 真机 PNG 解码、文件系统盘符和图片缓存 API 版本仍需前端与主工程共同确认。

