# Web UI 开发与构建

本文适用于随启动器打包的 Neo UI，以及放进 `webui` 文件夹的独立静态界面。
所有命令从仓库根目录运行。包管理器、Node.js、Rust、Python、Meson 由 Pixi 环境提供。
原生构建入口是 Meson；前端使用 pnpm。不要直接运行尚未 install 的 C++ 可执行文件。

## 1. 一键得到可以运行的启动器

Windows 需要安装 Visual Studio 2022 的“使用 C++ 的桌面开发”和 Windows SDK，
以及 WebView2 Runtime。首次构建需要联网取得 Pixi、Qt、pnpm、Cargo 和 Meson wrap 依赖。
Qt 会由配置任务安装到 `third_party/qt`；无需全局安装 pnpm。

```powershell
pixi run install-webui
pixi run run-webui
```

第一条命令按顺序执行：安装缺少的 Qt 模块 → Meson setup/reconfigure → C++、
前端和 Tauri 编译 → Meson install → 运行库部署。pnpm 使用 frozen lockfile，
Cargo 使用 locked 构建。Windows 部署包含 Qt 插件、WebSockets 和 Multimedia DLL。
部署工具也通过 `meson devenv` 进入 MSVC 环境，以便找到编译器运行库。
分发到没有 MSVC 运行库的 Windows 机器时，可先安装随目录提供的 `vc_redist.x64.exe`。
已有安装目录内的外部 Web UI 和数据会保留；请先关闭占用安装文件的启动器。

默认目录：

| 配置 | 构建目录 | 安装目录 |
| --- | --- | --- |
| `release` | `build-meson-webui-release` | `install-webui-release` |
| `debug` | `build-meson-webui-debug` | `install-webui-debug` |

Windows 主程序为 `install-webui-release/luna-neo-ui.exe`，后端为同目录的
`lunalauncher-cli.exe`。Linux 对应 `bin/luna-neo-ui` 和 `bin/lunalauncher`；macOS
对应 `LunaLauncher.app/Contents/MacOS/` 下的两个程序，后端同样叫 `lunalauncher`。
`run-webui` 会按平台寻找正确位置。

```powershell
pixi run install-webui debug
pixi run run-webui debug
```

`debug` 目前控制 Meson/C++ 的调试配置。Meson 内的 Tauri custom target 仍按 Cargo
release 构建，前端仍是生产静态包，不代表开启 Rust 调试器或前端 HMR。

Linux 原生构建还需要系统提供 GTK3/WebKitGTK 4.1 的开发依赖；Ubuntu/Debian
通常需要 `build-essential libgtk-3-dev libwebkit2gtk-4.1-dev librsvg2-dev`。
macOS 需要 Xcode Command Line Tools。Web UI 任务只接受本机 `release/debug`，
没有配置 Windows 到 Linux 的 Tauri 交叉编译。本次完整构建和窗口测试在 Windows 验证。

### 复用已有构建目录

例如复用此前的 API 测试安装，避免另建一份 C++ 构建缓存：

```powershell
$env:LUNA_BUILD_DIR = 'build-meson-neo-ui-api'
$env:LUNA_INSTALL_DIR = 'install-neo-ui-api'
pixi run install-webui
pixi run run-webui
```

这两个变量同时影响配置、编译、安装和运行；相对路径以仓库为基准，安装 prefix
会转换为绝对路径。不要把不同编译器或 debug/release 混用到同一构建目录。
回到默认目录：

```powershell
Remove-Item Env:LUNA_BUILD_DIR, Env:LUNA_INSTALL_DIR -ErrorAction SilentlyContinue
```

### 命令速查

| 命令 | 行为 |
| --- | --- |
| `pixi run install-webui [release或debug]` | 一键配置、编译、安装、部署 |
| `pixi run build-webui [release或debug]` | 配置并编译，不安装 |
| `pixi run configure-webui [release或debug]` | 只配置 Meson，明确启用 `neo_ui` |
| `pixi run run-webui [release或debug]` | 启动已安装的 Neo UI，不触发构建 |
| `pixi run webui-deps` | 安装锁定的 pnpm 依赖 |
| `pixi run webui-check` | 安装依赖并运行 Vue/TypeScript 类型检查 |
| `pixi run webui-dev` | 启动浏览器预览，终端显示地址，Ctrl+C 结束 |
| `pixi run webui-assets` | 安装依赖、类型检查、生成独立静态包 |
| `pixi run test-webui [release或debug]` | 对已安装程序运行隔离的真实窗口测试 |

方括号表示可选参数，不要输入方括号。可用 `pixi run --dry-run install-webui debug`
检查任务依赖链。任务不自动后台启动构建或测试。

原来的 `pixi run install` 默认不编译 Neo UI；`-Dneo_ui=disabled` 也继续可用。
`*-webui` 原生任务通过 `LUNA_NEO_UI=enabled` 显式开启它。CurseForge WebView 下载器
仍是独立组件，不受这个 Neo UI 开关控制。

## 2. 前端开发循环

### 只调布局

```powershell
pixi run webui-dev
```

浏览器预览支持 Quasar 的热更新，但浏览器没有 Tauri 宿主，因此没有 `window.luna`
或真实实例/账户 API。显示“网页预览”或未连接是预期行为。需要 API 的行为在已安装
Tauri 中验证；不要为浏览器预览开放无认证的本地管理服务。

### 调真实功能，不反复编译 C++ / Rust

先安装一次宿主，然后构建纯前端并复制到一个独立目录：

```powershell
pixi run webui-assets
$bundle = Join-Path $PWD 'install-webui-release/webui/neo-dev'
New-Item -ItemType Directory -Force $bundle | Out-Null
Copy-Item -Path 'neo-ui/dist/spa/*' -Destination $bundle -Recurse -Force
pixi run run-webui
```

在 Web UI 管理窗口（Ctrl/Command+Shift+U）刷新并选择 `install/neo-dev`。
之后修改 Vue 源码，重复 `webui-assets` 和复制，点击管理窗口的“重新加载”。
复制时关闭宿主或先切回内置界面，避免页面在更新途中请求一半新、一半旧的资源。
使用了自定义安装目录时，相应修改 `$bundle`；macOS/Linux 放到可执行文件旁的
`webui`，或直接使用管理窗口显示的数据目录。

`neo-ui/dist/spa` 包含完整界面和 `webui.json`，不包含 `node_modules`、Rust 源码或
后端。发布静态包时打包这个目录的**内容**。整个目录也可以复制给其他同版本宿主。
纯静态资源更新不需要重新编译 C++ 或 Tauri；修改桥接/native 权限时才需要重建宿主。

### 使用隔离数据

```powershell
$env:LUNA_LAUNCHER_DATA = Join-Path $PWD 'neo-ui-test-data'
pixi run run-webui
```

不要同时让 Qt 和 Neo UI 的两个后端操作同一个数据目录。未设置该变量时，宿主遵循
后端的正常便携/默认目录规则。`LUNA_LAUNCHER_CLI` 可覆盖后端路径，必须指向已经
完整安装的后端；通常留空，让宿主找到旁边的程序。读不到旧实例时先检查这两个变量
和 `runtime.info` 的 `dataRoot`，不要靠显示名称判断是否打开了同一份数据。

```powershell
Remove-Item Env:LUNA_LAUNCHER_DATA, Env:LUNA_LAUNCHER_CLI -ErrorAction SilentlyContinue
```

## 3. 独立界面的文件约定

默认扫描以下两个根目录的直接子目录，加载设置可以关闭其中任意一项或增加额外目录：

```text
<luna-neo-ui 所在目录>/webui/<目录名>/index.html
<runtime.info 返回的 dataRoot>/webui/<目录名>/index.html
```

每套界面需要可读取的 `index.html`，资源留在同一目录树中。可以使用绝对 `/assets/`
或相对路径；Vue Router 使用 hash 模式。没有 history 路由回退，缺失文件返回 404。
文件由 Tauri 的本地协议提供，不需要端口或外部 HTTP 服务器。不要按 `file://` 运行。

可选的 `webui.json` 是 UTF-8 JSON（最大 64 KiB）：

```json
{
  "name": "My Launcher UI",
  "version": "1.0.0",
  "description": "独立前端",
  "apiVersion": 1,
  "nativeFrame": true
}
```

| 字段 | 约定 |
| --- | --- |
| `name` | 显示名称，省略时使用目录名 |
| `version` / `description` | 显示用字符串 |
| `apiVersion` | 宿主桥接版本，目前为 1；省略按 1，其他版本拒绝加载 |
| `nativeFrame` | 默认 true，由系统提供标题栏；自绘标题栏的 UI 使用 false |

不提供 manifest 也能加载。无入口、坏 JSON、不支持的版本会在管理窗口显示原因。
路径穿越以及逃出根目录的符号链接/junction 不可读取。UI 具有启动器 API 权限，
应视为本地可执行代码；外部网站不具有这些 IPC 权限。

`install/<目录名>` 与 `data/<目录名>` 是不同 ID；`builtin` 是内置界面。额外目录下的
UI 使用宿主返回的 `custom:…/<目录名>` ID（根据路径生成，重排目录不会改变它）。
不要自行构造 ID。选择与加载设置保存在 `<数据目录>/webui/selection.json`。
目录被移除或 manifest 失效后，下次启动回退内置。
若界面自身 JS 损坏，可使用独立管理窗口，或强制恢复：

```powershell
$env:LUNA_WEB_UI = 'builtin'
pixi run run-webui
Remove-Item Env:LUNA_WEB_UI
```

该启动选择也会保存。不同外部 UI 共享 `webui` origin，localStorage/IndexedDB 应采用
各自的命名空间；内置 UI 使用另一 origin。宿主通过 `no-store` 响应提供最新资源，
外部 UI 不应注册缓存宿主根路径的 service worker。

### 从 UI 修改加载设置

管理窗口和 Luna Deck 的“界面加载设置”页面都能编辑；其他 UI 可以通过桥接实现
自己的设置页面。这些是宿主设置，不属于 C++ 的 `settings.set` 或页面 localStorage。

| 字段 | 默认值 / 含义 |
| --- | --- |
| `rememberLastUi` | true，启动时恢复上次切换到的界面 |
| `startupUi` | `builtin`，关闭 rememberLastUi 后使用的固定启动 ID |
| `scanInstallDirectory` | true，扫描可执行文件旁的 webui |
| `scanDataDirectory` | true，扫描当前数据目录的 webui |
| `additionalDirectories` | 空数组；最多 16 个绝对路径，各 UI 放在这些路径的直接子目录 |
| `showManagerOnStart` | false，是否在启动时打开独立管理窗口 |

```js
const current = await luna.webUiSettings();
await luna.updateWebUiSettings({ rememberLastUi: false, startupUi: 'builtin' });
// 支持部分更新，未提供的字段保持原值。跨平台路径由你的设置界面输入。
const discovered = await luna.webUis();
```

更新时校验字段类型、路径和固定启动 UI，失败则拒绝 Promise，保留之前的配置；
成功后原子替换文件，再更新宿主状态。扫描结果立即生效；当前已加载页面继续使用
原来的资源目录，启动策略在下次启动生效。保存设置不必取消当前下载。
若要移除固定启动 UI 所在目录，请同时把 `startupUi` 改为 `builtin` 或另一个可用 ID。
添加目录后先保存并重新读取 `webUis()`，再选该目录中发现的界面作为固定启动项。

启动优先级为 `LUNA_WEB_UI` → 上次选择（rememberLastUi=true）或固定启动项 → 内置回退。
`webUis().startupOverride` 显示当前环境覆盖；持久设置不会修改进程环境变量。
旧版仅有 `selected` 的 selection.json 自动使用默认加载策略，仍然兼容。

## 4. 宿主桥接 API

宿主在页面脚本执行前注入 `window.luna`。任意前端框架或原生 JS 都能使用，不需要
再启动后端，不需要为基本业务调用安装 Tauri SDK。参考可运行的
[实例列表示例](../examples/webui/instance-list/index.html) 和
[TypeScript 声明](../examples/webui/luna.d.ts)。声明文件可以复制进项目的 `src/types`。
[Luna Deck](../examples/webui/luna-deck/README.md) 是可直接运行的静态前端，演示账户选择、
真实 Minecraft 启动、认证交互、任务取消、多个实例的实时控制台、停止客户端和加载设置。
它在启动前订阅日志，使用 `wait:false` 在进程启动后释放请求，通过实例状态事件跟踪退出；
`console.reset` 的 `available:false` 仅表示暂时没有日志任务，不能当作启动失败。

| 方法 | 返回 / 用途 |
| --- | --- |
| `execute(operation, parameters = {})` | `Promise<ApiResult>`，调用业务 API |
| `catalog()` | `{apiVersion, operations}`，操作名、参数 JSON Schema、能力和 destructive 标记 |
| `respond({interactionId, value})` | 回复输入/选择；取消使用 `cancel:true` |
| `onEvent(handler)` | 监听任务、状态、登录输入等，返回 Promise 包装的取消监听函数 |
| `onStream(handler)` | 监听控制台和日志批次，同样返回取消监听函数 |
| `onExit(handler)` | 监听后端断开 |
| `openPath(path)` | 系统文件管理器打开/定位绝对路径，使用宿主 opener |
| `webUis()` | `{entries, roots, active, warning, settingsPath, startupOverride}`，界面列表、扫描路径与当前加载信息 |
| `webUiSettings()` | 读取宿主的加载策略 |
| `updateWebUiSettings(patch)` | 校验并保存部分更新，返回完整的新设置 |
| `switchWebUi(id)` | 导航到所选 UI；有未完成请求时拒绝 |
| `openWebUiManager()` | 打开独立管理窗口 |

`luna.apiVersion` 为桥接版本。`catalog().apiVersion` 和业务响应里的 `apiVersion`
描述后端协议；不要以启动器版本号或固定接口数量替代能力查询。

```ts
async function call<T>(operation: string, parameters: Record<string, unknown> = {}): Promise<T> {
  if (!window.luna) throw new Error('请在已安装的 Luna Neo UI 中打开');
  const result = await window.luna.execute<T>(operation, parameters);
  if (!result.ok) throw new Error(result.error);
  return result.data;
}

const catalog = await window.luna!.catalog();
const supported = new Set(catalog.operations.map(op => op.name));
if (supported.has('instance.list')) {
  const instances = await call<Array<{id: string; name: string}>>('instance.list');
  console.log(instances.map(i => i.name));
}
```

业务失败是 `{ok:false, error, exitCode, operation, apiVersion}`；成功是
`{ok:true, data, exitCode, operation, apiVersion}`。业务失败不会自动 reject；桥接断开、
协议错误和界面切换错误会 reject。界面同时处理这两种情况，不解析翻译后的错误文本。
完整原始协议见 [NEO-UI-API.md](NEO-UI-API.md)，边界和领域见
[LAUNCHER-API.md](LAUNCHER-API.md)。不要在 Web UI 中重新实现安装、认证或下载引擎。

## 5. 页面和后端对象的对应

对象是 JSON 快照，不是可以一直调用的 C++ 指针；持久标识使用返回的实例/账户 ID。
操作成功后重新获取有关快照，监听运行状态和账户变化；不要直接写 `instance.cfg`。

| UI 功能 | 推荐接口和接入要点 |
| --- | --- |
| 启动器总览 | `launcher.get`、`runtime.info`，检查真实数据目录与能力 |
| 实例列表/管理 | `instance.list/get`，按 ID 缓存；`instance.icon` 的 `dataUrl` 可直接赋给 img.src |
| 多实例启动 | 每个实例独立保存状态和日志；`instance.launch` 默认不等待游戏退出，不用一个全局“当前实例”覆盖所有状态 |
| 账户 | `account.list/snapshot/login/configure/remove` 和 `account.subscribe`；包括 microsoft/offline/yggdrasil/unified-pass |
| Java | `java.list`、目录中 `java.*` 操作；使用后端检测和下载结果 |
| 本地 Mod/资源 | `resource.list/inspect/enable/disable/remove`，保留名称、图标、描述和 provider 元数据 |
| 下载 Mod | `resource.providers/search/project/versions` → `resource.install-version` 或依赖安装接口 |
| 整合包 | `modpack.search/project/versions/install`；传 provider/projectId/versionId，使用后端完整安装及封面流程 |
| 设置 | `settings.list/get/set/reset`，scope 为 launcher 或 instance；实例 scope 传 instance ID |
| 任务中心 | `task.list/status/cancel` 和 task 事件，按任务 ID 保存历史 |
| 日志 | `instance.console.subscribe`、`instance.log.subscribe`、`event.poll/unsubscribe` |
| 第三方工具 | 根据 `integration.status` 的实际能力启用相应功能 |

启动器和实例设置的 `category/surface` 用于分类；`surface:qt` 仅影响传统 Qt 界面，
应该放到单独的兼容设置分类。Neo UI 的 CSS 主题、密度和本地偏好由前端管理。
实例设置还要处理继承开关，例如 `OverrideMemory`，不能只提供内存数值框而省略开关。
布尔值、数字、数组按 JSON 原类型提交；空字符串与“恢复默认/继承”不等价。

## 6. 异步调用、任务进度和登录

`luna.execute` 串行排队普通操作。下载、安装、登录的 Promise 持续到整个操作结束。
任务控制和订阅轮询可穿过队列；`respond` 也不排队，否则登录会死锁。不要在同一
页面混用多套独立客户端队列去并行发普通操作。

在调用长操作之前注册监听器。`onEvent` 的事件含 `requestId` 和 `kind`；它不是
任务 UUID，也不是 interactionId。一个业务请求可能先后创建多个任务。

| kind | 前端行为 |
| --- | --- |
| `task` | 用 `event.data` 更新任务快照 |
| `status` | 更新状态文字 |
| `input` | 根据 prompt/secret/choices 显示文本、密码或选项弹窗 |
| `device_code` | 展示设备码、认证 URL 和有效期 |

收到 input 时保存 `interactionId`。文本回复 `value:string`；choices 回复从 0
开始的选项索引。用户关闭输入弹窗应发送 `respond({interactionId,cancel:true})`，
不能只隐藏窗口。不要把密码/令牌写入日志、URL、localStorage 或错误报告。

```js
const removeListener = await luna.onEvent(event => {
  if (event.kind === 'task') console.log(event.data.id, event.data.state);
  // input/device_code 应交给应用的交互弹窗处理器。
});
try {
  const result = await luna.execute('instance.import', {source: '/absolute/path/to/pack.mrpack'});
  if (!result.ok) throw new Error(result.error);
  // 此时整个导入已经完成；刷新实例列表和图标。
} finally {
  removeListener();
  // 清理该业务操作的 busy 状态与进度弹窗。
}
```

任务快照的 `steps` 是该阶段的步骤明细，`children` 是嵌套子任务树；不要把两者
当作同一个列表重复渲染。下载源信息位于 `source`，`source.kind === 'download'`
的下载任务计数按字节解释，`transferRate` 是 B/s。顶层进度可能是文件数/工作量，
没有通用的“全部任务都是字节”保证；未知单位展示比例，不根据数值大小猜 MiB。
递归读取子任务，保留 completed/queued/running 的区分，避免只显示一个总进度条。
父任务已经聚合速度时，不要再加一次子任务速度。

终态以原 `execute` 返回为整个操作完成依据；中间某个下载 100% 不表示整合包已提交。
无当前任务时 `task.status` 可以失败，不应因此无限保留模态框。短操作可能没有任务事件。
取消按钮遵循 `canAbort`，通过 execute 调用 `task.cancel`：

```js
await luna.execute('task.cancel', {taskId: snapshot.id});
```

取消后仍等待原 Promise 结束。`window.luna` 不暴露任意 JSON-RPC 通知接口；登录输入
用 respond 取消，任务用 task.cancel。完整协议客户端的 requestId 取消见协议文档。

## 7. 控制台、订阅与切换生命周期

先注册 `onStream`，再调用 `instance.console.subscribe`，最后启动实例。批次包含
subscriptionId、events、nextCursor、dropped；按 subscriptionId 分发到各个实例页面。
`console.line` 使用 text；`console.data`/文件块的 base64 字节用增量 TextDecoder
解码，不能把分块的多字节字符各自独立解码。

`event.poll({subscriptionId,after})` 对应 `execute('event.poll', {...})`，用于补读；
按事件 sequence 去重，不能把推送和轮询的同一条日志显示两遍。`dropped > 0` 表示
有历史缺口，不代表游戏崩溃。每个后端会话最多 16 个订阅；每个订阅保留有界历史。

切换界面只替换主 WebView 文档，保持宿主、后端和运行中的游戏。未完成请求期间会
拒绝切换，未保存的前端输入会丢失。新页面要获取快照，并从 `event.subscriptions`
恢复/复用需要的后端订阅；不要每次加载都创建重复订阅，也不要删除其他页面仍需要的订阅。
只移除 JS 监听器不会取消后端订阅：组件不再需要它时还要调用 `event.unsubscribe`。

不要依赖 `beforeunload` 内的异步请求一定完成。主动切换按钮可以先清理自己拥有的
订阅，再调用 `switchWebUi`；也可以持久记录自己的 subscriptionId 并在新文档接管。
后端退出时这些 ID 失效，清除旧状态并重新建立订阅。

## 8. 验证与故障排查

```powershell
pixi run webui-check
pixi run test-webui
```

`test-webui` 不隐式构建，使用已经安装的程序；首次先运行 `install-webui`。
测试创建临时数据和 WebView 目录，启动真实窗口验证静态资源、API、登录回复、
切换保护、后端连续性、选择记忆、回退以及管理窗口退出。需要图形桌面/WebView。
Rust 文件发现和路径限制测试可单独运行：

```powershell
pixi run cargo test --offline --release --features custom-protocol --manifest-path neo-ui/src-tauri/Cargo.toml --target-dir build-meson-webui-release/neo-ui/cargo-target
```

| 现象 | 检查 |
| --- | --- |
| `window.luna` 不存在 | 是否在普通浏览器；宿主是否已更新为支持外部 UI 的版本 |
| 管理窗口没有新 UI | 入口是否恰好为 `webui/名字/index.html`，是否多套了一层 dist；刷新列表 |
| 无样式/资源 404 | 是否复制完整 dist 内容，构建 base 路径是否正确，是否用了 history 路由 |
| 双标题栏 | 使用自绘标题栏时 manifest 设置 `nativeFrame:false` |
| 无实例/找错目录 | 检查 runtime.info.dataRoot、LUNA_LAUNCHER_DATA 和 LUNA_LAUNCHER_CLI |
| 无法切换/一直 busy | 检查未回复的 input、未结束的下载；查询 task.list 并按 canAbort 取消 |
| pnpm 询问重装 node_modules | 使用这些 Pixi/Meson 任务，它们给依赖安装设置 CI=true；不要混用全局 pnpm |
| 安装无法覆盖 EXE/DLL | 关闭占用该安装目录的启动器后重新执行 install-webui |
| Rust 错用 link.exe | 用 Visual Studio C++ 工具链，并通过 Meson/Pixi 构建；检查全局 PATH 的同名工具 |

## 9. 源码入口、扩展和版本约定

| 文件 | 职责 |
| --- | --- |
| `pixi.toml` | 用户入口及任务依赖链 |
| `scripts/pixi_meson.py` | 平台工具链、Meson 选项、安装和 Qt 部署 |
| `scripts/pixi_webui.py` | 前端依赖、静态包、运行和窗口测试入口 |
| `neo-ui/meson.build` / `scripts/build_neo_ui.py` | 将 pnpm + Cargo 纳入可选 Meson target |
| `neo-ui/src-tauri/src/webui.rs` | 目录扫描、本地协议、选择持久化和切换 |
| `neo-ui/public/webui-bridge.js` | 对外 window.luna 协议 |
| `neo-ui/src-tauri/src/main.rs` | 后端常驻进程与 IPC 分发 |
| `neo-ui/src/boot/launcher-api.ts` | 内置 Vue 界面的客户端 |
| `launcher/api` | 领域 API 和对象快照，复用 C++ 核心实现 |

新增前端源码/资源文件时，同步登记 `neo-ui/meson.build` 的 `neo_ui_sources`，让
Meson 增量构建追踪它。只新增页面通常无需更改 C++；领域功能应通过 API 扩展，
继续保留原来的 Qt 实现和目录结构，以便合并上游。

桥接的向后兼容扩展保留 apiVersion 1；需要破坏既有调用约定时才升级版本和宿主校验。
后端输入参数以运行中的 catalog 为准，读取者应容忍新增字段。新功能同时更新桥接
声明、本文、示例和相关真实调用测试。重用 XMCL/Vue 源码时保留对应 MIT 许可证及
版权声明；遵循仓库现有第三方许可清单，不移除启动器自身的许可证。
