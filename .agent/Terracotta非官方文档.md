# Terracotta 集成指南

本指南面向启动器开发者，完整总结集成 Terracotta 所需的调用方式、接口、平台差异与最佳实践。

## 目标与适用范围

- 通过子进程启动 Terracotta，并与其 Web API 或脚手架（TCP）协议交互。
- 适用于 Windows、Linux、macOS（含守护进程模式）。

## 获取与启动

- 获取二进制：在 Releases 下载各平台发行包。
- 默认行为：无参数启动会启动本地 Web 服务（动态端口）并打开浏览器作为 UI。
  - 入口函数与参数解析：`src/main.rs:148`、`src/main.rs:227`
  - Web 服务器启动与端口分配：`src/server/mod.rs:13`、`src/server/mod.rs:27`

## 启动模式与命令行

- 无参数：普通模式，自动打开 UI。`src/main.rs:229`、`src/main.rs:382`
- `--help`：打印帮助信息。`src/main.rs:234`
- `--hmcl <file>`：HMCL 集成模式，端口就绪后将 `{"port": <number>}` 原子写入到 `<file>`。`src/main.rs:247`、`src/main.rs:532`
  - Windows 内部委托 `--hmcl2` 防止文件锁定冲突。`src/main.rs:251`、`src/main.rs:269`
- macOS `--daemon`：守护进程模式，通过 `launchctl` 注册，再以 Secondary 模式连接 UI。`src/main.rs:283`、`src/main.rs:314`、`src/main.rs:355`

## 端口获取与 UI 集成

- `--hmcl` 文件格式：`{"port": 12345}`，由 Terracotta 端口就绪后原子写入（先写 `.tmp` 后重命名）。`src/main.rs:532`
- 建议流程：
  - 启动：`terracotta --hmcl <绝对路径/port.json>`
  - 轮询等待文件存在并成功解析（10–15s 超时）
  - 嵌入 UI：加载 `http://127.0.0.1:<port>/` 到你的 WebView，或外部浏览器打开。`src/main.rs:446`、`src/main.rs:476`

## Web API（HTTP）

- 基础信息：
  - `GET /meta` → 版本、编译时间、EasyTier 版本、脚手架端口等。`src/server/mod.rs:35`
  - 字段示例：`version`、`compile_timestamp`、`easytier_version`、`yggdrasil_port`、`target_*`
- 状态与场景：`src/server/states.rs:8`
  - `GET /state` → 返回当前状态 JSON（详见 `src/controller/api.rs:52` 状态序列化）
  - `GET /state/ide` → 切换为“等待”状态。`src/server/states.rs:13`
  - `GET /state/scanning?room=<code>&player=<name>` → 主机扫描/拉起。`src/server/states.rs:18`
  - `GET /state/guesting?room=<code>&player=<name>` → 访客加入房间。`src/server/states.rs:24`
- 管理与诊断：`src/server/mod.rs:9`
  - `GET /log?fetch=true` → 下载日志文件；macOS 不带 `fetch` 时默认打开目录。`src/server/mod.rs:9`
  - `GET /panic?peaceful=true` → 平滑退出当前实例；否则触发 panic。`src/server/mod.rs:23`
- 静态资源与主页：嵌入 7z 包，内存解压与 60s 缓存。`src/server/statics.rs:42`、`src/server/statics.rs:87`

### HTTP 接口详解

- `GET /meta`
  - 描述：返回版本与运行环境信息。
  - 查询参数：无
  - 成功响应：`200 application/json`
    - 字段：`version:string`、`compile_timestamp:string`、`easytier_version:string`、`yggdrasil_port:number`、`target_tuple:string`、`target_arch:string`、`target_vendor:string`、`target_os:string`、`target_env:string`
    - 示例：
      ```json
      {
        "version": "1.2.3",
        "compile_timestamp": "1730000000000",
        "easytier_version": "0.8.0",
        "yggdrasil_port": 13448,
        "target_tuple": "x86_64-pc-windows-msvc",
        "target_arch": "x86_64",
        "target_vendor": "pc",
        "target_os": "windows",
        "target_env": "msvc"
      }
      ```
  - 参考：`src/server/mod.rs:35`

- `GET /log?fetch=<bool>`
  - 描述：获取日志文件或在 macOS 打开日志目录。
  - 查询参数：`fetch`（可选，默认 `false`）
  - 成功响应：
    - macOS 且 `fetch=false`：`204 No Content`（系统打开日志目录）
    - 其他情况或 `fetch=true`：`200 application/octet-stream`（日志文件内容）
  - 失败响应：`500 Internal Server Error`
  - 参考：`src/server/mod.rs:9`

- `GET /panic?peaceful=<bool>`
  - 描述：触发退出或 panic。
  - 查询参数：`peaceful`（可选，默认 `false`）
  - 行为：
    - `peaceful=true`：进程立即退出；客户端可能观察到连接重置或 `502 Bad Gateway`（用于热切换检测）。
    - 其他：触发 panic（开发调试用）。
  - 参考：`src/server/mod.rs:23`、`src/main.rs:510`

- `GET /.well-known/appspecific/com.chrome.devtools.json`
  - 描述：保留接口，始终返回 NotFound
  - 响应：`404 Not Found`
  - 参考：`src/server/mod.rs:67`

- `GET /<path..>`（静态资源）
  - 描述：返回嵌入静态页面与资源；根路径会映射到 `_.html`。
  - 缓存：首次请求加载到内存，60 秒无访问自动释放缓存。
  - 参考：`src/server/statics.rs:42`、`src/server/statics.rs:87`

- `GET /state`
  - 描述：返回应用状态 JSON。
  - 响应：`200 application/json`
  - 参考：`src/server/states.rs:8`、`src/controller/api.rs:15`

- `GET /state/ide`
  - 描述：切换为“等待”状态。
  - 响应：`200 OK`
  - 参考：`src/server/states.rs:13`

- `GET /state/scanning?room=<code>&player=<name>`
  - 描述：主机模式，开始扫描并拉起服务。
  - 查询参数：`room`（可选）、`player`（可选）
  - 响应：`200 OK`（异步执行）
  - 参考：`src/server/states.rs:18`、`src/controller/api.rs:106`

- `GET /state/guesting?room=<code>&player=<name>`
  - 描述：访客模式，加入指定房间。
  - 查询参数：`room`（必填，有效房间码）、`player`（可选）
  - 成功响应：`200 OK`；失败：`400 Bad Request`
  - 参考：`src/server/states.rs:24`、`src/controller/api.rs:137`

### 状态对象字段（`GET /state`）

- `waiting`
  - 示例：`{"state":"waiting","index":123}`
- `host-scanning`
  - 示例：`{"state":"host-scanning","index":124}`
- `host-starting`
  - 字段：`room:string`、`index:number`
  - 示例：`{"state":"host-starting","index":125,"room":"ABCD-EF"}`
- `host-ok`
  - 字段：`room:string`、`profile_index:number`、`profiles:Profile[]`
  - `Profile`：`{ "machine_id":string, "name":string, "vendor":string, "kind":"HOST"|"GUEST" }`
  - 示例：
    ```json
    {
      "state": "host-ok",
      "index": 200,
      "room": "ABCD-EF",
      "profile_index": 1,
      "profiles": [
        {"machine_id":"host-mid","name":"Host","vendor":"HMCL","kind":"HOST"},
        {"machine_id":"guest-mid","name":"Alice","vendor":"HMCL","kind":"GUEST"}
      ]
    }
    ```
- `guest-connecting`
  - 字段：`room:string`
- `guest-starting`
  - 字段：`room:string`、`difficulty:"UNKNOWN"|"EASIEST"|"SIMPLE"|"MEDIUM"|"TOUGH"`
- `guest-ok`
  - 字段：`url:string`（`127.0.0.1` 或 `127.0.0.1:<port>`）、`profile_index:number`、`profiles:Profile[]`
- `exception`
  - 字段：`type:number`（`0`=PingHostFail、`1`=PingHostRst、`2`=GuestEasytierCrash、`3`=HostEasytierCrash、`4`=PingServerRst、`5`=ScaffoldingInvalidResponse）

## 应用状态 JSON（概要）

- 状态序列化位于：`src/controller/api.rs:52`
- 常见状态：
  - `waiting`：空闲
  - `host-starting` / `host-ok`：主机启动中/已就绪，字段含 `room`、`port` 等
  - `guest-starting` / `guest-ok`：访客连接中/已就绪，字段含 `url`、`profiles` 等
- `exception`：异常，`type` 为枚举值（如网络不可达、进程崩溃等），详见 `src/controller/states.rs:79`

### 状态机详解与触发条件

- 通用字段
  - `index:number`：单调递增的状态版本号，每次状态改变或共享更新都会 +1。
  - `profile_index:number`：共享更新计数（如只更新资料列表时），仅在 `host-ok`/`guest-ok` 出现。
  - 轮询建议：客户端持有上次 `index`，若新值不同则刷新 UI；若仅 `profile_index` 变化，则只刷新资料列表。

- `waiting`
  - 说明：空闲状态；可以受理新的场景请求。
  - 进入方式：启动后默认；或通过 `GET /state/ide`。
  - 后续操作：调用 `GET /state/scanning`（主机）或 `GET /state/guesting`（访客）。

- `host-scanning`
  - 说明：主机模式扫描本地 Minecraft 广播与端口，自动寻找待托管的服务器。`src/controller/api.rs:106`
  - 进入方式：`GET /state/scanning?room=&player=`（`room` 可选，自定义房间码；`player` 可选，主机显示名称）。`src/server/states.rs:18`
  - 演进：找到端口后转入 `host-starting`，随后进入 `host-ok`。
  - 退回：若未找到或出错，进入 `exception`。

- `host-starting`
  - 说明：主机模式启动 EasyTier 与脚手架，准备服务；包含 `room` 字段。`src/controller/api.rs:25`
  - 演进：成功后进入 `host-ok`。
  - 失败：进入 `exception`（如 EasyTier 崩溃、找不到脚手架）。

- `host-ok`
  - 说明：主机模式已就绪；包含 `room`、`profiles` 与 `profile_index`。
  - 资料列表：主机为第一项（`kind=HOST`），访客进入时追加（`kind=GUEST`），超时自动移除。`src/controller/rooms/experimental/room.rs:214`
  - 访客心跳：通过 TCP `c:player_ping` 更新或追加资料。`src/controller/rooms/experimental/protocols.rs:52`
  - 维护：若 EasyTier 崩溃或脚手架失联，进入 `exception`。

- `guest-connecting`
  - 说明：访客模式的连接准备阶段；包含 `room`。
  - 进入方式：`GET /state/guesting?room=&player=`（仅当当前为 `waiting` 才受理）。`src/controller/api.rs:137`
  - 演进：拉起 EasyTier 后进入 `guest-starting`。

- `guest-starting`
  - 说明：访客模式正在建立与主机的网络连通；包含 `difficulty`（`UNKNOWN`|`EASIEST`|`SIMPLE`|`MEDIUM`|`TOUGH`）。`src/controller/api.rs:52`
  - 演进：发现脚手架与 MC 端口后进入 `guest-ok`。
  - 失败：进入 `exception`（如 EasyTier 崩溃或无法连接脚手架）。

- `guest-ok`
  - 说明：访客模式已就绪；返回 `url`（`127.0.0.1` 或 `127.0.0.1:<port>`）以供 Minecraft 连接。`src/controller/api.rs:67`
  - 资料列表：包含已知玩家资料，`profile_index` 增加表示资料更新。
  - 维护：若连接断开或 MC 端口丢失，可能转入 `exception`。

- `exception`
  - 说明：发生错误或中断；字段 `type:number` 映射如下：`src/controller/states.rs:79`
    - `0` PingHostFail：无法找到或连接主机脚手架
    - `1` PingHostRst：与主机的连接复位/丢失
    - `2` GuestEasytierCrash：访客端 EasyTier 崩溃
    - `3` HostEasytierCrash：主机端 EasyTier 崩溃
    - `4` PingServerRst：Minecraft 服务器连接复位/丢失
    - `5` ScaffoldingInvalidResponse：脚手架响应非法
  - 恢复：调用 `GET /state/ide` 回到 `waiting`，或重新发起场景请求。

### 请求使用指南（状态流转）

- 主机模式（推荐）
  - 步骤：
    1. 确认 `GET /state` 返回 `waiting`
    2. 调用 `GET /state/scanning?room=<可选>&player=<可选>`
    3. 轮询 `GET /state` 直到 `host-ok`；期间可能短暂出现 `host-scanning`、`host-starting`
    4. 显示房间码与资料列表；必要时提供导出日志与异常恢复
  - 失败处理：若进入 `exception`，提示用户并可 `GET /state/ide` 后重试。

- 访客模式（推荐）
  - 步骤：
    1. 确认 `GET /state` 返回 `waiting`
    2. 调用 `GET /state/guesting?room=<必填>&player=<可选>`
    3. 轮询 `GET /state` 直到 `guest-ok`，读取 `url` 并指示 Minecraft 连接
    4. 根据 `profile_index` 更新资料列表展示
  - 失败处理：若进入 `exception`，提示用户并可 `GET /state/ide` 后重试。

- 通用建议
  - 轮询间隔：建议 500–1000ms；异常时退避重试。
  - 并发请求：`/state/guesting` 与 `/state/scanning` 互斥，当前非 `waiting` 会返回失败（`guesting` 为 `400`）。
  - 热切换：版本更新时使用 `GET /panic?peaceful=true` 触发旧实例退出后接管；客户端可在 1.5s 后重试连接。`src/main.rs:517`

## Secondary 热切换与版本接管

- 若检测到旧实例的 `compile_timestamp` 早于当前版本，发起 `/panic?peaceful=true` 请求使旧实例退出，并接管服务器。`src/main.rs:486`、`src/main.rs:510`

## 脚手架服务（TCP）

- 端口：`yggdrasil_port`，默认尝试 `13448`，失败则随机端口。`src/controller/mod.rs:13`
- 请求报文格式：`src/scaffolding/client.rs:105`
  - 1 字节：`kind` 长度（`namespace:path`）
  - N 字节：`kind` 字符串内容（例如 `c:ping`）
  - 4 字节：请求体长度（大端 `u32`）
  - M 字节：请求体
- 响应报文格式：`src/scaffolding/server.rs:60`
  - 1 字节：`status`（`0`=OK，`255`=服务端错误，其它值=业务失败）
  - 4 字节：响应体长度（大端 `u32`）
  - K 字节：响应体
- 已实现协议：`src/controller/rooms/experimental/protocols.rs:18`
  - `c:ping`：回显请求体（用于连通性与指纹校验）
  - `c:protocols`：以 `\0` 分隔的协议列表（如 `c:ping\0c:server_port...`）
  - `c:server_port`：若主机已就绪，返回 2 字节端口（大端）；否则 `status=32`
  - `c:player_ping`：访客心跳（JSON：`{"name","machine_id","vendor"}`），成功则 `status=0`
  - `c:player_profiles_list`：返回访客列表（JSON 数组），仅在 `HostOk` 状态可用
- 连接与校验示例：`src/controller/rooms/experimental/room.rs:318`
- 发送 `c:ping` 并校验 16 字节指纹匹配，确认脚手架服务已准备好

### TCP 协议详解

- 通用格式
  - 请求：`[len:1][kind:len][body_len:4][body:body_len]`，`kind="<namespace>:<path>"`
  - 响应：`[status:1][len:4][body:len]`，`status=0` 成功，`255` 服务端错误，其它为业务错误码
  - 参考：`src/scaffolding/client.rs:120`、`src/scaffolding/server.rs:60`

- `c:ping`
  - 请求体：任意字节串（建议 16 字节指纹）
  - 成功响应体：原样回显

- `c:protocols`
  - 请求体：空
  - 成功响应体：以 `\0` 分隔的 `namespace:path` 列表

- `c:server_port`
  - 请求体：空
  - 成功响应体：`2` 字节端口（大端 `u16`）
  - 业务失败：`status=32`（主机未就绪）

- `c:player_ping`
  - 请求体：JSON `{ "name":string, "machine_id":string, "vendor":string }`
  - 成功：`status=0`、空体
  - 失败：`255` 或业务错误（如状态非法）

- `c:player_profiles_list`
  - 请求体：空
  - 成功响应体：JSON 数组 `[{"name","machine_id","vendor","kind"}]`
  - 状态要求：主机处于 `HostOk`

## 可选接口：JNI（嵌入式集成）

- 适用：需要在 Java/Android 环境直接调用核心逻辑。
- 接口列表与行为：`src/lib.rs:268`
  - `jni_get_state(): String` → 返回 `GET /state` 同款 JSON 字符串
  - `jni_set_waiting(): void` → 等价于 `GET /state/ide`
  - `jni_set_scanning(room: String|null, player: String|null): void` → 等价于 `GET /state/scanning`
  - `jni_set_guesting(room: String, player: String|null): boolean` → 等价于 `GET /state/guesting`，返回受理与否
  - `jni_verify_room_code(room: String): int` → 返回房间类型：`1`=Legacy、`2`=PCL2CE、`3`=Experimental，`-1`=无效。`src/lib.rs:301`
  - `jni_get_metadata(): String` → 返回 `"<version>\0<compile_ts_i64>\0<easytier_version>"`。`src/lib.rs:316`
  - `jni_prepare_export_logs(): jlong` / `jni_finish_export_logs(ptr: jlong): void` → 导出日志的文件句柄管理。`src/lib.rs:324`
  - `jni_panic(): void` → 手动触发 panic。`src/lib.rs:346`

## 日志与工作目录

- 根目录：
  - macOS：`~/terracotta`；其他平台：`%TEMP%/terracotta`。`src/main.rs:104`
  - 工作子目录：`YYYY-MM-DD-HH-MM-SS-<pid>`。`src/main.rs:119`
- 日志文件：`application.log`，非调试构建重定向 stdout/stderr 到该文件。`src/main.rs:538`
- 清理策略：过期目录自动清理（调试 120s，发布 24h）。`src/main.rs:597`

## 平台差异

- Windows：`windows_subsystem = "windows"`，无控制台窗口；存在控制台附着与释放逻辑。`src/main.rs:1`、`src/main.rs:158`
- Linux：桌面入口 `Exec=terracotta`。`build/linux/terracotta.desktop:3`
- macOS：守护进程 `LaunchAgent` 安装与触发。`build/macos/scripts/postinstall:4`、`src/main.rs:314`

## 推荐集成流程

1. 启动 Terracotta：`terracotta --hmcl <绝对路径/port.json>`
2. 轮询端口文件出现并解析 `port`
3. 打开 UI 或在 WebView 嵌入：`http://127.0.0.1:<port>/`
4. 驱动场景：
   - 主机：`GET /state/scanning?room=&player=`
   - 访客：`GET /state/guesting?room=&player=`
5. 版本切换或退出：`GET /panic?peaceful=true`
6. 诊断：`GET /meta`、`GET /log?fetch=true`

## 健壮性与错误处理建议

- 端口文件读取：等待文件存在后再读取，避免独占锁；读失败重试。
- 超时与重试：端口文件等待 10–15s；HTTP 请求设置合理超时与重试。
- 脚手架校验：首次连接使用 `c:ping` 指纹校验，确保服务已就绪。`src/controller/rooms/experimental/room.rs:318`
- 兼容旧实例：依赖 Secondary 热切换逻辑，无需手动杀进程。`src/main.rs:486`

## 示例代码片段

### PowerShell（Windows）

```powershell
# 启动并等待端口
$portFile = "$env:TEMP\terracotta_port.json"
Start-Process -FilePath "terracotta.exe" -ArgumentList "--hmcl", $portFile
for ($i=0; $i -lt 150; $i++) {
  if (Test-Path $portFile) { break }
  Start-Sleep -Milliseconds 100
}
$port = (Get-Content $portFile | ConvertFrom-Json).port

# 访客加入
Invoke-WebRequest "http://127.0.0.1:$port/state/guesting?room=$room&player=$name" | Out-Null
Start-Process "http://127.0.0.1:$port/"
```

### Java（伪代码）

```java
Process p = new ProcessBuilder("terracotta", "--hmcl", portPath).start();
int port = waitAndReadPortJson(portPath);
httpGet("http://127.0.0.1:"+port+"/state/scanning?room="+code+"&player="+name);
openInWebView("http://127.0.0.1:"+port+"/");
```

### Node.js（示例）

```js
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const portFile = path.join(require('os').tmpdir(), 'terracotta_port.json');
spawn('terracotta', ['--hmcl', portFile], { detached: true });

function readPort(timeoutMs = 15000) {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    if (fs.existsSync(portFile)) {
      return JSON.parse(fs.readFileSync(portFile, 'utf8')).port;
    }
    Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 100);
  }
  throw new Error('Port file timeout');
}

const port = readPort();
require('node-fetch')(`http://127.0.0.1:${port}/state/guesting?room=${code}&player=${name}`);
```

## 许可与合规

- 许可：AGPLv3 或更高版本，含例外（见 `README.md` 许可章节）。`LICENSE`
- 合规建议：不要在日志或端口文件中写入敏感信息；UI/集成处展示 Terracotta 版权信息（依例外条款）。

## 快速索引

- 入口与参数解析：`src/main.rs:148`、`src/main.rs:227`
- Web 服务与端口回调：`src/server/mod.rs:13`、`src/server/mod.rs:27`
- UI 打开/端口写文件：`src/main.rs:446`、`src/main.rs:532`
- Secondary 热切换：`src/main.rs:486`、`src/main.rs:510`
- 静态资源与状态路由：`src/server/statics.rs:42`、`src/server/states.rs:8`
- 脚手架协议：`src/scaffolding/server.rs:15`、`src/scaffolding/client.rs:105`、`src/controller/rooms/experimental/protocols.rs:18`
