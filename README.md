# VelaPet · 会成长的智能手表表宠

## 一、作品简介

VelaPet 是一个面向 **openvela / Apache NuttX** 智能手表的虚拟宠物应用，已在
**SiFli SF32LB52-DevKit-LCD** 真机上完整跑通。

用户通过日常步数、连续运动和签到来养育自己的表宠：完成任务获得经验、升级解锁新皮肤，
表宠则用形象和文字给出反馈。**端侧业务核心、LVGL 前端、AI 云端桥接三部分均为本项目实现。**

**亮点**

- **完整成长闭环，真机验证通过** —— 步数 → 完成任务 → 领取经验 → 升级 → 解锁并切换皮肤，
  整条链路都能在手表上一次走完。
- **五页 LVGL 前端** —— 主页、任务、AI 聊天、个性化形象、皮肤，含经验条、任务卡、
  皮肤仓库、轻提示与升级弹窗，触摸交互完整。
- **真实图片渲染** —— PNG 经 LVGL 解码显示（lodepng + POSIX 文件系统驱动），
  按容器尺寸等比缩放，读图失败自动回退到手绘形象。
- **掉电安全的存档** —— 状态以 cJSON 序列化，写入采用「临时文件 → rename」的原子替换，
  中途断电不会留下半个损坏的存档。
- **对无电池硬件的适配** —— 本板没有 RTC 备用电池，应用在开机时把时钟播种为固件编译时间，
  业务侧的日期逻辑也能容忍时钟回跳，不会出现「复位后永远无法签到」。
- **可测试** —— 业务逻辑与日期逻辑都有主机侧测试，不需要硬件即可回归。

---

## 二、选题方向

**快应用 / 手表应用创新**（作品形态：**应用**，代码放在 `app/`）。

选它的理由是：这是一款完整的**手表端应用**——五页 LVGL 界面 + 一套端侧业务核心
（成长 / 任务 / 奖励 / 皮肤 / 存档 / 后台服务），有真实的触摸交互与真机验证；
另外还包含一个可独立运行的 AI 云端桥接（Python 后端 + 设备端 client/bridge）。
界面用 LVGL 原生实现（而非 `.ux` 快应用框架），所以代码放在 `app/` 而不是 `quickapp/`。

需要说明的是，本作品同时也包含**真实的板级适配**工作（见 `board/`），
但它服务于上面的手表应用，因此不作为「新硬件适配」赛道参赛。

---

## 三、目录结构

```text
contest2026_084_AAAmengchongpifa/
├── app/
│   └── velapet/              # VelaPet 应用完整源码
│       ├── src/              #   端侧业务核心、公共 ABI、后台服务、应用入口
│       ├── ui/               #   LVGL 前端（五页 + 主题/组件，含模拟器 Mock）
│       ├── backend-ai/       #   Python 后端 + 设备端 AI client/bridge
│       ├── data/             #   默认宠物数据
│       ├── tests/            #   主机侧测试（业务逻辑、日期/时钟回跳）
│       ├── docs/             #   架构交接、数值设定、跨模块联调文档
│       └── CMakeLists.txt / Kconfig / Make.defs / Makefile
├── board/
│   └── velapet_sf32lb52/     # SiFli SF32LB52 板级改动（补丁形式）+ 应用说明
├── firmware/
│   └── nuttx.bin             # 已验证的固件镜像，可直接烧录体验，无需先构建
├── logs/
│   └── kei-ds/               # AI Coding 日志（按 GitHub 账号 / 日期组织）
├── skills/                   # 本次开发沉淀的可复用 Skill（Claude Code 格式）
│   ├── openvela-board-bringup/   #   构建、烧录、串口排障的标准流程 + 烧录脚本
│   └── nuttx-app-integration/    #   把应用接进 openvela/NuttX 构建体系的正确姿势
├── AI-开发日志.md            # 开发过程记录（含失败与教训）
└── contest2026_084_AAAmengchongpifa.xml   # 本仓 manifest（含 linkfile 映射）
```

**关于 `<linkfile>`**：`app/velapet` 由 manifest 软链到 openvela 编译树的
`packages/demos/contest2026_084_velapet`，apps 的 CMake 自动发现机制会找到它，
**不需要改动 `apps` 仓库的任何文件**。

`board/velapet_sf32lb52/` 装的是**针对公共仓库 `vendor/sifli` 的补丁**，不是一个可编译的
板级目录，因此**有意不配置 linkfile**；应用步骤见该目录下的 `README.md`。

---

## 四、运行方式

### 1. 拉取工程

```bash
repo init -u https://github.com/open-vela/contest2026_084_AAAmengchongpifa \
  -b dev-ai-contest-2026 -m contest2026_084_AAAmengchongpifa.xml
repo sync -c -j8
```

同步后本仓位于工作区下的 `contest2026_084_AAAmengchongpifa/`。

### 2. 应用板级补丁

应用要跑在 SF32LB52-DevKit-LCD 上，还需要 `vendor/sifli` 的板级改动。
这些改动按《参赛代码提交指南》应走公共仓 PR，这里以补丁形式随仓提交：

```bash
cd <工作区>/vendor/sifli
git am <工作区>/contest2026_084_AAAmengchongpifa/board/velapet_sf32lb52/0001-*.patch
git am <工作区>/contest2026_084_AAAmengchongpifa/board/velapet_sf32lb52/0002-*.patch
git am <工作区>/contest2026_084_AAAmengchongpifa/board/velapet_sf32lb52/0003-*.patch
```

补丁基于该仓库 `9c079ca`（即 `repo sync` 后的默认状态）。若基线漂移导致冲突，
补丁很小，可逐个文件手工对照。三个补丁分别解决：**使能应用与显示/文件系统依赖**、
**NOR 写校验的诊断输出**、**无 RTC 电池时的时钟播种**。

### 3. 编译

构建必须在 Linux / WSL 下进行（Windows 侧没有工具链）：

```bash
cd <工作区>
source build/envsetup.sh          # 必需：否则 arm-none-eabi-gcc 不在 PATH

cmake -B cmake_out/sf32lb52_devkit_lcd -S "$PWD/nuttx" -GNinja \
  -DBOARD_CONFIG=../vendor/sifli/boards/sf32lb52/sf32lb52_devkit_lcd/configs/nsh \
  -DEXTRA_FLAGS="-Wno-cpp -Wno-deprecated-declarations"
cmake --build cmake_out/sf32lb52_devkit_lcd
```

产物：`cmake_out/sf32lb52_devkit_lcd/nuttx.bin`。

> **坑（必读）**：改动 `configs/nsh/defconfig` 之后，必须
> `rm -rf cmake_out/sf32lb52_devkit_lcd` 再重新配置。只重跑 cmake 会被已有的
> `.config` 覆盖，新配置不生效。

### 4. 烧录

本仓 `firmware/nuttx.bin` 是已验证的镜像，**不构建也能直接烧录体验**
（由 `apps` 提交 `3f7ee0d` + `vendor/sifli` 提交 `6ad7ca4` 构建）。

```bash
sftool -c SF32LB52 -m nor -p COM5 -b 1000000 \
  --before default_reset --after soft_reset \
  write_flash nuttx.bin@0x12010000
```

烧到 NOR 的 `0x12010000`（Windows 侧执行，COM5 = CH343 USB-UART）。

> **坑（必读）**：只有**刚拔插过 USB** 时烧录才可靠——ROM bootloader 的 `ATSF32`
> 监听窗口只有约 2 秒。否则会一直 `Failed to download stub: TimedOut`，
> 换波特率或参数都没用。若日志出现 `nsh: ~ATSF32!~...: command not found`，
> 说明 sftool 在跟运行中的 NuttX 说话、没进 ROM 模式，拔插 USB 后立刻烧即可。

### 5. 运行

串口终端 **1000000 8N1**，上电后：

```text
nsh> velapet
```

界面启动后即可体验完整闭环。演示构建会模拟步数增长（`CONFIG_EXAMPLES_VELAPET_DEMO_INPUT`），
方便在没有计步器的板子上走完「步数 → 任务 → 经验 → 升级 → 换肤」。

> 注意：`velapet` 是**前台内置命令**，运行期间 NSH 不响应控制台，这是正常行为，复位即可退出。

### 6. 主机侧测试（可选，不需要硬件）

```sh
cd app/velapet
CJ=<工作区>/apps/netutils/cjson/cJSON
gcc -Wall -Wextra -Isrc -I"$CJ" \
    tests/velapet_logic_test.c \
    src/pet.c src/task.c src/reward.c src/json_utils.c src/storage.c \
    "$CJ/cJSON.c" -lpthread -lm -o /tmp/velapet_logic_test
/tmp/velapet_logic_test
```

`tests/velapet_date_test.c` 覆盖同一日期、隔天签到、时钟回跳。

---

## 五、AI Coding 使用说明

本作品从需求拆解到真机调试全程使用 **Claude Code（CLI）** 辅助开发，AI 对话日志已导出到
[`logs/kei-ds/`](logs/kei-ds/)（3 个会话、3554 个事件）。

**各环节的协作方式**

| 环节 | 怎么做 |
| --- | --- |
| 需求拆解 / 方案设计 | 由我给出目标与约束（板子型号、无 RTC 电池、无计步器、`/data` 可用性未知），AI 提出模块划分与 ABI 设计，我确认或否决后再动手 |
| 编码 | AI 按模块产出实现；业务核心与日期逻辑这类容易出错的边界（跨天、时钟回跳）由 AI 先写测试用例再写实现 |
| 调试 | 真机串口日志贴给 AI 定位；构建配置不生效、NOR 写校验失败、文件系统格式化根因等都是这样查出来的 |
| 文档 | 架构交接、数值设定、联调文档与 README 由 AI 起草、我核对事实 |

**AI 带来的实际帮助**

- 把「一个能跑的 LVGL 应用 + 一套端侧业务核心 + 一个 Python 后端」在**几天内**推到真机可用，
  主要省在**样板代码**（CMake/Kconfig 接入、LVGL 页面骨架、cJSON 序列化）与**排查速度**上。
- 整个开发过程被记录成 [`AI-开发日志.md`](AI-开发日志.md)，其中**失败与教训被刻意保留**
  （构建配置陷阱、烧录时序、文件系统格式化的根因、蓝牙联网方案被证伪、前台命令占用控制台），
  因为这些才是可复用的部分。
- 过程中沉淀了两个可复用 Skill，放在 [`skills/`](skills/)：openvela 板级 bring-up 流程、
  以及把应用接进 openvela/NuttX 构建体系的正确姿势。

> 日志由官方 `contest-log-collector` 采集，未做任何内容修改；格式说明见
> [`logs/README.md`](logs/README.md)。
