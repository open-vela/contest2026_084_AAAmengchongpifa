# 接口与端侧对接

## 1. 系统边界

端侧核心负责等级、任务、签到、奖励和本地存档。后端只读取请求携带的上下文，生成回复/图像资源，不覆盖端侧成长状态。所有手表业务结构体只使用组员二的 `vela_pet_api.h`、`pet.h`、`task.h`。

API Base 示例：`http://电脑局域网IP:8000`。HTTPS 部署后验证 CA 与主机名，不能关闭证书检查。所有业务接口请求头为 `X-VelaPet-Token: 设备Token`。`/health` 不需要 Token。

## 2. 聊天

`POST /api/v1/chat`，JSON 示例（兼容现有 `vela_pet_get_ai_context()` 的 context）：

```json
{
  "device_id": "bandx-01",
  "prompt_type": "cheer",
  "message": "",
  "locale": "zh-CN",
  "max_chars": 50,
  "context": {
    "pet_name": "Vela", "level": 2, "exp": 80, "exp_to_next": 250,
    "title": "egg_baby", "current_skin": "sport_blue",
    "unlocked_skins": ["default", "sport_blue"],
    "total_steps": 12500, "consecutive_days": 2, "daily_progress": 80,
    "tasks": [{"description": "Walk 3000 steps today", "target": 3000, "progress": 2400, "status": 1}]
  }
}
```

`prompt_type`：`cheer` 鼓励，`task` 任务提醒，`level` 成长进度，`greeting` 问候，`custom` 配合 message。`locale` 支持 `zh-CN/en-US`。回复长度按 Unicode 字符计数，C 缓存按 UTF-8 字节计算。手表客户端固定请求最多 50 字，使用 256 字节缓存。

现有上下文没有真实 `today_steps`，任务步数达到目标后封顶；后端不能把 `total_steps` 当成今日步数。端侧可后续增加 `today_steps`、`task.type`、`personality`，再扩展后端模型。

成功/可用降级回复均为 HTTP 200：

```json
{
  "request_id": "32位随机ID",
  "reply": "每一步都算数，Vela正在为你加油！",
  "intent": "encouragement", "emotion": "proud",
  "source": "local_fallback",
  "reward": {"eligible": true, "event": "ai_encouragement", "event_id": "32位随机ID"},
  "error_code": "MIMO_NOT_CONFIGURED", "created_at": "UTC ISO8601时间"
}
```

`source=mimo` 为模型回复；`local_fallback` 为规则回复。后端故障/密钥未配置通过 error_code 表达，不应因此丢弃可用 reply。intent 枚举：greeting/reminder/encouragement/level_up/chat；emotion 枚举：cheerful/proud/gentle/excited/calm。`level_up` 当前也是成长进度分类，不代表权威升级事件，实际升级只以端侧为准。

只有 cheer 场景且最终意图为 encouragement 才提供鼓励奖励资格。桥接完成一次请求后最多调用一次 `vela_pet_on_ai_encouragement()`，并作 300 秒本进程冷却；重启后冷却不保留，也不保证跨进程幂等。正式奖励防刷需端侧提供带 event_id 的持久化幂等奖励 API。

## 3. 图片上传、任务和资源

`POST /api/v1/pet-assets/jobs` 使用 multipart/form-data：

| 字段 | 内容 |
| --- | --- |
| file | PNG/JPEG/WebP 文件，默认不超过5 MiB、1600万像素 |
| device_id | 1–64位字母数字、下划线、点、短横线 |
| style | 最多80字符，默认 soft cartoon mascot |
| asset_kinds | avatar,home,skin，可取子集 |

响应 HTTP 202，保存 `job_id`。每 2 秒请求 `GET /api/v1/pet-assets/jobs/{job_id}`。状态：queued → processing → succeeded / failed。失败读取 error_code，保留旧缓存并停止轮询。建议设备总等待180秒，每次 HTTP 独立设40秒超时。超时不自动重复创建收费图片任务。

成功时 artifacts 为资源数组：

| 字段 | 用途 |
| --- | --- |
| kind | avatar/home/skin |
| download_url | 相对路径，或配置的后端绝对URL |
| device_path | POSIX 文件缓存目标，不一定等于LVGL驱动路径 |
| version | SHA-256前12位，内容不变则版本不变 |
| sha256 | 完整64位十六进制校验值 |
| byte_size | 下载体字节数 |
| width,height | 终端尺寸 |
| mime_type | image/png |

资源路径：`/data/agent/assets/pet/avatar.png`、`home_pet.png`、`skin_custom.png`。下载接口 `GET /api/v1/pet-assets/{job_id}/files/{filename}` 也必须带设备 Token。原图不提供下载路由。资源URL中的 job_id 是随机值，访问授权仍依赖Token。

下载步骤：白名单检查目标 → 下载至同目录临时文件 → 校验字节数/SHA-256 → flush/fsync → rename → 通知UI。失败不覆盖旧文件。每个文件独立替换，三个文件不是事务性切换；中间失败后可重查同一 job 并补齐资源。LVGL 同路径缓存需在UI线程调用版本对应的图像缓存失效函数。

## 4. C桥接接入

将 `device/src/*.c` 纳入 VelaPet 主程序，只引用端侧核心的真实头文件，禁止把前端目录的重复 `vela_pet_api.h` 作为正式ABI。需 pthread、cJSON。64 KiB 工作线程栈已显式设置，内存预算由真机验证。

`VelapetAiClientConfig` 四个 HTTP 回调是板级对接点：

| 回调 | 要求 |
| --- | --- |
| post_json | POST JSON，设 Content-Type，响应NUL终止，溢出返回非0 |
| post_image | multipart四字段，同上；流式读取文件，不一次装入整图 |
| get_json | GET任务状态，响应不超过8191字节 |
| download | 下载至临时文件，验证传入的expected_sha256和expected_size后替换 |

四者完成 HTTP 往返返回0，HTTP状态码另写 http_status；网络、TLS、超时、截断、校验失败返回非0。禁止跟随到其他主机并转发Token。HTTPS必须验主机名和证书。若使用 libcurl，可参考[流式multipart文件上传](https://curl.se/libcurl/c/curl_mime_filedata.html)、[响应回调](https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html)及[证书校验](https://curl.se/libcurl/c/CURLOPT_SSL_VERIFYPEER.html)。目前未假设板上已安装libcurl。

初始化设置 base_url/device_id/device_token/locale 及四回调，调用 `velapet_ai_bridge_init()`。配置字符串和user_data必须持续有效直到deinit。设备Token不是大模型Key。

UI线程内建立约100ms定时器调用 `velapet_ai_bridge_poll_ui()`。所有桥接公共接口均在同一UI线程调用；网络线程不直接操作LVGL。退出时先停UI定时器，再deinit等待网络工作线程，然后销毁UI及业务核心。deinit等待当前网络回调/图片轮询结束，不适合放在普通页面返回按钮上。一次只允许一个任务，忙时返回-1并保留当前结果。

## 5. 前端同学需要合入的修改

补丁 `integration/frontend-ai.patch` 相对 member3 目录，先执行 `git apply --check --recount`，再合并。未替换组员原交付文件。

聊天quick_cb调用 `velapet_ai_request_reply(type)`。原mock仅保留在 `VELAPET_UI_USE_MOCK` 模式。跨页面回填只更新缓存，控件刷新函数先判断 `g_ui_page`；页面销毁时最好统一清空静态控件指针（包括task/home/skin页）。默认256字节只保证本客户端50字回复，多字节截断仍应使用UTF-8边界处理。

图片入口需要获得真实 source_path 后调用 `velapet_ai_request_pet_image(source_path)`，source_path是已存在的用户照片，不是输出路径。手表无相机/文件选择器时，先用电脑客户端上传并联调，不能用空路径伪装生成。

真实显示：在形象页创建 lv_img 对象；资源READY且实际解码成功才显示新图。把POSIX路径转换为已注册LVGL文件系统盘符路径（如 `S:/...`，仅在确有S驱动时使用），设置 `lv_img_set_src()`；对失败保留默认表宠。主页要展示同一图需保存共享资源路径并在home页也创建图像对象。更新期间文件解码和缓存失效在UI线程进行。

生成的skin_custom.png不在端侧白名单。MVP可独立作为个性化形象展示；若要“装备自定义皮肤”，端侧同学应新增注册/解锁/持久化接口并明确奖励规则，不能直接覆写current_skin。
