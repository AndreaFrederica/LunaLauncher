# Neo UI API 开发指南

本文描述 Luna Neo UI 的两套接口。它们都使用 `apiVersion: 1`，但版本号只表示
协议格式，不表示功能清单：

- **普通 API**：一次请求对应一次 `launcher/execute` 调用。
- **Neo Async API**：通过 `neo.task.*` 提交长操作，立即返回任务快照，随后由前端
  读取状态、处理交互并决定是否取消。

操作数量、参数、可选集成和异步支持必须在运行时读取，不能硬编码。启动时先调用
`api.describe`（或兼容的 `launcher/catalog`），按返回的 JSON Schema 和
`capability` 生成页面。当前源代码额外注册 `neo.task.start`、`status`、`list`、
`cancel`、`respond`、`capabilities` 六个操作；旧安装包可能尚未包含它们，应按
目录检测并降级。

## 连接宿主

Tauri 宿主启动同目录的 CLI sidecar：

```text
lunalauncher-cli.exe --dir ABSOLUTE_DATA_DIRECTORY --mcp
```

必须使用完整安装目录（Qt 库、插件、Java、PCL.Download 和平台插件都要在旁边）。
stdout 只包含协议，stderr 只用于诊断。管道采用 UTF-8、每行一个 JSON 对象；读取
端必须支持 UTF-8 分片、合并多行和进程提前退出。单个请求最大 4 MiB，写入后立即
flush。一个数据目录只能由一个 launcher 进程占用。

浏览器预览不能访问 sidecar。使用 `neo-ui/src/boot/launcher-api.ts` 中的 Tauri
adapter；独立静态 Web UI 使用 `window.luna`（见 [WEBUI-DEVELOPMENT.md](WEBUI-DEVELOPMENT.md)），
不要混用两种桥接对象。

## JSON-RPC 外壳

```json
{"jsonrpc":"2.0","id":1,"method":"launcher/execute","params":{"operation":"instance.list","parameters":{}}}
```

目录和控制方法：

```json
{"jsonrpc":"2.0","id":2,"method":"launcher/catalog","params":{}}
{"jsonrpc":"2.0","id":3,"method":"launcher/execute","params":{"operation":"api.describe","parameters":{}}}
```

成功与失败均使用业务信封：

```json
{"ok":true,"apiVersion":1,"operation":"instance.list","exitCode":0,"data":[]}
{"ok":false,"apiVersion":1,"operation":"instance.info","exitCode":2,"error":"Instance not found."}
```

JSON-RPC `error` 表示协议或参数错误；`result.ok:false` 表示操作失败。不要解析
翻译后的 `error` 文本来决定流程。目录项包含 `name`、`description`、`apiVersion`、
`inputSchema`、`destructive` 和可选 `capability`。能力组只说明代码路径存在，
不代表可选程序已安装；用 `integration.status` 检查集成，用 `runtime.info` 检查
平台能力。MCP 工具、CLI `api OPERATION JSON_OBJECT` 和 Neo UI 共用目录。

## 普通 API 使用顺序

| 场景 | 调用顺序 |
| --- | --- |
| 首页 | `launcher.get` → `instance.list` → `account.snapshot` |
| 新建实例 | `component.catalog` → `component.versions` → `instance.create` |
| 资源浏览 | `resource.providers` → `resource.search` → `resource.project` / `resource.versions` |
| 资源更新 | `resource.updates.check` → 选择 `items` → `resource.updates.apply`；放弃用 `resource.updates.discard` |
| 账户 | `account.list` / `account.snapshot` → `account.login` 或 `account.refresh` |
| 服务端 | `server.distribution.catalog` → `versions` / `resolve` → `server.distribution.install` → `server.start` |
| 设置 | `settings.list/get/set/reset`；备份恢复用 `settings.export/import` |
| 外观语言 | `appearance.catalog/select`、`language.list/select/refresh` |

资源搜索按 provider 分页（通常每页 25）；`resource.update` 只刷新本地元数据，
不是远程升级。批量更新计划十分钟过期，应用不是事务；逐项检查 `updated`、
`failed`、`cancelled` 和恢复目录。服务端 YAML 写入支持 `ifRevision`，终端写入
上限为 32 KiB，`resize` 的行列范围为 1–1000。

## Neo Async API

### 提交

```json
{"jsonrpc":"2.0","id":10,"method":"launcher/execute","params":{"operation":"neo.task.start","parameters":{"operation":"resource.install-version","parameters":{"instance":"demo","provider":"modrinth","kind":"mods","projectId":"sodium","versionId":"..."},"priority":"normal","idempotencyKey":"install-demo-sodium-v1"}}}
```

响应立即返回 `{ok:true,data:<snapshot>}`，其中 `data.id` 是任务 ID。支持列表由
`neo.task.capabilities` 返回，当前包括实例创建/导入/更新、组件版本切换、特殊
loader、Java 安装、整合包安装、资源安装（含依赖）、账户登录/刷新、实例启动/导出
和服务端启动。未知操作应显示为不支持并禁用按钮。

`idempotencyKey` 可安全重试提交；命中已有任务时会返回 `deduplicated:true` 和
原快照。`priority` 只有 `normal`、`high` 两档。

### 快照、完成和取消

```json
{"jsonrpc":"2.0","id":11,"method":"launcher/execute","params":{"operation":"neo.task.status","parameters":{"taskId":"uuid"}}}
{"jsonrpc":"2.0","id":12,"method":"launcher/execute","params":{"operation":"neo.task.cancel","parameters":{"taskId":"uuid"}}}
{"jsonrpc":"2.0","id":13,"method":"launcher/execute","params":{"operation":"neo.task.list","parameters":{}}}
```

快照可能包含 `state`（`queued`、`running`、`succeeded`、`failed`、`cancelled`）、
`finished`、`status`、`error`、`progress`、`total`、`transferRate`、`instanceId`、
`createdInstanceId`、`pid`、`conflictKey`、`cancelRequested`、`deadlineMs`、
`interaction`、`deviceCode`、`deviceUrl` 和 `deviceExpiresIn`。以 `finished` 和
`state` 判断终态，不要只依赖进度值。取消是协作式的，已完成的文件不会自动回滚。

运行时最多同时执行 6 个任务。不同资源键可并行；相同实例、账户、Java 或全局
实例集合的写入会排队。安装类任务还共享 `interactive-install` 交互通道。UI 应
显示 `queued`，不要自行实现第二个队列；冲突策略以 `neo.task.capabilities` 为准。

### 交互和设备登录

轮询快照发现 `interaction` 后，使用任务专属回复：

```json
{"jsonrpc":"2.0","id":14,"method":"launcher/execute","params":{"operation":"neo.task.respond","parameters":{"taskId":"uuid","interactionId":"interaction-uuid","value":"continue"}}}
```

取消交互传 `cancel:true`。必须回传当前 `interactionId`；过期、重复或其他任务
的回复会失败。交互可能是 `input`、`select` 或 `type:"restricted-downloads"`
的文件校验流程，只有重新扫描且校验通过才能继续。Microsoft 登录使用快照中的
`deviceUrl`、`deviceCode`、`deviceExpiresIn` 展示设备码。

## 事件和实时流

sidecar 可发送 `launcher/event`：

```json
{"jsonrpc":"2.0","method":"launcher/event","params":{"requestId":10,"kind":"task","data":{"id":"uuid","state":"running","progress":4,"totalProgress":10,"canAbort":true}}}
```

`kind` 包括 `status`、`device_code`、`task`、`input`。通知只用于即时刷新，最终
结果仍以原请求 ID 的响应为准。请求取消使用：

```json
{"jsonrpc":"2.0","method":"notifications/cancelled","params":{"requestId":10}}
```

`instance.console.subscribe`、`instance.log.subscribe`、`account.subscribe` 和
`launcher.update.subscribe` 返回订阅 ID。推送格式为 `launcher/stream`，也可用
`event.poll` 按游标读取：

```json
{"jsonrpc":"2.0","method":"launcher/stream","params":{"subscriptionId":"sub","events":[],"nextCursor":8,"dropped":0,"hasMore":false}}
```

每个订阅最多缓存 512 条或 1 MiB；`dropped > 0` 表示历史缺口。控制台和日志字节
块使用 base64，`console.line` 是已脱敏文本；轮换或截断会产生 `log.reset`。游标
属于订阅，`event.poll` 与推送消费互不影响。完成后调用 `event.unsubscribe`。

## TypeScript 客户端

仓库中的 [examples/neo-ui/launcher-api.ts](../examples/neo-ui/launcher-api.ts) 已
实现乱序响应分发、写入串行化、断管拒绝 Promise、目录读取、普通调用、取消和
交互回复。异步任务可封装为：

```ts
const started = await client.execute('neo.task.start', {
  operation: 'resource.install-with-dependencies',
  parameters: { instance: 'demo', provider: 'modrinth', kind: 'mods', projectId: 'x' },
  idempotencyKey: crypto.randomUUID(),
});
const taskId = (started.data as any).id;
for (;;) {
  const result = await client.execute('neo.task.status', { taskId });
  const task = result.data as any;
  if (task.interaction?.interactionId) await renderAndReply(task.interaction);
  if (task.finished) break;
  await new Promise(resolve => setTimeout(resolve, 250));
}
```

实际 UI 应优先监听 stream 或 Tauri `launcher-event`，轮询仅作为重连和丢事件后的
补偿。不要缓存完整操作表；每次后端升级都重新读取目录。

## 安全、兼容和验证

路径参数只能访问 launcher 允许的实例目录和已声明的本地文件。敏感设置默认脱敏，
确需读取时才使用 `reveal:true`；密码不得放在命令行、日志或 localStorage。新闻
HTML 必须在浏览器端清理后再渲染。`destructive:true` 只是目录元数据，是否需要
`confirm` 以操作 schema 为准。可选集成缺失、网络不可用和未知操作要分别展示。

```text
python tests/api_sidecar_smoke.py --exe install-api-test/lunalauncher-cli.exe
node --experimental-strip-types --test tests/launcher_api_client.test.mjs
```

涉及长任务、受限下载、交互回复和并行冲突时，再运行
`tests/neo_restricted_downloads_test.py`。具体字段和能力始终以目标安装包的
`api.describe`、`launcher/catalog`、`neo.task.capabilities` 为准。
