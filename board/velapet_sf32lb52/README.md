# 板级适配：SiFli SF32LB52-DevKit-LCD

VelaPet 的应用代码在 `app/velapet/`，但它要跑在 **SiFli SF32LB52-DevKit-LCD** 上，
还需要 `vendor/sifli` 仓库里的板级与芯片侧改动。这些改动属于**公共仓库**
（按《参赛代码提交指南》第三节，公共仓库的改动应 fork 后向 `dev-ai-contest-2026`
提 PR，而不是在专属仓里直接改），所以本目录以**补丁**形式保存，便于评委复现。

## 补丁清单

| 补丁 | 作用 | 是否必需 |
| --- | --- | --- |
| `0001-boards-sf32lb52_devkit_lcd-enable-VelaPet-and-rework.patch` | 打开 `CONFIG_EXAMPLES_VELAPET` 与 cJSON、LVGL PNG/文件系统；修正触摸 I2C 引脚；接上面板供电轨；把 NOR 数据分区偏移修正为 `0x008A0000`；重做 `/data` 挂载并创建目录；把默认形象图放进 romfs | **必需**（缺了编不出 `velapet`） |
| `0002-chips-sf32lb52-report-the-exact-verify-mismatch-in-t.patch` | NOR 写校验失败时打印首个差异字节与完整 32 字节，便于定位 | 可选（只影响诊断输出） |
| `0003-boards-sf32lb52_devkit_lcd-seed-the-clock-from-the-f.patch` | 上电时把时钟播种为固件编译时间（本板无 RTC 备用电池） | 必需（否则日期是硬件默认值，每日任务/签到不可信） |

补丁涉及的 5 个文件见 `vendor-sifli-changed-files.txt`。

## 基线

补丁基于 openvela 工作区中 `vendor/sifli` 仓库的提交 **`9c079ca`**
（分支 `dev-ai-contest-2026`），即 `repo sync` 后的默认状态。

## 应用方式

先完成根目录 `README.md` 里的 `repo init` / `repo sync`，然后：

```sh
cd <工作区>/vendor/sifli
git am <工作区>/contest2026_084_AAAmengchongpifa/board/velapet_sf32lb52/0001-*.patch
git am <工作区>/contest2026_084_AAAmengchongpifa/board/velapet_sf32lb52/0002-*.patch
git am <工作区>/contest2026_084_AAAmengchongpifa/board/velapet_sf32lb52/0003-*.patch
```

若基线漂移导致 `git am` 冲突，也可以手工照改——补丁很小，逐个文件对照即可。

应用后，`app/velapet/` 已经由 manifest 的 `<linkfile>` 软链到
`packages/demos/contest2026_084_velapet`，apps 的 CMake 自动发现机制会找到它，
**不需要改动 `apps` 仓库的任何文件**。

## 固件

`../firmware/nuttx.bin` 是构建产物，可直接烧录体验。烧录与构建要点见根目录 `README.md`。
