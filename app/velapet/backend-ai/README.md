# VelaPet 后端与 AI

适配本项目 PPT 和两份组员交接的比赛 MVP。代码含 FastAPI 后端、MiMo 文本适配、图片任务与资源服务、电脑联调客户端、手表端 C 协议客户端和异步桥接。

## 快速启动

Python 3.11 或以上（本次使用 Windows CPython 3.12 测试）。在本目录执行：

```powershell
py -3.12 -m venv .venv-win
.\.venv-win\Scripts\python.exe -m pip install -r requirements-dev.txt
Copy-Item .env.example .env
.\.venv-win\Scripts\python.exe run.py
```

已有 `.env` 时不要覆盖。虚拟环境属于本机生成内容，不纳入工程；使用 `.venv-win` 或自己已有的 CPython 环境。

打开 `http://127.0.0.1:8000/docs` 查看全部接口，`/health` 查看实际启用的模式。无 MiMo Key 时聊天返回 `source=local_fallback`；图片设为 `provider=demo` 时只是上传原图的裁剪/缩放/颜色处理，不是 AI 生成。

启用聊天：在 `.env` 设置 `MIMO_API_KEY`、`MIMO_API_BASE_URL` 和账号可用的 `MIMO_CHAT_MODEL`。Key 仅保存在后端。代码按 MiMo 的 Chat Completions 协议关闭 thinking，仅读取最终 content，不展示推理字段。

参考图理解可选用火山方舟多模态对话：设置 `ARK_VISION_ENABLED=true` 和 `ARK_VISION_MODEL=doubao-seed-2-1-turbo-260628`。该适配器调用 `/chat/completions`，输入图片和文字、输出图片描述提示词；它不是图片生成模型。该模型已完成真实 API 验证。

火山方舟图片生成使用 `VELAPET_IMAGE_PROVIDER=volcengine_ark`。当前本地 `.env` 已切换到 `doubao-seedream-5-0-pro-260628`，按 Pro 协议调用 `/images/generations`：纯文字提示词、`response_format=url`、2K、非流式。参考图先由方舟 VLM 转成细节提示词，因此仍能保留宠物特征。返回 URL 仅允许 HTTPS `*.volces.com`，后端立即限量下载并验证图片，再生成端侧三种 PNG；不会把临时供应商 URL 直接交给设备。

Pro 模型的严格示例请求已真实返回 HTTP 200 和图片 URL。随后账号返回 `403 AccountOverdueError`，需恢复方舟余额或后付费状态后运行 `python tools/check_providers.py` 完成最终落盘验收。

`openai_compatible` 适配器仍保留给其他供应商，支持 `IMAGE_API_MODE=edits` 或 `generations`，不要把它误用于方舟的图生图协议。

## 联调

后端与客户端的设备 Token 要一致：后端 `.env` 的 `VELAPET_DEVICE_TOKENS` 可设一个随机长字符串；客户端通过 `VELAPET_CLIENT_TOKEN` 使用其中一个值。它不是 MiMo Key。

```powershell
$env:VELAPET_CLIENT_TOKEN='填写设备Token'
python client_demo.py --base http://127.0.0.1:8000
python client_demo.py --base http://127.0.0.1:8000 --image C:\images\pet.png --cache .\runtime-data\client-cache
```

客户端检查版本、长度和 SHA-256，先写 `.part`，校验成功后原子替换。默认图片依次生成头像 128×128、主页图 240×240、皮肤图 160×160。上传图片需要来自你有权使用的文件。

`VELAPET_PUBLIC_BASE_URL` 留空最方便联调：返回相对下载路径，客户端使用自己的后端地址。如果填写地址，必须与设备实际访问的地址一致；示例中的 `192.168.1.100` 需要替换。

## 文档入口

- `docs/api-integration.md`：JSON 字段、C 桥接、线程和图片文件协议。
- `docs/deployment.md`：部署限制、测试与演示流程。
- `../docs/integration/ai-cloud.md`：端侧对 AI 云端联调问题的正式答复。

## 当前边界

本版本为单进程比赛原型。任务元数据落盘，进程重启会把未完成图片任务标记为失败；不提供分布式任务队列。所有配置的设备 Token 共用一个信任域，没有独立账号及图片归属隔离，不能据此作为多用户公网服务发布。

手表 C 桥接已有协议、线程和结果回填逻辑；板级 HTTP/TLS 的四个函数指针需要按开发板已有库接入。当前没有硬件工程、网络库和 LVGL 配置，不能声明真机联调完成。AI 自定义图片暂作独立展示资源，不自动加入端侧五种成长奖励皮肤白名单。

## 参考

- [MiMo 深度思考参数及官方请求示例](https://platform.xiaomimimo.com/docs/en-US/usage-guide/passing-back-reasoning_content)
- [小米官方兼容接入配置](https://github.com/XiaomiMiMo/awesome-mimo-agent/blob/main/docs/workbuddy.md)
- [火山方舟图片生成 API](https://www.volcengine.com/docs/82379/1541523)
- [火山方舟官方 Python Runtime](https://github.com/volcengine/ark-runtime-python)

示例模型名来自官方资料，可通过环境变量更换。图片输出能力与图片输入理解分开配置。
