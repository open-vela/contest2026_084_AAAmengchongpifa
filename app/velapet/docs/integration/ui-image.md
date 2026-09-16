# VelaPet 端侧对前端图片适配反馈

## 1. 确认结果

已读取新版 `ui` 文件夹。前端当前已经完成：

- 收到本地图片路径后创建并显示 `lv_img`；
- 调用 `lv_img_cache_invalidate_src()` 使同路径旧缓存失效；
- 使用 `lv_img_decoder_get_info()` 在显示前验证资源可解码；
- 按展示区域等比例缩放图片；
- 形象页和主页共享最新图片状态；
- 路径为空、解码失败或生成失败时回退默认表宠；
- 页面销毁时清空主页、聊天页和形象页的静态控件指针。

端侧认可这一刷新流程。AI 网络线程仍然只负责下载和设置完成状态，所有 LVGL 解码、缓存失效和控件更新必须由 LVGL/UI 线程执行。

## 2. 本次端侧配合修改

新版 UI 原来把 `/data/agent/assets/pet/` 固定转换成 `S:/`。本次已改为可配置宏：

```c
VELAPET_UI_LVGL_FS_PREFIX
```

默认值仍为模拟器使用的 `S:`。真机主工程必须将它设置为实际注册且映射到 `/data/agent/assets/pet` 的 LVGL 盘符，例如：

```cmake
-DVELAPET_UI_LVGL_FS_PREFIX=A:
```

实际盘符由主工程注册结果决定，不能直接假定真机也是 `S:`。

同时已把前端文档中的图片文件名统一为端云协议使用的：

```text
/data/agent/assets/pet/avatar.png
/data/agent/assets/pet/home_pet.png
/data/agent/assets/pet/skin_custom.png
```

## 3. 图片解码器回应

当前 UI 使用的是 LVGL v8 风格接口：

```c
lv_img_create()
lv_img_decoder_get_info()
lv_img_cache_invalidate_src()
lv_img_set_src()
lv_img_set_zoom()
```

主工程必须提供与当前 LVGL 版本匹配的 PNG 解码器，并保证 `lv_img_decoder_get_info()` 能识别磁盘文件。仅打开一个配置宏但没有把解码器源文件或依赖库链接进最终程序，仍然会加载失败。

真机验收应使用 AI 实际生成的 `home_pet.png`，而不是只测试编译进固件的 C array 图片。

## 4. 文件系统盘符回应

UI 收到的是 POSIX 存储路径：

```text
/data/agent/assets/pet/home_pet.png
```

LVGL 使用的是驱动盘符路径，例如：

```text
A:/home_pet.png
```

主工程需要注册一个 LVGL 文件系统驱动，使该盘符根目录准确映射到：

```text
/data/agent/assets/pet
```

驱动至少应实现打开、关闭、读取和定位。UI 不直接负责挂载 `/data`，也不应该在 UI 中绕过 LVGL 文件系统驱动调用底层文件 API。

## 5. 真机资源路径回应

端云协议继续采用以下固定目录：

```text
/data/agent/assets/pet/
```

三个输出文件保持为 `avatar.png`、`home_pet.png`、`skin_custom.png`。其中主页与形象页当前使用 `home_pet.png`。

需要主工程/硬件适配确认：

1. `/data` 已挂载为可写持久化分区；
2. 启动时创建 `/data/agent/assets/pet`；
3. 能在该目录创建、写入、同步、重命名和删除临时文件；
4. LVGL 文件系统驱动能从同一目录读取；
5. 重启后文件仍然存在。

当前桌面工作区无法代替真机验证挂载和权限，因此“路径约定已确认”，但“真机可写”必须由主工程在板上实测。

## 6. LVGL 内存回应

当前端侧可确认：AI 网络工作线程栈为 64 KiB，主 VelaPet 应用栈默认 8 KiB。但这不等于 LVGL 图片解码内存预算。

LVGL 内存需要覆盖：

- PNG 解码器状态和临时缓冲；
- 解码后的像素数据；
- 图片缓存；
- 页面对象、字体和绘制缓冲；
- 与 TLS/cJSON 同时运行时的系统堆峰值。

RGBA8888 图片仅像素数据可按以下公式估算：

```text
宽 × 高 × 4 字节
```

例如 220×176 约为 155 KiB，390×450 约为 686 KiB，尚未包含 PNG 解码临时内存和 LVGL 其他对象。因此后端应尽量直接输出接近最终显示尺寸的图片，避免真机解码超大图片。

主工程需要在真机上测量：

- 加载图片前后的空闲堆；
- PNG 解码瞬时峰值；
- 切页和重复刷新后的内存是否回收；
- AI TLS 请求与 PNG 解码重叠时的最小剩余内存；
- 图片缓存命中和失效后的内存变化。

在没有目标板 RAM、LVGL 配置文件和显示色深数据前，端侧不能给出可靠的固定 `LV_MEM_SIZE` 数值。

## 7. 主工程需要提供的最小能力

主工程/硬件适配需完成以下事项后，前端即可从模拟器切换到真机：

- 启用并链接与当前 LVGL 版本匹配的 PNG 解码器；
- 注册映射 `/data/agent/assets/pet` 的 LVGL 文件系统驱动；
- 设置 `VELAPET_UI_LVGL_FS_PREFIX`；
- 确保 `/data/agent/assets/pet` 可写且重启后持久化；
- 配置足够的系统堆、LVGL 堆和绘制缓冲；
- 在 LVGL 线程周期调用 `velapet_ai_bridge_poll_ui()`；
- 下载文件完成原子替换后才向 UI 回填 READY 状态。

## 8. 联调验收标准

1. 下载前显示默认表宠。
2. AI 请求期间显示生成中状态。
3. `home_pet.png` 写入并校验完成后，UI 线程显示真实图片。
4. 用不同图片覆盖同一路径后，缓存失效并显示新内容。
5. 删除文件、破坏 PNG 或关闭解码器时，UI 自动回退默认表宠且不崩溃。
6. 在请求期间连续切换主页和形象页，不访问已销毁控件。
7. 连续生成和刷新多次后，无持续性内存增长。
8. 重启设备后仍可加载上次成功保存的 `home_pet.png`。

## 9. 当前状态

- 前端图片显示逻辑：已完成，模拟器已验证。
- 盘符硬编码：已改为构建时可配置。
- 端云文件名：已统一。
- 真机 PNG 解码器：待主工程提供并验证。
- 真机 LVGL 文件系统驱动：待主工程提供并验证。
- 真机目录权限与持久化：待硬件/主工程验证。
- LVGL/系统堆最终配置：待目标板压力测试后确定。

