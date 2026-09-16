# VelaPet 组员三代码项目接入说明

## 目录说明

```text
VelaPet_member3_code_project/
|-- CMakeLists.txt
|-- src/
|   |-- vela_pet_api.h
|   `-- ui/
|       |-- velapet_ui.h
|       |-- velapet_ui.c
|       |-- ui_router.c
|       |-- ui_common.c
|       |-- ui_home.c
|       |-- ui_task.c
|       |-- ui_chat.c
|       |-- ui_pet_image.c
|       |-- ui_skin.c
|       `-- velapet_ui_mock.c
|-- examples/
|   `-- velapet_ui_entry_example.c
`-- docs/
```

## 推荐合并位置

把 `src/ui` 放到主项目：

```text
apps/examples/velapet/src/ui/
```

如果主项目还没有统一业务接口，可参考 `src/vela_pet_api.h`；如果已有同名文件，以主项目文件为准，只需要保证字段和函数签名一致。

## CMake 接入

在主项目 `CMakeLists.txt` 的源文件列表中加入：

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

如果业务模块尚未完成，需要先演示页面，再额外加入：

```cmake
src/ui/velapet_ui_mock.c
```

并启用：

```cmake
add_compile_definitions(VELAPET_UI_USE_MOCK)
```

## 主程序调用

LVGL、屏幕、触摸和业务模块初始化完成后调用：

```c
#include "ui/velapet_ui.h"

velapet_ui_init();
```

当组员一更新步数、组员二更新任务/经验/皮肤、组员四同步 AI 回复或图片资源后，可调用：

```c
velapet_ui_refresh_all();
```

AI 回复到达时：

```c
velapet_ui_chat_set_loading(0);
velapet_ui_chat_set_reply(reply);
```

图片资源状态变化时：

```c
velapet_ui_pet_image_set_state(state, asset_path);
```

## 对接边界

- 组员三负责 LVGL 页面、路由、按钮、显示和提示。
- 组员二负责 `vela_pet_api.h` 中的成长、任务、签到、皮肤数据。
- 组员四负责 AI 回复文本和 `/data/agent/assets/pet/` 下的图片资源。
- 组员一负责屏幕、触摸、图片解码、真机性能和振动反馈。
