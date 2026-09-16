# VelaPet LVGL UI

本目录包含 VelaPet 手表端 LVGL 前端、页面路由、公共 UI 组件、AI/图片接口和 Mock 演示数据。

## 成果内容

- `CMakeLists.txt`：可单独作为静态库接入，也可把源文件列表复制进主工程。
- `src/vela_pet_api.h`：Mock 模式专用的业务接口兼容头。正式联调时不使用该头文件，而是通过 `VELAPET_APP_INCLUDE_DIR` 引用组员二正式 ABI。
- `src/ui/velapet_ui.h`：前端模块对外入口。
- `src/ui/velapet_ui.c`：UI 初始化与统一刷新入口。
- `src/ui/ui_router.c`：主页、任务页、AI 聊天页、形象页、皮肤页路由。
- `src/ui/ui_common.c`：标题栏、返回按钮、通用按钮、经验条、提示弹窗。
- `src/ui/ui_home.c`：表宠主页，展示等级、经验、今日进度、皮肤、步数与入口按钮。
- `src/ui/ui_task.c`：任务列表、任务状态、领奖、签到。
- `src/ui/ui_chat.c`：AI 聊天页，支持快捷按钮、加载状态、回复/错误回填。
- `src/ui/ui_pet_image.c`：个性化表宠形象页，支持默认、生成中、加载成功、加载失败状态。
- `src/ui/ui_skin.c`：皮肤页，展示已解锁和未解锁皮肤，并调用切换接口。
- `src/ui/velapet_ui_mock.c`：缺少真实业务模块时用于静态演示的 mock 数据。
- `examples/velapet_ui_entry_example.c`：主程序接入示例。
- `docs/test_checklist.md`：页面跳转、数据显示、按钮、异常状态、小屏适配测试。
- `docs/common_issues.md`：图片加载失败、按钮无响应、经验条不刷新等问题处理。
- `../docs/integration/frontend.md`：正式业务与 AI bridge 对接说明。
- `../docs/integration/ui-image.md`：真机图片、盘符和内存适配说明。

## 接入方式

> **2026-09-14 起，正式固件不再使用本节描述的独立构建方式。** 应用现在直接位于
> `apps/examples/velapet`，由该目录的 `CMakeLists.txt` 把 `ui/src/ui/*.c` 编进应用目标
> （`DEPENDS lvgl`，同时链接正式 `src/vela_pet_api.h`）。构建与烧录见
> `../docs/project-handoff.md` §5。
>
> 下面这套 `ui/CMakeLists.txt` + `VELAPET_UI_BUILD_MOCK` 的用法**仅用于在 PC 上做纯 UI
> 模拟**，保留给前端单独调试。

以下为独立（模拟器）接入方式。把 `src/ui` 目录复制到项目的 `apps/examples/velapet/src/ui/`，
并在工程文件中加入以下源文件：

```cmake
src/ui/velapet_ui.c
src/ui/ui_router.c
src/ui/ui_common.c
src/ui/ui_home.c
src/ui/ui_task.c
src/ui/ui_chat.c
src/ui/ui_pet_image.c
src/ui/ui_skin.c
```

如需先静态演示，可额外加入：

```cmake
src/ui/velapet_ui_mock.c
```

并添加编译宏：

```c
VELAPET_UI_USE_MOCK
```

真实联调时不要启用该宏，由正式 `vela_pet_api.h` 和 AI bridge 提供数据。例如：

```cmake
-DVELAPET_UI_BUILD_MOCK=OFF
-DVELAPET_APP_INCLUDE_DIR=<正式 vela_pet_api.h 所在目录>
-DVELAPET_AI_BRIDGE_INCLUDE_DIR=<velapet_ai_bridge.h 所在目录>
-DVELAPET_UI_LVGL_FS_PREFIX=<映射到 /data/agent/assets/pet 的 LVGL 盘符，例如 S:>
```

正式模式下 AI 页三个快捷按钮分别向 bridge 请求 `cheer`、`task` 和 `level`；
AI bridge 的轮询与初始化仍由主工程负责。

`VELAPET_UI_LVGL_FS_PREFIX` 默认是模拟器使用的 `S:`。真机必须改为主工程实际注册的 LVGL 文件系统盘符，UI 不再把盘符写死在源码中。
