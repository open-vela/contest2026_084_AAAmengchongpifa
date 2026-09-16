# VelaPet 工程交接说明

> 本文档是了解本项目的首要入口。最近一次更新：2026-09-14（真机联调后）。
> 根目录的 `project-handoff.md` 只是指向本文的占位。

## 1. 交付范围

```text
端侧核心（src、data、tests）
        ↕ 正式 C ABI（src/vela_pet_api.h）
LVGL 前端（ui）
        ↕ AI bridge（backend-ai/device）
设备端 AI client/bridge + Python 后端（backend-ai）
```

代码已经**接进 openvela 工程并能在 SF32LB52-DevKit-LCD 真机上运行**，不再是模块级代码汇总。
应用以标准形式放在 `apps/examples/velapet`，由 `apps/examples/CMakeLists.txt` 的
`nuttx_add_subdirectory()` 自动发现，**不需要改动 apps 仓库的其他文件**。

本工程不保存 Python 虚拟环境、构建缓存或历史压缩包。本地 `.env` 已由 `.gitignore` 排除，
分发时不得包含。

## 2. 模块与负责人边界

| 模块 | 主要职责 | 当前状态 |
| --- | --- | --- |
| 端侧核心 | 宠物成长、任务、签到、奖励、皮肤、本地存档、业务 API | 已实现，主机测试通过，真机验证通过 |
| LVGL 前端 | 页面、交互、图片显示、缓存刷新、失败回退 | **已在真机运行**（显示、触摸正常） |
| AI device client/bridge | 请求组包、异步工作线程、图片任务轮询、下载调度、UI 线程回填 | 代码已编入固件，但**未初始化**（缺 transport，见 §10） |
| Python 后端 | 聊天、图片任务、资源下载接口、模型适配与降级 | 代码及测试已提供 |
| 主工程/硬件适配 | 网络/TLS、文件系统挂载、LVGL 驱动、PNG 解码、触摸/显示、内存配置 | 显示/触摸/PNG/LVGL 文件系统已接通；**网络与持久化未完成** |

## 3. 唯一正式 ABI

```text
src/vela_pet_api.h          # 唯一正式业务 ABI
src/pet.h  src/task.h       # 数据结构定义
```

正式 UI 和 AI bridge 必须引用这套头文件，禁止自行复制并修改 `PetProfile` 或 `Task`。
`ui/src/vela_pet_api.h` 只供 Mock 构建使用，**不要**把 `ui/src` 加进 include 路径，
否则会和正式 ABI 抢 `<vela_pet_api.h>`。

## 4. 目录说明

### 4.1 端侧核心

- `src/pet.*`：等级、经验、称号、皮肤。
- `src/task.*`：步数、连续运动、签到、社交任务；日期逻辑容忍时钟回跳。
- `src/reward.*`：等级和签到皮肤奖励。
- `src/storage.*`：状态恢复与保存（**临时文件 + rename 的原子写**）。
- `src/json_utils.*`：cJSON 序列化。
- `src/vela_pet_api.*`：供 UI、AI、硬件调用的线程安全接口。
- `src/vela_pet_service.*`：每分钟刷新任务并定期保存。
- `src/vela_pet_main.c`：LVGL 应用入口；另提供 `velapet mkfs` 子命令（见 §11）。
- `src/vela_pet_demo_input.*`：演示用步数源，可用 Kconfig 关闭（见 §8）。
- `data/default_pet.json`：默认宠物状态。
- `tests/velapet_logic_test.c`：业务逻辑测试。
- `tests/velapet_date_test.c`：日期/时钟回跳测试。

### 4.2 LVGL 前端

`ui/` 已支持主页、任务、聊天、个性化形象、皮肤页，以及真实图片显示、缓存失效、
尺寸缩放和失败回退。

**构建方式已变化**：正式固件不再使用 `ui/CMakeLists.txt`（那是给独立模拟器用的），
而是由 `apps/examples/velapet/CMakeLists.txt` 把 `ui/src/ui/*.c` 直接编进应用目标，
通过 `DEPENDS lvgl` 链接 LVGL。UI 源码里原有的 v8 风格 API 依赖 LVGL 自带的
`lv_api_map_v8.h` 兼容层，只有图像解码的几处改成了 v9 名字。

默认形象图由板级 romfs 提供，路径 `/etc/pet/home_pet.png`；UI 里的
`VELAPET_UI_DEFAULT_ASSET`（`ui/src/ui/ui_internal.h`）写的是
`/data/agent/assets/pet/home_pet.png`，会被 `ui_resolve_img_src()` 改写成
`S:/home_pet.png`，再由 LVGL 的 `LV_USE_FS_POSIX`（`S:` → `/etc/pet`）落到 romfs 上。
换形象图：把图缩到 440×274（屏幕形象区 220×176 的 2 倍）存成那个路径，重编重烧。

### 4.3 AI 后端与设备桥接

- `backend-ai/app`：Python API 与模型/资源服务。
- `backend-ai/device/include`：设备端 AI 接口。
- `backend-ai/device/src`：AI client 和异步 bridge（已编入固件）。
- `backend-ai/tests`：Python API 与 C bridge 测试。
- `backend-ai/docs`：部署、协议和联调文档。

后端环境应从 `requirements.txt` 或 `pyproject.toml` 重新创建，不能复制开发者本机 `.venv`。

## 5. 构建与烧录

构建在 **WSL Ubuntu-22.04 的 `/root/openvela`** 工作区里做（Windows 侧没有工具链）。

```bash
cd /root/openvela
source build/envsetup.sh          # 必需：否则 arm-none-eabi-gcc 不在 PATH

cmake -B cmake_out/sf32lb52_devkit_lcd -S "$PWD/nuttx" -GNinja \
  -DBOARD_CONFIG=../vendor/sifli/boards/sf32lb52/sf32lb52_devkit_lcd/configs/nsh \
  -DEXTRA_FLAGS="-Wno-cpp -Wno-deprecated-declarations"
cmake --build cmake_out/sf32lb52_devkit_lcd
```

产物是 `cmake_out/sf32lb52_devkit_lcd/nuttx.bin`，烧到 NOR 的 **`0x12010000`**。

**坑**：改动 `configs/nsh/defconfig` 之后，必须 `rm -rf cmake_out/sf32lb52_devkit_lcd`
再重新配置。只重跑 cmake 会被已有的 `.config` 覆盖，新配置不生效。

烧录（Windows 侧，串口是 COM5 = CH343 USB-UART，1000000 8N1）：

```bash
sftool -c SF32LB52 -m nor -p COM5 -b 1000000 \
  --before default_reset --after soft_reset \
  write_flash nuttx.bin@0x12010000
```

**关键经验**：只有**刚拔插过 USB** 时烧录才可靠（ROM bootloader 的 `ATSF32` 监听窗口
只有约 2 秒）。平时会一直 `Failed to download stub: TimedOut`，换波特率/参数都没用。
如果日志里出现 `nsh: ~ATSF32!~...: command not found`，说明 sftool 在跟运行中的
NuttX 说话、没进 ROM 模式，拔插 USB 再立刻烧即可。

控制台用 `picocom -b 1000000 --noreset --lower-rts --lower-dtr /dev/ttyUSB0`
（RTS 经负载开关控制 SoC 供电，`screen`/`cu` 会把芯片按在复位态）。注意
**`velapet` 是前台内置命令，运行期间 NSH 完全不响应控制台**，这是正常行为，复位即可退出。

## 6. 端云协议摘要

```text
POST /api/v1/chat
POST /api/v1/pet-assets/jobs
GET  /api/v1/pet-assets/jobs/{job_id}
GET  /api/v1/pet-assets/{job_id}/files/{filename}
```

设备请求通过 `X-VelaPet-Token` 鉴权。大模型密钥只保存在服务端，不下发到设备。

设备端允许的三个目标文件：

```text
/data/agent/assets/pet/avatar.png
/data/agent/assets/pet/home_pet.png
/data/agent/assets/pet/skin_custom.png
```

单个文件不超过 1 MiB。下载必须使用临时文件、校验大小和 SHA-256、同步后原子重命名，
失败时保留旧资源。

## 7. 线程与内存

- VelaPet 应用任务栈：**32 KiB**（`CONFIG_EXAMPLES_VELAPET_STACKSIZE`；8 KiB 跑 LVGL 太小）。
- AI bridge 网络工作线程栈：64 KiB。
- 演示步数源工作线程：pthread 默认栈（16 KiB）。
- AI JSON 响应缓冲：8 KiB；AI 文本回复缓存：256 字节。
- 下载必须流式进行，不能把最多 1 MiB 的图片整体放入 RAM。
- **LVGL API 只能在 LVGL 线程调用**。演示步数源的工作线程只碰业务 API 和
  `/dev/buttons`，通过一个 dirty 标志让 LVGL 定时器去刷 UI —— 新增模块请沿用这个分工。

## 8. 演示步数源

板子没有计步器，真实步数要靠外部数据源。为了让「步数 → 完成任务 → 领奖 → 升级 →
解锁皮肤」这条链路在真机上能跑通，加了 `src/vela_pet_demo_input.c`：

- 每 2 秒自动 +100 步（3000 步约一分钟走完）；
- KEY2（本板唯一的按键，PA11）按下 +500 步、+2 分钟连续运动。

用 `CONFIG_EXAMPLES_VELAPET_DEMO_INPUT`（默认 `y`）开关，**产品构建应关闭**，届时所有
模拟逻辑都不编入。注意 `vela_pet_update_steps()` 每次调用都会存档，现在 `/data` 是
tmpfs 无所谓，将来换成 NOR 上的文件系统后这个频率需要考虑擦写磨损。

## 9. 真机验证现状（2026-09-14）

已确认可用：

- 应用作为 NSH 内置命令 `velapet` 启动，LVGL 打开 `/dev/lcd0` 与 `/dev/input0`；
- 主页/任务/聊天/形象/皮肤页显示正常，触摸正常；
- 成长链路完整可用（见 §8）；
- 形象页和主页显示真实 PNG 图片；
- 聊天页快捷按钮返回基于当前等级的本地回复（AI 未接通，见 §11）；
- 上电时钟被播种为固件编译时间（`date` 合理且走时正常）；
- `tests/velapet_logic_test.c` 与 `tests/velapet_date_test.c` 主机侧通过。

## 10. 待办（按当前阻塞状态）

**已完成**

- ~~确认目标板网络栈~~ / ~~实现四个 transport 回调~~ → 见下方阻塞说明
- ~~挂载 `/data` 并创建 `/data/agent/memory`、`/data/agent/assets/pet`~~ → 目录已创建，挂载见 §11
- ~~注册映射资源目录的 LVGL 文件系统驱动~~ → 已用 `LV_USE_FS_POSIX`（`S:` → `/etc/pet`）
- ~~设置 `VELAPET_UI_LVGL_FS_PREFIX`~~ → 默认 `"S:"` 与上面的映射一致
- ~~启用并链接 PNG 解码器~~ → 已用 `LV_USE_LODEPNG`
- ~~调整线程栈~~ → 应用栈提到 32 KiB
- ~~移除 `vela_pet_main.c` 里的演示模拟逻辑~~ → 已挪到可关闭的 demo 输入模块（§8）

**被阻塞**

1. **AI 对话**（原 1、2 项）：`CONFIG_NET` 根本没开，这颗芯片也没有 WiFi/以太网驱动。
   四个 transport 回调（`post_json` / `post_image` / `get_json` / `download`）无从实现。
   蓝牙网络共享（PAN）**已排查确认不可行**，原因见 §11。
   可行方向二选一：**加 WiFi 模组**（apps 里已有 `netutils/esp8266` 驱动，得到真 netdev 后
   按原设计实现 transport），或**走 USB 串口桥接**（板子有 CDC ACM `/dev/ttyACM0`，
   在 PC 上做代理；但需先确认 Windows 侧能枚举出该串口）。
   在接通之前，聊天页使用本地兜底回复（§11）。
2. **`/data` 持久化**（原 4 项）：见 §11。
3. **真实步数来源**：板上无计步器，需要外部数据源（依赖上面的网络方案）。

**待办**

4. 真机内存压力测试，据此调整系统堆、LVGL 堆和绘制缓冲。
5. AI bridge 的初始化与 `velapet_ai_bridge_poll_ui()` 定时器，等 transport 就绪后接入。

## 11. 已知限制

- **`/data` 目前是 tmpfs，复位即丢进度。** NOR 上的文件系统分区已确认在
  `0x008A0000`（4 MiB，原本空白），但 littlefs 还不能用：板级 MTD 把页大小和擦除扇区
  都报成 4096（`chips/sf32lb52/sf32lb_flash.c` 里 `SF32LB_NOR_PAGE_SIZE` 名字叫 PAGE、
  实际是 4 KiB 扇区），littlefs 据此算出 `prog_size == block_size`，会在没有擦除干净的
  块上编程，写 0xFF 到已是 0x00 的位（NOR 物理上做不到），于是写校验失败、格式化与挂载
  全部报 `EIO`。试过把 geometry 拆成 `blocksize=256 / erasesize=4096`，写错误消失了但
  **板子会在挂载时卡死**，已回退。
  为避免格式化失败导致整板起不来，**启动路径不做格式化**，改成从 NSH 手动执行
  `nsh> velapet mkfs`（内部用 littlefs 的 `forceformat` 挂到临时挂载点再卸载），
  失败时错误可见、板子也不会卡死。继续排查请用这个入口。
- **RTC 无备用电池**：每次上电时间不可信，板级已用固件编译时间播种（偏差超过一天才覆盖）。
  跨上电的**真实墙钟做不到**，需要电池或网络授时。`src/task.c` 的日期逻辑已能容忍时钟
  回跳，避免复位后“永远无法签到”。
- AI bridge 已编入固件但**未初始化**（无 transport）。聊天页的三个快捷按钮因此会走
  **本地兜底回复**：`ui_chat.c` 的 `local_ai_reply()` 用 `vela_pet_get_profile()` /
  `vela_pet_get_tasks()` / `vela_pet_get_daily_progress()` 现算一句话，底部提示
  "AI offline, replying locally"。等 transport 就绪后这条兜底会自然退到失败分支之后。
- **蓝牙网络共享（PAN）在当前环境不可行**，不是配置问题：NuttX 侧没有 BNEP 实现
  （全树无 `*bnep*`，`BTPROTO_BNEP` 只定义未使用），厂商层只有 HCI 传输（H4 over UART
  + LCPU 共享内存），BT 主机栈（`external/zblue` + `frameworks/connectivity/bluetooth`）
  在本工作区**未检出**，而且实际构建里 `CONFIG_BT` 并未生效；PAN 还依赖经典蓝牙，
  而现有 BT 栈是 BLE-only。要做需要从零实现 BNEP + PAN netdev。
  可行的联网替代：加 WiFi 模组（apps 里已有 `netutils/esp8266` 驱动），或走 USB 串口桥接。
- `skin_custom.png` 当前作为个性化形象展示，尚未注册为可装备皮肤。
- AI 奖励冷却只在进程内保存，重启后不保留，也未实现 `event_id` 持久化幂等。
- 社交互动事件和社交任务领奖各有 15 EXP，是否属于重复奖励需产品确认。
- `vela_pet_get_profile()` 和 `vela_pet_get_tasks()` 返回共享快照，不宜被多个调用方长期持有。

## 12. 建议联调顺序

1. ~~编译运行端侧业务测试~~ —— 已完成（`tests/`）。
2. ~~关闭 UI Mock，验证主页、任务、签到和皮肤真实数据~~ —— 已完成（真机）。
3. 接通后端 `/health` 和聊天接口 —— **待网络方案**。
4. 接入四个设备 transport 回调 —— **待网络方案**。
5. ~~验证 `/data/agent/assets/pet` 真机写入~~ —— 目录已建，待持久化。
6. ~~注册 LVGL 文件系统和 PNG 解码器~~ —— 已完成。
7. 验证新图片覆盖同一路径后的缓存刷新。
8. 最后进行断网、损坏文件、反复切页、重复生成和内存压力测试。

## 13. 相关文档

- `README.md`：端侧核心与接口说明。
- `docs/settings.md`：成长、任务和奖励设定。
- `docs/integration/frontend.md`：业务与 AI bridge 前端对接。
- `docs/integration/ai-cloud.md`：AI 云端六项问题答复。
- `docs/integration/ui-image.md`：图片显示适配回应。
- `backend-ai/docs/api-integration.md`：端云协议详情。
- `ui/README.md`：前端独立（模拟器）构建方式。
