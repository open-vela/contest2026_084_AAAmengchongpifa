---
name: nuttx-app-integration
description: Add an application into an openvela / Apache NuttX build tree (apps/) so it builds as a built-in NSH command. Use when wiring a new app into CMakeLists/Kconfig, when nuttx_add_application complains or silently does nothing, when a dependency header is not found, or when an app builds but never appears in the builtin list.
---

# 把应用接进 openvela / NuttX 构建体系

适用：给 `apps/` 树（openvela、NuttX）加一个新应用，让它编成内置命令。

## 1. 放哪里 —— 不用改别人的文件

应用放到 `apps/<category>/<name>/`（例如 `apps/examples/velapet/`）。
`apps/examples/CMakeLists.txt` 里的 `nuttx_add_subdirectory()` 会**自动发现带
`CMakeLists.txt` 的子目录**，`nuttx_generate_kconfig()` 会**自动收集 Kconfig**。

➡️ **不需要**修改 `apps/CMakeLists.txt`、`apps/examples/CMakeLists.txt` 或任何上层文件。
如果某个应用"放了但没被编译"，先确认目录里有 `CMakeLists.txt`。

## 2. CMakeLists.txt 的正确写法

```cmake
if(CONFIG_EXAMPLES_MYAPP)
  nuttx_add_application(
    NAME ${CONFIG_EXAMPLES_MYAPP_PROGNAME}
    SRCS
      src/myapp_main.c
      src/other.c
    PRIORITY ${CONFIG_EXAMPLES_MYAPP_PRIORITY}
    STACKSIZE ${CONFIG_EXAMPLES_MYAPP_STACKSIZE}
    MODULE ${CONFIG_EXAMPLES_MYAPP}
    DEPENDS lvgl                       # 依赖的库目标
    INCLUDE_DIRECTORIES
      ${CMAKE_CURRENT_LIST_DIR}/src
      ${CMAKE_BINARY_DIR}/apps/include/netutils
  )
endif()
```

必须知道的约束：

- **`nuttx_add_application()` 没有 `LIBRARIES` 参数。** 写了它会解析失败（或被忽略），
  不要指望用它链接库。库依赖用 `DEPENDS`。
- **`SRCS` 的第一项会被当作含 `main()` 的文件**，自动加上 `main=<name>_main` 别名。
  所以第一个文件必须是入口；不要在同一个目标里放两个 `main`。
- `INCLUDE_DIRECTORIES` 用于私有头文件路径（等价于 `-I`）。
- 条件编译源文件用变量拼：

  ```cmake
  set(MYAPP_SRCS src/a.c src/b.c)
  if(CONFIG_MYAPP_EXTRA)
    list(APPEND MYAPP_SRCS src/extra.c)
  endif()
  ```

## 3. ⚠️ 依赖头文件找不到（以 cJSON 为例）

NuttX 里很多库的头文件被"发布"到构建目录下带命名空间的路径。例如 cJSON：

- 源码头文件实际位于 `<build>/apps/include/netutils/cJSON.h`
- 因此既能写 `#include "netutils/cJSON.h"`（若基础包含路径已含 `apps/include`），
  也可以把 `<build>/apps/include/netutils` 加进 `INCLUDE_DIRECTORIES` 后写 `#include <cJSON.h>`。

**排查方法**：不要在源码里猜包含形式，去构建目录里找头文件到底被放在哪：
`find <build> -name 'the_header.h'`。

另外注意有些库在**配置阶段**才下载（如 cJSON 走 FetchContent），首次配置需要网络。

## 4. ⚠️ 不要用你自己的头文件"顶掉"官方 ABI

如果工程里同时存在"正式 ABI 头"和"给 Mock 用的同名头"，**只把正式那份的目录加进
`INCLUDE_DIRECTORIES`**。两份都加会出现同名头文件互相抢占，编译通过但链接到错误的
数据结构定义。这类问题症状隐蔽（能编过、运行行为诡异）。

## 5. Kconfig

```kconfig
config EXAMPLES_MYAPP
  tristate "My app"
  default n
  ---help---
    ...

if EXAMPLES_MYAPP

config EXAMPLES_MYAPP_PROGNAME
  string "Program name"
  default "myapp"

config EXAMPLES_MYAPP_STACKSIZE
  int "Stack size"
  default 32768        # 跑 LVGL 之类的要够大，默认 8192 往往不够

endif
```

## 6. 别忘了在 defconfig 里打开

`configs/<config>/defconfig` 加上 `CONFIG_EXAMPLES_MYAPP=y`，否则应用根本不会被编译。

**改完 defconfig 必须删除构建目录重新配置**，否则旧的 `.config` 会覆盖它（详见
`openvela-board-bringup` Skill）。

## 7. 验证它真的接进去了

```bash
grep -rn "myapp" <build>/apps/builtin/builtin_list.h     # 内置命令注册了吗
arm-none-eabi-nm <build>/nuttx | grep myapp_main         # 符号在最终镜像里吗
grep EXAMPLES_MYAPP <build>/.config                      # 配置生效了吗
```

`builtin_list.h` 里应出现形如 `{ "myapp", <prio>, <stack>, myapp_main }` 的一行。

## 8. 附带资源：不用改构建文件

板级 `src/etc/` 目录会被**整目录**打包进 romfs 挂到 `/etc`。
往里放文件（含二进制、子目录）即可出现在设备上，**不需要修改 CMakeLists**。

注意：新增文件后，那个目录没有声明依赖，增量构建可能不会自动重打 romfs ——
删掉生成的 `romfs.img` / `romfs_*.c` 再构建，或改一次被跟踪的文件。
