# VelaPet 端侧给 AI 云端对接说明

## 1. 文档目的

本文档正式回复 AI 云端同学提出的六项联调问题，并说明端侧核心、AI client、AI bridge、板级网络和 LVGL UI 之间的边界。结论以当前工作区源码为准。

## 2. 问题一：以哪一份 vela_pet_api.h 作为唯一正式 ABI？

唯一正式 ABI 是：

```text
apps/examples/velapet/src/vela_pet_api.h
```

其数据结构来自同目录的：

```text
src/pet.h
src/task.h
```

AI bridge 和正式前端都必须通过 include path 使用这份头文件。前端原来维护的同名头文件存在结构体布局不兼容问题，本次已改为转发到正式 ABI，不再重复定义 `PetProfile` 和 `Task`。

正式构建禁止启用 `VELAPET_UI_USE_MOCK`。

## 3. 问题二：前端字段与端侧 PetProfile 是否完全一致？

修改前不一致，修改后正式构建已统一到端侧定义。

原问题包括：

- 前端将 `name`、`current_skin`、`description` 定义为指针，端侧为定长数组。
- 前端将 `unlocked_skins` 定义为 `const char **`，端侧为二维定长数组。
- 前端缺少部分 `PetProfile` 与 `Task` 字段。

本次已删除前端正式 ABI 的重复结构体定义，并修正皮肤页面中按指针判断定长数组的代码。Mock 模式仍使用简化字段，只能用于静态演示。

端侧 `vela_pet_get_profile()` 和 `vela_pet_get_tasks()` 返回共享快照。调用方只读使用，不得修改，也不应长期保存返回地址。

## 4. 问题三：AI 网络线程可用栈和内存是多少？

当前 AI bridge 显式创建工作线程并设置：

```c
pthread_attr_setstacksize(&attr, 64 * 1024);
```

因此 AI 网络工作线程栈为 64 KiB。主 VelaPet 应用线程栈由 `CONFIG_EXAMPLES_VELAPET_STACKSIZE` 配置，当前默认是 8192 字节，两者不是同一个线程栈。

已知工作线程内存占用包括：

- HTTP JSON 响应缓冲：8192 字节，位于工作线程栈。
- URL 缓冲：384 字节。
- AI 回复文本：256 字节。
- 最多三个图片资源描述。
- cJSON 请求/响应对象使用动态堆内存。
- HTTP/TLS 收发缓冲由板级网络实现决定。

单个下载文件限制为 1 MiB，但 `download` 回调必须流式写文件，禁止把整个图片一次性读入 RAM。

当前没有为 AI 模块预留固定堆额度，因此只能确认 64 KiB 线程栈，不能仅凭源码承诺整机峰值内存。真机必须测量：

- TLS 握手期间峰值堆占用；
- cJSON 解析时峰值；
- PNG 解码及 LVGL 图片缓存峰值；
- 最小剩余堆和线程栈高水位。

## 5. 问题四：三个文件能否保存到 /data/agent/assets/pet/？

协议和 AI client 已允许以下三个固定目标：

```text
/data/agent/assets/pet/avatar.png
/data/agent/assets/pet/home_pet.png
/data/agent/assets/pet/skin_custom.png
```

AI client 会拒绝服务器返回的其他目标路径，并检查：

- `byte_size` 大于 0 且不超过 1 MiB；
- SHA-256 字符串长度为 64；
- 下载 URL 属于配置的后端主机。

但当前源码只能确认“目标路径被允许”，尚不能确认真机目录一定可写：

- 端侧现有存储代码只确保 `/data/agent/memory` 存在。
- `/data/agent/assets/pet` 尚无统一创建和启动探测代码。
- 是否可写取决于真机 `/data` 挂载方式和权限。
- 实际写文件逻辑属于尚未实现的板级 `download` 回调。

板级实现必须先创建目录，并在启动时做一次创建、写入、flush、关闭和删除临时文件的可写性探测。下载时要求：

1. 写入同目录临时文件；
2. 流式计算 SHA-256；
3. 校验实际大小；
4. `fflush`/`fsync`；
5. `rename` 原子替换正式文件；
6. 失败时保留旧文件。

因此当前答复是：三个目标文件名和安全规则已确定；真机可写性尚需主工程在设备上确认。

## 6. 问题五：HTTP/TLS 回调接入哪个网络模块？

当前端侧核心目录没有可直接复用的 HTTP/TLS 模块，也没有证据表明工程已经固定使用 libcurl、NuttX webclient 或其他具体实现。因此不能在未确认主工程配置时指定一个不存在的网络模块。

正式接入点是 `device/include/velapet_ai_client.h` 中的四个回调：

```c
VelapetHttpPostJsonFn post_json;
VelapetHttpPostImageFn post_image;
VelapetHttpGetJsonFn get_json;
VelapetHttpDownloadFn download;
```

建议由主工程/硬件适配层新增一个板级 transport 模块，例如：

```text
src/platform/velapet_http_transport.c
src/platform/velapet_http_transport.h
```

该模块基于真机已经启用的网络栈实现四个回调，再通过 `VelapetAiBridgeConfig.client` 注入 bridge。网络实现负责：

- `X-VelaPet-Token` 请求头；
- JSON POST、multipart 文件上传、JSON GET 和流式下载；
- 每次请求的有限超时；
- HTTPS CA 与主机名校验；
- 响应 NUL 终止及缓冲区溢出检测；
- 不向其他主机的重定向转发 Token；
- 文件大小和 SHA-256 校验。

该逻辑不应放入 `vela_pet_api.c`，因为业务状态与网络传输需要保持解耦。

## 7. 问题六：保存文件后如何通知 UI 安全刷新？

所有 LVGL 操作必须在 LVGL/UI 线程执行。当前 bridge 已使用“工作线程产出结果、UI 线程轮询消费”的方式：

```text
AI工作线程下载并完成原子替换
    ↓
设置 bridge.worker_done
    ↓
LVGL线程定时调用 velapet_ai_bridge_poll_ui()
    ↓
velapet_ui_pet_image_set_state(READY, device_path)
    ↓
UI线程清缓存、解码并更新 lv_img
```

主 UI 线程应建立约 100 ms 的 LVGL timer，在回调中调用：

```c
velapet_ai_bridge_poll_ui();
```

禁止网络线程直接调用 `lv_img_set_src()`、`lv_label_set_text()` 或其他 LVGL API。

当前 bridge 已在图片任务成功后向 UI 回填 `home` 资源路径，前端也已增加切页保护。但前端目前仍只显示状态和路径，尚未真正加载 PNG。最终实现还需前端完成：

- 注册 `/data` 对应的 LVGL 文件系统驱动；
- 将 POSIX 路径转换成 LVGL 路径；
- 在 UI 线程调用当前 LVGL 版本对应的图片缓存失效接口；
- 调用 `lv_img_set_src()`；
- 解码失败时回退旧图或默认图。

同一路径文件被替换时，必须先使旧缓存失效，否则 LVGL 可能继续显示旧图片。

## 8. 已完成的端云桥接状态

当前已有：

- AI 文本请求组包和响应解析；
- 图片任务提交、轮询和三个文件下载调度；
- 下载目标白名单、大小与 SHA-256 参数检查；
- 64 KiB 独立工作线程；
- 一次只允许一个任务的忙碌保护；
- 300 秒进程内 AI 奖励冷却；
- UI 线程 poll 回填；
- 前端快捷按钮调用真实 bridge；
- 异步切页后的基础页面保护。

C bridge 主机回归测试已通过，覆盖：忙碌保护、奖励冷却、三个文件下载、失败回退和线程关闭。

## 9. 当前阻塞项

以下事项需要主工程/硬件或前端继续完成：

1. 确认真机实际使用的 HTTP/TLS 栈。
2. 实现并注入四个 transport 回调。
3. 真机验证 `/data/agent/assets/pet` 创建和写权限。
4. 在 LVGL 线程创建 bridge poll 定时器。
5. 启用 PNG 解码和文件系统驱动，真正显示下载图片。
6. 测量 TLS、cJSON 和 PNG 解码期间的峰值 RAM。
7. 如需装备 `skin_custom.png`，新增端侧自定义皮肤注册、解锁和持久化规则。

## 10. 最小联调顺序

1. 先关闭 UI mock，验证统一 ABI 下主页和任务页数据。
2. 使用局域网 HTTP 联通 `/health` 和 `/api/v1/chat`。
3. 验证三个聊天按钮、忙碌状态和失败降级。
4. 实现流式下载并在真机验证资源目录可写。
5. 完成图片任务，核对三个文件的大小和 SHA-256。
6. 在 LVGL 线程刷新 `home_pet.png`。
7. 最后启用 HTTPS，验证 CA、主机名、超时和断网恢复。

