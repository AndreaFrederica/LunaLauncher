# Tauri / Neo UI 接入

采用常驻子进程即可复用当前启动器核心。Neo UI 不需要链接 Qt；后端仍使用
Qt 和现有实例、账户、下载、设置实现。新增接口集中在 `launcher/api`，GUI
和核心任务保持原有结构，方便继续合并上游。

## 启动与打包

使用 **Meson install 并部署运行库后的完整目录**，启动其中的：

```text
lunalauncher-cli.exe --dir ABSOLUTE_DATA_DIRECTORY --mcp
```

不要只复制 build 目录中的 exe。Qt 库、插件、Java JAR、PCL.Download、WebView
及选用的联机工具需要遵循现有安装布局。Tauri 的 Rust 层或 shell 插件负责启动
进程、持有 stdin、读取 stdout/stderr，并在退出时关闭 stdin。Windows 子进程
使用隐藏窗口。一个数据目录仅由一个启动器进程管理，避免和现有 GUI 同时占用。

首次 headless 启动不会弹出旧数据迁移对话框。新目录不会自动导入或修改旧资料。

## 请求、结果和事件

协议是 UTF-8、每行一个 JSON 对象；每个请求使用唯一的字符串或数字 ID。
stdout 只承载协议，stderr 是诊断日志。输入允许分片和多行合并，每个请求限制
为 4 MiB；发送后要刷新 stdin。保持管道打开才能接收交互并管理持续任务。

读取目录：

```json
{"jsonrpc":"2.0","id":1,"method":"launcher/catalog","params":{}}
```

调用接口：

```json
{"jsonrpc":"2.0","id":2,"method":"launcher/execute","params":{"operation":"resource.search","parameters":{"provider":"modrinth","kind":"mods","query":"sodium","minecraftVersion":"1.21.1","loaders":["fabric"]}}}
```

响应的 `result` 为 `{ok, apiVersion, operation, exitCode, data}`；失败时为
`{ok:false, apiVersion, operation, exitCode, error:"说明"}`。`data` 可以是数组
或对象，取决于操作。JSON-RPC 的 `error` 表示协议问题；`result.ok=false`
表示业务操作失败，两者应分别处理。

任务运行期间会发出以下通知：

```json
{"jsonrpc":"2.0","method":"launcher/event","params":{"requestId":2,"kind":"task","data":{"id":"task-uuid","state":"running","running":true,"canAbort":true,"progress":100,"totalProgress":1000}}}
```

`task` 通知约每 100 ms 检查一次变化，短任务可能只出现在 `task.list` 的历史中。
其他通知类型是 `status`、`device_code` 和 `input`。最终结果通过原请求 ID 的
响应返回；没有单独的 started/succeeded 事件。任务 UUID 和 JSON-RPC 请求 ID
是两个不同标识，一个操作可能先后产生多个任务。

普通操作串行执行：运行中的下载或登录会使其他普通调用返回 JSON-RPC
`-32000`。以下调用仍可执行：

- `launcher/catalog`、`api.describe`、`tools/list`、`ping`。
- `task.list`、`task.status`、`task.cancel`，可以通过 native 或 MCP 调用。
- `event.poll`、`event.subscriptions`、`event.unsubscribe`，以及
  `server.console.command/write/resize`。
- `launcher/respond` 和 `notifications/cancelled`。

查询当前任务可省略 `taskId`；指定 UUID 可查询已完成任务快照。取消整个操作：

```json
{"jsonrpc":"2.0","method":"notifications/cancelled","params":{"requestId":2}}
```

其他请求 ID 的取消通知不会影响当前操作。取消是协作式的：以任务的 `canAbort`
和最终结果为准。取消任务不会自动回滚已经完成的文件写入。UI 在取消后仍应等待
原调用结束，再提交下一项普通操作。

## 登录输入与选择

需要用户名、密码或多个账户档案之间的选择时，后端发送：

```json
{"jsonrpc":"2.0","method":"launcher/event","params":{"requestId":3,"kind":"input","interactionId":"uuid","prompt":"Username","secret":false,"expiresIn":300}}
```

`secret:true` 表示密码输入；有 `choices` 数组时渲染选择器。回复文本，或选择项
从零开始的下标：

```json
{"jsonrpc":"2.0","id":4,"method":"launcher/respond","params":{"interactionId":"uuid","value":"NeoPlayer"}}
```

取消该输入时传 `cancel:true`。输入五分钟未回复会结束等待；过期/不匹配的
interactionId、错误类型或越界下标返回 `-32602`。密码通过管道传递，不放在启动
命令参数里。Microsoft 登录使用 `device_code` 的 URL、code、expiresIn 渲染引导。
现有 MCP `tools/call` 的参数和通知形式保持兼容；交互回复流程用于 native 调用。

## TypeScript 客户端

参考 [launcher-api.ts](../examples/neo-ui/launcher-api.ts)。客户端不依赖 Tauri
包；将写入函数接到子进程 stdin，将完整 stdout 行交给 `acceptLine`，在子进程
退出时调用 `close`。如果 Rust 层提供字节块，先做 UTF-8 增量解码和换行分帧。
不要把 stderr 交给 JSON 解析器。

```typescript
const client = new LauncherClient(line => child.write(line), event => {
  // 按 event.requestId 更新进度、登录引导或输入框。
}, batch => {
  // 按 batch.subscriptionId 分发控制台／日志事件并记录 nextCursor。
  // dropped > 0 表示缓冲区已有历史缺口。
});
// 注册 stdout / close 监听之后：
const catalog = await client.catalog();
const call = client.begin("component.versions", { uid: "net.minecraft" });
// 取消按钮：await call.cancel();
const result = await call.result;
if (!result.ok) showError(result.error);
```

客户端根据 ID 分发乱序响应，串行写入管道，并在子进程退出或断管时拒绝所有
未完成 Promise。它不替 UI 排队所有业务操作；通常 await 当前调用，任务控制和
输入回复可以同时发送。接口目录作为版本协商依据，避免把完整功能表硬编码进 UI。

## 功能使用顺序

| 页面 | 推荐调用 |
| --- | --- |
| 新建实例 | `component.catalog` → `component.versions` → `instance.create` |
| 资源浏览 | `resource.providers` → `resource.search` → `resource.project` / `resource.versions` |
| 资源安装/升级 | 检查版本返回的 dependencies，必要时 `resource.resolve-dependency`，再逐项 `resource.install-version` |
| 批量升级 | `resource.updates.check` → 展示 updates 并勾选 itemId → `resource.updates.apply`；取消计划用 discard |
| 世界管理 | `instance.world.list` → `instance.world.import/export/rename/delete/reset-icon` |
| 设置 | `settings.list/get/set/reset`，instance scope 同时提供 instance ID |
| 外观／语言 | `appearance.catalog/refresh/select`、`language.list/select/refresh` |
| 服务端 YAML | `server.yaml.read` → 编辑 content → `server.yaml.write`，附带读取时的 ifRevision |
| 服务端终端 | `instance.console.subscribe` → `server.start` → `server.console.write/resize`；结束用 `instance.stop/kill` |
| 日志跟随 | `instance.log.subscribe` → `launcher/stream` 通知或 `event.poll` → `event.unsubscribe` |
| 联机 | `integration.status` → install/start → host/join → state/log → leave/stop |
| 下载队列 | `aria2.downloads`、`aria2.cancel`、`aria2.clear-finished` |

## 升级计划和实时流

`resource.updates.check` 最多检查 256 个索引资源，默认只选择 release，并读取
实例 Minecraft／loader 配置。返回的 `updates` 由前端选择 itemId 后交给
`resource.updates.apply`；计划在十分钟后过期，最多保留 16 个。执行前会校验原文件、
索引和目标文件名，下载使用临时目录和既有校验和逻辑，成功后替换并保留禁用状态。
单项失败会尝试恢复原文件；批次没有整体回滚，依赖仍需单独选择安装。

`instance.console.subscribe` 和 `instance.log.subscribe` 返回 subscriptionId。服务端
终端和日志文件的字节块使用 base64，客户端 console.line 来自脱敏日志模型。
`launcher/stream` 通知与 `event.poll` 共享有界历史但游标独立；每个订阅最多保留
512 条／1 MiB，`dropped` 大于零表示需要提示日志缺口。文件轮换或截断会产生
`log.reset`。`server.console.write` 最多 32 KiB，command 会补换行，resize 范围为
1–1000。外观接口应用 Qt 后端主题，Neo UI 自己负责浏览器样式和文案翻译。

当前接口目录为 **120 项**，仍未达到所有 GUI 功能的完全覆盖。递归依赖自动安装、
世界管理、服务端软件安装、设置导入导出等缺口见
[覆盖表](LAUNCHER-API.md#current-coverage-and-remaining-gaps)。目前有些设置需重启
sidecar 才能应用到常驻服务；服务端 loader 配置也不等于安装服务端软件。
