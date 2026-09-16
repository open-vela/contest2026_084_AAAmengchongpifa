# 部署与验收

## 部署

1. 创建CPython环境安装requirements-dev.txt，复制.env.example为.env，填入设备Token及实际后端地址。
2. `python run.py`，默认0.0.0.0:8000，单worker。Windows与开发板同网络时使用电脑局域网IP，不能填127.0.0.1。
3. `GET /health`确认mimo/local_fallback与demo/openai_compatible/volcengine_ark模式。配置变更后重启。
4. 本版本不要加`--workers 2`或以上：任务与限流在进程内，多worker会读取不一致。不要用多个进程共享同一个runtime-data。
5. 演示环境仅信任已知设备。需要公网多用户服务时，先增加独立身份认证、资源归属授权、网关上传体积限制、任务队列与存储清理策略，再部署HTTPS。当前没有执行公网部署。

原图和生成文件保存在runtime-data/assets/{job_id}，任务元数据在runtime-data/jobs。数据不会自动过期；比赛演示结束后按用户需求清理，勿把照片和.env上传仓库。任务重启时已完成仍可查询/下载，未完成转failed/SERVER_RESTARTED，需用户决定是否重新提交。

## 自动化验证

```powershell
python -m pytest -p no:cacheprovider -o tmp_path_retention_policy=all
```

2026-09-09已在Windows CPython3.12环境执行：20项通过、进程退出码0。覆盖旧7项API测试、模型空/异常JSON、MiMo、方舟VLM与Seedream Pro请求结构、供应商临时URL白名单下载、奖励意图门控、图片上游失败及持久化、下载损坏保留旧文件、跨域资源拒绝。

测试依赖出现Starlette/httpx等弃用提示，没有影响本次结果。真实MiMo请求已通过，`mimo-v2.5-pro` 可返回结构化短回复。方舟`doubao-seed-2-1-turbo-260628`已用远程URL和本地data URI完成真实图片理解请求。`doubao-seedream-5-0-pro-260628`按用户示例做纯文字2K生成时已返回HTTP 200及图片URL，证明模型已开通；后续请求返回`403 AccountOverdueError`。恢复账号计费状态后运行`python tools/check_providers.py`，成功时会把校验过的PNG保存到`VELAPET_DATA_ROOT/provider-check/ark-output.png`。

C文件以gcc C11、Wall/Wextra/Werror做语法检查通过，cJSON使用声明桩；其结果不能证明真机链接/运行。另有tests/bridge_test.c对真实pthread桥接进行主机逻辑验证，使用假的HTTP/core/UI接口；不连接网络，不操作LVGL。

本次C主机测试已运行通过：忙碌状态保护、两次鼓励只奖励一次、三个资源全部下载、失败状态收尾和deinit。`python tools/smoke_http.py`也已执行通过：临时启动真实Uvicorn服务，经本机TCP完成鉴权、聊天、上传、轮询、三图下载/哈希/尺寸检查后正常停止。没有留下后台服务。

## 演示验收顺序

1. 启动后端，无Key请求聊天，source显示local_fallback且有短回复。
2. 填MiMoKey再重启，确认source=mimo；故意断网后应回退而非卡住UI。
3. 电脑端client_demo.py上传图片，轮询到succeeded，下载三个文件并验证尺寸/哈希。
4. 同一份成功任务重复下载应命中缓存；损坏下载保留旧文件。
5. 前端合并请求接口及生命周期修复；快速切页时后台回复到达不崩溃。
6. 手表接好四个HTTP回调；抓取请求确认context来自core，不含模型Key。
7. 生成图实际显示在主页/形象页，PNG无法解码时保留默认图。
8. 同时点击多次只保留一个进行中请求；冷却期间不重复加鼓励经验。
9. 启用真实图像服务后确认provider不为demo，效果合格再录制比赛演示。

## 待外部信息

恢复方舟账号余额或后付费状态；开发板HTTP/TLS库；LVGL版本、PNG解码与文件系统盘符；可用内存/线程栈；后端实际局域网访问地址。方舟两类协议均已实现，待计费状态恢复后完成三图落盘验收；开发板信息决定真机适配。
