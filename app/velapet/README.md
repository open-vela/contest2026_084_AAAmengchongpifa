# VelaPet · 会成长的智能手表表宠

面向 **openvela / Apache NuttX** 智能手表的虚拟宠物应用，已在 **SiFli SF32LB52-DevKit-LCD**
真机上完整跑通。

用户通过日常步数、连续运动和签到来养育自己的表宠：完成任务获得经验、升级解锁新皮肤，
表宠则用形象和文字给出反馈。端侧业务核心、LVGL 前端、AI 云端桥接三部分均为本项目实现。

---

## 亮点

- **完整成长闭环，真机验证通过** —— 步数 → 完成任务 → 领取经验 → 升级 → 解锁并切换皮肤，
  整条链路都能在手表上一次走完。
- **五页 LVGL 前端** —— 主页、任务、AI 聊天、个性化形象、皮肤，含经验条、任务卡、
  皮肤仓库、轻提示与升级弹窗，触摸交互完整。
- **真实图片渲染** —— PNG 经 LVGL 解码显示（自带 lodepng + POSIX 文件系统驱动），
  按容器尺寸等比缩放，读图失败自动回退到手绘形象。
- **掉电安全的存档** —— 状态以 cJSON 序列化，写入采用「临时文件 → rename」的原子替换，
  中途断电不会留下半个损坏的存档。
- **对无电池硬件的适配** —— 板子没有 RTC 备用电池，应用在开机时把时钟播种为固件编译时间，
  业务侧的日期逻辑也能容忍时钟回跳，不会出现「复位后永远无法签到」。
- **可测试** —— 业务逻辑与日期逻辑都有主机侧测试，不需要硬件即可回归。

---

## 功能现状

### 已实现（均已在真机验证）

| 模块 | 能力 |
| --- | --- |
| 成长系统 | 等级、经验曲线、称号；升级自动发放奖励 |
| 每日任务 | 步数目标、连续运动、每日签到、好友互动；完成状态与领奖流程 |
| 皮肤系统 | 等级奖励解锁（Lv2 / Lv5 / Lv10）+ 连续签到 3 天解锁；切换前校验解锁状态 |
| 存档 | 原子写 JSON，重启恢复；`/data` 不可用时自动降级并给出提示 |
| 后台服务 | 每分钟刷新任务、定期保存状态 |
| LVGL 前端 | 五个页面 + 共用主题/组件；页面路由与统一刷新入口 |
| 图片显示 | PNG 解码、等比缩放、缓存失效、失败兜底 |
| 聊天页 | 快捷提问 + 加载/回复/错误状态；AI 未接通时给出**基于当前状态的本地回复** |
| 演示输入源 | 自动加步与按键加步，方便无计步器硬件时演示完整闭环（可编译开关关闭） |
| 时钟适配 | 上电播种为固件编译时间；日期逻辑容忍时钟回跳 |
| 测试 | `tests/` 下两个主机测试：业务逻辑、日期/时钟回跳 |

### 未来开发方向

按依赖关系由近及远排列：

1. **接通 AI 云端对话**（价值最高）
   设备端 AI bridge（请求组包、异步工作线程、图片任务轮询、下载调度、UI 线程回填）
   与 Python 后端都已就绪，缺的是设备侧的网络通路 —— 目前只差「连接方案 + 四个
   transport 回调（`post_json` / `post_image` / `get_json` / `download`）」。
   两条可行路线：**加 WiFi 模组**（工作区已有 `netutils/esp8266` 驱动，接入后即可得到
   真正的网络接口，按原设计实现回调即可），或**走 USB 串口桥接**（板子提供了 CDC ACM
   `/dev/ttyACM0`，在 PC 侧做一个代理把帧协议转成 HTTP）。
   一旦接通，个性化鼓励语、AI 生成形象、云端奖励幂等等能力即可启用。

2. **本地状态持久化**
   存档已支持原子写，但目标板上 `/data` 目前挂在 tmpfs（内存盘），复位后进度会丢失。
   NOR 上的持久化分区已定位并验证为空白，但文件系统格式化受限于板级 MTD 几何信息报告
   的问题，尚不可用。已提供 `nsh> velapet mkfs` 作为受控的格式化入口，便于后续排查。

3. **真实步数来源**
   当前用演示输入源代替（板子无计步器）。接入传感器或手机数据通道后，替换调用
   `vela_pet_update_steps()` 的来源即可，业务与 UI 无需改动。

4. **形象资源与缓存刷新**
   皮肤形象资源目前用一张内置图片演示；补齐全套皮肤素材后，配合「新图片覆盖同一路径
   触发缓存刷新」的验证，即可展示完整换肤效果。

5. **稳定性与性能收尾**
   真机内存压力测试，据此调整系统堆、LVGL 堆、绘制缓冲与线程栈；断网、损坏文件、
   反复切页、重复生成等异常路径回归。

---

## 系统架构

```text
        ┌──────────────────────────────┐
        │   LVGL 前端（ui/）            │  主页 / 任务 / 聊天 / 形象 / 皮肤
        └──────────────┬───────────────┘
                       │  唯一正式 ABI：src/vela_pet_api.h（线程安全）
        ┌──────────────┴───────────────┐
        │   端侧业务核心（src/）         │  成长 · 任务 · 奖励 · 皮肤 · 存档 · 后台服务
        └──────────────┬───────────────┘
                       │  AI bridge
        ┌──────────────┴───────────────┐
        │  设备 AI client/bridge        │  请求组包 · 异步线程 · 下载调度
        └──────────────┬───────────────┘
                       │  HTTP（待接通）
        ┌──────────────┴───────────────┐
        │  Python 后端（backend-ai/）    │  聊天 / 图片任务 / 资源分发 / 模型降级
        └──────────────────────────────┘
```

设计要点：

- **单一数据源**：所有业务状态只经过 `src/vela_pet_api.h`，UI、AI、硬件适配都调它，
  不各自复制数据结构。
- **线程分工明确**：LVGL 只能在 LVGL 线程调用；耗时的业务与网络操作放在工作线程，
  通过标志位让 UI 线程刷新。
- **硬件无关**：业务核心不依赖具体传感器，步数/运动/社交都由外部事件驱动，
  换板子只需替换事件来源。

---

## 目录结构

```text
velapet/
├── src/            端侧业务核心、公共 ABI、后台服务、应用入口
├── data/           默认宠物数据
├── tests/          主机侧测试（业务逻辑、日期逻辑）
├── ui/             最新版 LVGL 前端（含模拟器 Mock 与前端文档）
├── backend-ai/     Python 后端、设备端 AI client/bridge、测试与部署文档
└── docs/           设定说明、架构交接、跨模块联调文档
```

核心文件：

- `src/pet.*`：等级、经验、称号、皮肤
- `src/task.*`：每日任务、步数、连续运动、签到、社交
- `src/reward.*`：等级与签到奖励发放
- `src/storage.*`：目录创建、原子写、状态恢复
- `src/json_utils.*`：基于 cJSON 的序列化
- `src/vela_pet_api.*`：供 UI / AI / 硬件调用的线程安全接口
- `src/vela_pet_service.*`：周期刷新与保存
- `src/vela_pet_main.c`：LVGL 应用入口（含 `velapet mkfs` 子命令）
- `src/vela_pet_demo_input.*`：演示用步数源（可用 Kconfig 关闭）

---

## 关键接口

唯一正式业务 ABI 是 `src/vela_pet_api.h`；`ui/src/vela_pet_api.h` 仅供 Mock 构建使用，
不要加进 include 路径。

UI 常用：

```c
vela_pet_init();
PetProfile *pet = vela_pet_get_profile();
Task *tasks = vela_pet_get_tasks(&count);
vela_pet_get_daily_progress();
vela_pet_claim_task(index);
vela_pet_can_checkin();
vela_pet_checkin();
vela_pet_get_current_skin_asset_path();
vela_pet_is_skin_unlocked("sport_blue");
vela_pet_switch_skin("sport_blue");
```

AI 常用：

```c
char *context = vela_pet_get_ai_context();   /* 用完调用 vela_pet_free_ai_context() */
vela_pet_on_ai_encouragement();
vela_pet_get_daily_summary();
```

硬件适配常用：

```c
vela_pet_update_steps(step_delta);
vela_pet_update_exercise_time(minutes);
vela_pet_on_social_interaction();
vela_pet_on_sit_reminder();
```

皮肤切换会校验 `PetProfile.unlocked_skins`，未解锁的皮肤不能直接切换；默认皮肤始终可用。
`current_skin` 与 `unlocked_skins` 统一保存短名（如 `sport_blue`），需要资源路径时调用
`vela_pet_get_skin_asset_path()`。

---

## 构建与运行

在 openvela 工作区中构建（Linux / WSL）：

```bash
cd <openvela>
source build/envsetup.sh

cmake -B cmake_out/sf32lb52_devkit_lcd -S "$PWD/nuttx" -GNinja \
  -DBOARD_CONFIG=../vendor/sifli/boards/sf32lb52/sf32lb52_devkit_lcd/configs/nsh \
  -DEXTRA_FLAGS="-Wno-cpp -Wno-deprecated-declarations"
cmake --build cmake_out/sf32lb52_devkit_lcd
```

产物为 `cmake_out/sf32lb52_devkit_lcd/nuttx.bin`，烧录到 NOR 的 `0x12010000`。
上电后在串口终端（1000000 8N1）执行：

```text
nsh> velapet
```

**详细步骤、烧录工具用法与踩坑记录见 [`docs/project-handoff.md`](docs/project-handoff.md) §5。**

---

## 数据路径

默认为 `/data/agent/memory`（可用 `CONFIG_VELAPET_DATA_PATH` 修改）：

- `/data/agent/memory/pet_profile.json`
- `/data/agent/memory/task_log.json`

目标板上 `/data` 由板级 bring-up 挂载：优先尝试 NOR 上的文件系统，失败则退回 tmpfs，
并创建 `/data/agent/memory` 与 `/data/agent/assets/pet`。当前实际落在 tmpfs，
**复位后进度会丢失**（原因与后续排查入口见 `docs/project-handoff.md` §11）。

---

## 测试

两个测试都在主机上运行，不需要硬件：

```sh
CJ=../netutils/cjson/cJSON
gcc -Wall -Wextra -Isrc -I"$CJ" \
    tests/velapet_logic_test.c \
    src/pet.c src/task.c src/reward.c src/json_utils.c src/storage.c \
    "$CJ/cJSON.c" -lpthread -lm -o /tmp/velapet_logic_test
/tmp/velapet_logic_test
```

- `tests/velapet_logic_test.c`：步数、运动、签到、社交任务与升级
- `tests/velapet_date_test.c`：同一日期、隔天签到、时钟回跳

---

## 奖励规则

- 今日 3000 步：20 EXP
- 连续运动 10 分钟：30 EXP
- 好友碰一碰：15 EXP
- 每日签到：10 EXP
- AI 鼓励触发：3 EXP
- Level 2 / 5 / 10 解锁 `sport_blue` / `flame_red` / `star_purple`
- 连续签到 3 天解锁 `rainbow`

完整设定（经验曲线、称号、任务参数、字段说明）见 [`docs/settings.md`](docs/settings.md)。

---

## 文档索引

| 文档 | 内容 |
| --- | --- |
| [`docs/project-handoff.md`](docs/project-handoff.md) | **首要入口**：模块边界、构建与烧录、真机验证现状、待办与已知限制 |
| [`docs/settings.md`](docs/settings.md) | 成长、任务、奖励的完整数值设定 |
| [`docs/integration/frontend.md`](docs/integration/frontend.md) | 业务与 AI bridge 前端对接 |
| [`docs/integration/ai-cloud.md`](docs/integration/ai-cloud.md) | AI 云端协议问答 |
| [`docs/integration/ui-image.md`](docs/integration/ui-image.md) | 真机图片、盘符与内存适配 |
| [`ui/README.md`](ui/README.md) | LVGL 前端的独立（模拟器）构建方式 |
| [`backend-ai/README.md`](backend-ai/README.md) | Python 后端与设备端桥接 |
