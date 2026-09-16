---
name: openvela-board-bringup
description: Build, flash and debug an openvela / Apache NuttX board (SiFli SF32LB52 style) from a Windows + WSL setup. Use when compiling firmware for such a board, when flashing with sftool fails with "Failed to download stub", when a defconfig change seems to have no effect, or when the serial console stops responding after launching an app.
---

# openvela 板级构建 / 烧录 / 排障

适用：Windows 上编辑、WSL 里编译、通过 USB 串口烧录与调试的 openvela(NuttX) 板子
（本文以 SiFli SF32LB52-DevKit-LCD 为例）。所有结论都在真机上验证过。

## 硬性前提

- **编译只能在 WSL/Linux 侧做**，Windows 没有交叉工具链。
- **烧录和串口只能在 Windows 侧做**（WSL 通常没有串口透传）。
- 每次烧录**都需要有人拔插 USB**（原因见「烧录」）。AI 无法替代这一步。

## 1. 编译

```bash
cd <openvela>
source build/envsetup.sh          # 必需：否则 arm-none-eabi-gcc 不在 PATH

cmake -B cmake_out/<board> -S "$PWD/nuttx" -GNinja \
  -DBOARD_CONFIG=../vendor/<vendor>/boards/<board>/configs/<config> \
  -DEXTRA_FLAGS="-Wno-cpp -Wno-deprecated-declarations"
cmake --build cmake_out/<board>
```

产物：`cmake_out/<board>/nuttx.bin`

### ⚠️ 陷阱 0：`envsetup.sh` 必须从工作区根目录 source

先 `cd <openvela>` **再** `source build/envsetup.sh`。它是靠当前目录往上找
`nuttx/tools/Unix.mk` 来定位树顶的；不在树内执行时它会**静默跳过** PATH 设置。

这个陷阱很隐蔽，因为**编译前半段仍然能成功** —— CMake 在配置期已经把编译器的绝对路径
缓存下来了，所以 gcc/g++ 照常工作。直到链接之后需要按名字调用 `objcopy` 生成 `nuttx.bin`
时才炸：

```
arm-none-eabi-objcopy: not found
```

看到这个报错，先检查是不是忘了 `cd`，而不是去怀疑工具链。

### ⚠️ 陷阱 1：改了 defconfig 不生效

构建目录里已有的 `.config` 会压过新的 defconfig，**只重跑 cmake 是没用的**。

```bash
rm -rf cmake_out/<board>      # 改板级 defconfig 后必须这么做
```

否则新加的 `CONFIG_*` 不会出现在 `.config` 里，表现为"改了代码/配置却毫无变化"。
判断依据：`grep <你的符号> cmake_out/<board>/.config`。

## 2. 烧录

```bash
sftool -c <CHIP> -m nor -p <PORT> -b 1000000 \
  --before default_reset --after soft_reset \
  write_flash nuttx.bin@0x12010000
```

地址取板级链接脚本的 flash ORIGIN（未特别说明时 XIP 入口是 `0x12010000`）。

### ⚠️ 陷阱 2：同一条命令有时成功、有时连续失败十几次

现象：`Failed to download stub: Io(Custom { kind: TimedOut })`。

**这不是参数问题** —— 换波特率、`--compat`、`-m nor_type1` 都没用。

**诊断线索**：去看串口控制台。如果出现形如

```
nsh: ~ATSF32!~@r...: command not found
```

说明 **sftool 的同步串被运行中的 shell 当成命令执行了** —— 芯片根本没进 ROM 下载模式。

**根因**：ROM 引导程序的 `ATSF32` 监听窗口只有约 2 秒。

**做法**：**先拔插 USB，等端口一出现就立刻烧**。不要等端口"稳定"几秒再烧，那会错过窗口。
可靠的工作流是跑一个盯端口的循环：等端口消失 → 出现 → 立即发起烧录。
脚本见本 Skill 的 `scripts/flash-watch.ps1`。

### 串口参数

1000000 8N1。RTS 往往通过负载开关控制 SoC 供电，因此：

- **不要用 `screen` / `cu`**（会把芯片按在复位态）；用 `picocom --noreset --lower-rts --lower-dtr`。
- **不要手动脉冲 RTS 做复位**，容易把芯片留在 ROM 下载模式（表现为串口全哑、像变砖）。
  需要复位就让 sftool 带 `--after soft_reset`，它用的是正确时序。

## 3. 串口交互与验证

### ⚠️ 陷阱 3：启动应用后控制台"死"了

NuttX 的**内置命令是前台运行的**：NSH 会等它结束，而长驻应用（如 LVGL 界面）永不返回，
于是 NSH 一直阻塞在等待上 —— 控制台不报错、不回显、像崩溃，其实是**正常行为**。

后果：应用运行期间**无法用 `cat` / `ls` 之类做程序化验证**，只能靠人看屏幕；
要回 shell 就复位板子。

推论：如果某个功能需要靠读文件来验证，**先在没有启动应用的时候读**，或者让程序把结果
打到控制台。

### 复位与连接

- 需要"干净重启"时，跑任意 sftool 命令并带 `--after soft_reset`（只读命令也无副作用）。
- 若报端口不存在 / `os error 3`，是设备还在重新枚举，等 1~2 秒重试。

## 4. 排障对照表

| 现象 | 根因 | 处理 |
| --- | --- | --- |
| 改了 defconfig 无效果 | 旧 `.config` 覆盖 | 删构建目录重新配置 |
| `Failed to download stub: TimedOut` | 没抓住 ROM 监听窗口 | 拔插 USB 后立刻烧 |
| 控制台出现 `nsh: ~ATSF32!~...` | 芯片没进 ROM 模式 | 同上；检查是否手动动过 RTS |
| 启动应用后控制台无响应 | 前台内置命令在运行 | 正常，复位退出 |
| 串口全哑、像变砖 | 可能被留在 ROM 下载模式 | 用 `--after soft_reset` 让芯片正常启动 |
| 板子卡在启动、完全无输出 | 启动路径里的某个操作卡死（如挂文件系统） | 把该操作移出启动路径，改成手动触发 |

## 5. 一条设计教训

**不要把可能失败、且失败后不可见的操作放在启动路径上。** 我们曾在启动时尝试格式化
文件系统，格式化卡住后整板起不来、连错误都看不到，只能靠重新烧录恢复。
改成"启动只做安全尝试、失败就降级；危险操作做成手动命令"之后，板子永远能起来，
排查也变得可见。
