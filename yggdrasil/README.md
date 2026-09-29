# Yggdrasil Rust 组件

这是 Luna Launcher 工程内的独立 Rust workspace。它提供本地增强离线身份、Yggdrasil/authlib-injector HTTP 服务、多上游联邦、资源反向缓存和 `/api/v1` 管理 API。整个 `yggdrasil/` 目录可以单独复制、编译和部署，无需 Qt、Tauri 或启动器源码。

## 构建与运行

在此目录执行（Rust 1.91+，原生 C 工具链用于 SQLite）：

```sh
cargo build --locked --release -p yggdrasil-server
cargo test --locked --workspace -- --test-threads=2
cargo clippy --locked --workspace --all-targets -- -D warnings
```

Windows PowerShell 首次初始化：

```powershell
$env:YGGDRASIL_ADMIN_PASSWORD = 'replace-with-a-long-local-password'
./target/release/yggdrasil-server.exe --config config.example.toml
```

Linux/macOS：

```sh
YGGDRASIL_ADMIN_PASSWORD='replace-with-a-long-local-password' \
  ./target/release/yggdrasil-server --config config.example.toml
```

默认监听 `127.0.0.1:25585`，数据写入 `data/`。管理员密码只在没有管理员时用于初始化，随后通过管理 API 更改。退出后可移除环境变量。`public_url` 必须是客户端能够访问的 API 根地址；它与监听地址分开，支持在反向代理后部署。程序不信任客户端提供的 Forwarded/X-Forwarded-*；联机 IP 校验使用直接连接的地址。通过反向代理部署时不要依赖 hasJoined 的客户端 IP 参数。

启动器内的“添加增强离线账号”使用子进程模式，监听临时回环端口，数据位于启动器数据目录的 `yggdrasil/`。管理令牌仅经匿名管道传给启动器，不写入账号文件或进程参数。关闭启动器后服务退出；需要独立联机服务器或持续服务时使用上述独立部署模式。局域网/远程服务器不能访问另一个机器的 `127.0.0.1`，请改用独立服务及双方可达的 `public_url`。

## 启动器打包

服务默认不加入旧构建配置，显式启用：

```sh
meson configure <build-dir> -Dyggdrasil=enabled
meson compile -C <build-dir> yggdrasil-server lunalauncher
meson install -C <build-dir>
```

CMake 对应 `-DLAUNCHER_BUILD_YGGDRASIL=ON`。两种构建均将 `yggdrasil-server[.exe]` 安装到启动器可执行文件旁。交叉编译时需关闭此选项并单独为目标构建 Rust 服务，避免打包宿主程序。也可手动将 Cargo release 可执行文件放到启动器旁。

启动器账号页新增入口支持离线 UUID、指定 UUID、PNG 皮肤/披风和 slim 模型；高级角色方案及多角色通过管理 API 使用。服务启动或重启后，账号认证会更新临时 URL。原有微软账号、普通离线账号和所有权判定逻辑沿用工程设置。本地皮肤需要启动器已有的 authlib-injector 组件；首次离线前先通过启动器安装该组件和游戏依赖。

## 启动器元数据缓存与正版账号本地接管

设置 → **Yggdrasil Service**（Yggdrasil 服务）中的“Enable metadata cache and the local service”控制此功能（`LocalServiceCache`，默认开启；需要已安装本组件）。本地服务由元数据请求或账号同步按需启动，不要求选择增强离线账号。

元数据链路为“启动器 → 本地 Rust 缓存 → 启动器配置的 HTTP/SOCKS 代理（可选）→ 原元数据源”。保留 `MetaURLOverride` 的真实值，仅运行时改写请求地址。源地址改变会使用不同缓存路由；启动器与本地服务之间不走外部代理。系统代理按目标 URL 解析；Prism 元数据与纹理可使用不同代理。

元数据默认最多新鲜 5 分钟，遵守上游更短的 TTL；连接失败或 5xx 时，允许使用新鲜期结束后 30 天内的缓存。每次上游请求最长 4 秒，服务端不额外重试；当前为同步重验证，没有后台 stale-while-revalidate。401/403 不使用旧缓存替代。只有取过的内容可离线使用；元数据中的绝对资源 URL 不会自动代理，游戏依赖仍由启动器现有下载系统管理。

正版账号成功登录/刷新并获取所有权和角色资料后，将 UUID、名字、皮肤、当前披风、历史所有权结果及验证时间保存为私有快照。直接复用刚获取的数据，不重新请求微软认证。快照不包含微软、Xbox、Minecraft 的访问令牌或刷新令牌，也不自动创建本地可登录账号。纹理请求同时经过本地缓存。启动器在后台提前下载并校验 authlib-injector；首次断网前仍需准备好游戏和 Java。

启动时无法连接官方认证服务，且当前账号有已验证的快照时，会出现三个选择：

- **Use Local Authentication**：由本地服务从快照创建相同 UUID/名字的身份，签发独立本地令牌，通过 authlib-injector 提供签名皮肤/披风。游戏会话保持 `LaunchMode::Normal`，不调用传统 `MakeOffline`；启动前资源检查使用本地文件，不强制更新。
- **Traditional Offline Mode**：保留原有离线启动行为。
- **Cancel**：取消启动。

接管仅作用于本次游戏会话，不替换账号列表中的正版账号或写回微软令牌。下次正常启动会再次尝试官方认证。明确的凭据错误不会触发接管；所有权请求失败不会当作新的成功验证写入快照。移除账号会删除快照和关联的本地身份、令牌。UUID 或名字与已有独立本地角色冲突时明确报错，不覆盖其所有权。

本地认证可用于单机和信任该服务的游戏服务器，不会让第三方正版服务器接受本地令牌。内置服务只监听本机；跨机器联机需要双方可达的独立部署。较新版本安全聊天证书仍未实现。真实游戏版本兼容性需单独联调。

新增管理接口（均需要管理员认证，响应 `Cache-Control: no-store`）：

| 接口 | 用途 |
| --- | --- |
| `PUT /api/v1/launcher/cache` | 配置元数据与官方纹理上游，返回稳定缓存路径；重复配置保留缓存 |
| `PUT/GET/DELETE /api/v1/account-snapshots/{uuid}` | 保存、读取、删除按官方 UUID 隔离的快照 |
| `POST /api/v1/account-snapshots/{uuid}/session` | 用户选择接管后签发本地认证会话；快照自身不是官方所有权证明 |

独立组件可通过这些接口接受可信宿主已验证的资料，也可以继续使用 `profiles/{uuid}/mirror/minecraft` 在服务端验证一次性官方令牌。

回归测试：Rust `crates/yggdrasil-server/tests/launcher.rs` 覆盖快照、会话、签名纹理和断网缓存；Qt `AccountSnapshot_test` 覆盖凭据隔离与会话模式；`python tests/local_yggdrasil_smoke.py <launcher-cli>` 使用回环假源验证真实启动器和 Rust 子进程集成，不需要真实微软账号。

## 管理 API

OpenAPI 3.1 文档：`GET /api/openapi.json`。资源字段由 Rust DTO 生成；分页采用 `limit`（1–200）和 `cursor`（上一页的 `next_cursor`），返回 `{items,next_cursor}`。

```http
POST /api/v1/auth/login
Content-Type: application/json

{"username":"admin","password":"your-password"}
```

响应中的 `token` 用于后续 `Authorization: Bearer <token>`。浏览器也可使用响应的 HttpOnly/SameSite=Strict cookie；cookie 方式的写请求必须携带与 `public_url` 相同的 Origin。HTTPS public URL 会设置 Secure cookie。不开放跨域 CORS。

服务默认提供基础自助注册：

```http
POST /api/v1/auth/register
Content-Type: application/json

{"username":"Alice","password":"a-long-local-password","profileName":"Alice","requestUser":true}
```

注册会原子创建本地用户、可选的离线 UUID 角色，并直接返回 Yggdrasil `accessToken`/`clientToken`，随后可以使用 `/authserver/authenticate`、`/sessionserver` 和皮肤接口。`profileName` 可省略；此时需由管理员创建角色后才能游戏。公开注册只允许服务端生成离线玩家 UUID，不能由用户声明正版 UUID；固定 UUID、正版镜像和改名由管理员 API 管理。`registration_enabled = false` 可关闭自助注册，关闭后仍可由管理员通过 `POST /api/v1/users` 创建用户。独立模式默认开启注册，启动器 `--launcher` 模式强制关闭，由启动器通过私有接口管理账号。

注册按直连客户端 IP 和用户名分别限制每分钟 10 次尝试，不信任转发 IP 头；放在反向代理后，所有注册共享代理 IP 的额度。嵌入 Router 的宿主应提供 `ConnectInfo<SocketAddr>`，否则使用共用额度。跨站 Origin、未知字段、无效字段类型均会拒绝。用户名和上游 ID 中的 `::` 保留用于选源。

普通用户使用自己的本地 Yggdrasil access token 调用 `GET /api/v1/account` 查看自身资料和角色，或调用 `POST /api/v1/account/password`，正文为 `{"currentPassword":"旧密码","password":"新密码"}`。密码修改必须验证旧密码，成功后撤销该用户全部旧令牌和 join 记录，需要重新登录。这两个接口只接受 Bearer 本地用户令牌；管理员令牌、上游令牌不能代替它。`auth/login`、`auth/me` 和 `auth/logout` 是管理员会话接口；普通用户登录、刷新、退出分别使用标准 `/authserver/authenticate`、`refresh`、`invalidate`/`signout`。当前提供 HTTP API，没有附带注册管理网页、邮箱验证或忘记密码邮件流程。

```http
POST /api/v1/users
Authorization: Bearer <token>
Content-Type: application/json

{"username":"Alice","password":"a-long-local-password"}
```

```http
POST /api/v1/profiles
Authorization: Bearer <token>
Content-Type: application/json

{"user_id":"<created-user-id>","name":"Alice","uuid_scheme":{"kind":"offline_player"}}
```

支持 `random_v4`、`offline_player`、`fixed`、`mojang_mirror`、`name_v3`、`name_v5`。后四种的参数见 OpenAPI。角色改名不改变 UUID；已有绑定令牌暂时失效，可刷新恢复。

| 资源 | 接口 |
| --- | --- |
| 管理会话 | `auth/login`、`auth/logout`、`auth/me` |
| 注册、用户/管理员 | `auth/register`；`users`、`admins` 的创建、查询、更新、删除及 `/{id}/password` |
| 普通用户自助 | `GET account`、`POST account/password`，需要本地用户 Bearer 令牌 |
| 撤销用户令牌 | `POST users/{id}/revoke-sessions` |
| 角色 | `profiles` 的创建、查询、改名和删除 |
| 皮肤/披风 | `PUT/DELETE profiles/{id}/skin` 或 `/cape`，PUT 为 `image/png` 原始请求体，`?model=slim` 可选 |
| 名称历史 | `GET profiles/{id}/history` |
| 官方镜像 | `POST profiles/{id}/mirror/minecraft`，正文 `{"access_token":"一次性官方令牌"}` |
| 令牌/管理会话 | `GET tokens`、`GET sessions` 和 `DELETE .../{management_id}` |
| 上游 | `upstreams` CRUD、`/{id}/probe`、`/{id}/health` |
| 联邦 | `federation/rules` CRUD、`POST federation/rules/validate` |
| 缓存 | `cache/routes` CRUD、`cache/stats`、`POST cache/purge` |
| 状态 | `status`、`capabilities` |

官方镜像要求目标角色已经使用相同的官方 UUID（可先用 fixed 创建），从 Minecraft Services 验证持有者，下载当前启用的 skin/cape 并净化 PNG 后原子更新。官方令牌不会存入数据库、文件或日志。镜像之后断网仍可使用上次保存的角色和纹理。未持有官方令牌时不要将此接口视作公开用户名查询。

协议纹理 PUT 与管理 API 不同：`/api/user/profile/{uuid}/{skin|cape}` 使用 Yggdrasil Bearer accessToken 和 multipart `file`（image/png）、`model`。PNG 限制为 2 MiB，标准皮肤为 64×32 或 64×64；披风支持 64×32、22×17（自动补边）。

## 多上游和缓存

上游与策略示例见 [examples/resources.json](examples/resources.json)。逐个 POST 到文件标记的管理资源路径即可配置；启动不会自动导入示例。

同一服务同时提供自己的本地身份源和多个代理上游。客户端和游戏服务器统一使用网关的 `public_url`。配置示例包括 primary、secondary 两个 Yggdrasil 源，以及独立 HTTP 资源缓存源；上游请求可直连，或由网关通过自己的 HTTP/SOCKS 代理访问。

认证选源遵循以下规则：

- 已存在的本地用户名（包括停用账号）由本地源验证，即使默认登录规则指向上游；错误密码不会尝试其他源。
- `local::Alice` 明确登录本地；`primary::Alice`、`secondary::Alice` 明确登录相应上游。网关先去掉前缀再转发，因此各上游仍收到原用户名。同名账号可通过前缀区分。
- 没有前缀、也不存在本地账号时，使用 authenticate/signout 联邦规则；未配置规则时使用本地。
- 显式选源不会因源离线或凭据错误切换身份源。未知、停用或不支持认证的源直接报错。后续 refresh/validate/join/invalidate 不需要再次选源，使用已持久化的令牌归属。

角色批量查询可将 local 与多个远程源合并；UUID、名字、历史查询可按本地 → primary → secondary 的顺序回退。示例为批量查询开启同名冲突拒绝；登录账号名的前缀不改变 Minecraft 角色名或 UUID。同名角色的联机环境应使用唯一角色名或配置查询优先级，避免客户端姓名解析歧义。

优先级数字越小越优先；规则先按 `match_priority`、再按 id 排序。没有规则时使用本地身份源。`source` 可为 `local`、具体上游 id 或 `*`；具体 `priority` 覆盖上游优先级，通配符未设置 priority 时保留各上游优先级。支持 priority_failover、pinned 和仅限批量资料查询的 merge_priority。重复名称按不区分大小写比较；默认高优先级获胜，可配置冲突报错。凭据错误、权限错误和限流为终止错误；连接失败/5xx 可向后选择其他源。状态变更不自动重试；安全请求最多重试三次。

令牌哈希和 `serverId + username` 哈希持久记录来源，刷新、联机和 hasJoined 不会因优先级变化转到另一个上游。未知令牌不会广播。一个上游失效不应通过另一个独立认证站“验证”其令牌。

每个上游有独立 Reqwest 连接池、代理、超时和健康阈值；不读取系统代理。HTTP(S)、socks5（本地 DNS）和 socks5h（代理 DNS）由 transport.kind 配置，URL scheme 必须对应。代理密码和额外请求头不会通过 API 读接口明文返回。上游 profile 纹理会重新签名并镜像到本地；默认只允许上游自己的 origin，其他 HTTPS 纹理域名需在 `texture_domains` 中明确列出（例如 `textures.minecraft.net`）。不会自动跟随重定向或把认证头发往 CDN。

缓存只接受 `/cache/...` 下配置好的映射，不接受客户端目标 URL。磁盘内容按 SHA-256 寻址，元数据持久化；支持 TTL/负缓存 TTL、ETag、Last-Modified、304 和 stale-if-error。不以旧内容掩盖 401/403；默认绕过 Authorization/Cookie、Range 和客户端条件请求。private/no-store/Set-Cookie 及不在 vary_headers 中的 Vary 不存储。启用 authenticated caching 时仍把凭据哈希纳入缓存键隔离用户。HEAD 未命中会获取完整 GET，避免污染后续 GET。超过单对象缓存上限的响应不缓存；传输有 256 MiB 保护上限。当前缓存元数据上限为 10,000 项/2 GiB，按插入时间淘汰；无引用磁盘内容有一小时回收宽限期。清理元数据不保证立即释放磁盘空间。

启动器发送到本地 cache gateway 的 `User-Agent` 会随请求头原样转发到配置的上游。Rust cache 只追加条件重验证头，不覆盖或删除 `User-Agent`；上游适配器仅清理 Host、Connection 等 hop-by-hop 头。HTTP、SOCKS5 和 SOCKS5h 代理只改变传输路径，因此不会改写启动器 UA；`UserAgentOverride` 设置的值同样适用。缓存命中时不会访问上游，发生回源或 stale-if-error 时才会透传到上游。

## 嵌入 Rust

从其他 Cargo 工程添加路径依赖：

```toml
[dependencies]
yggdrasil-server = { path = "../yggdrasil/crates/yggdrasil-server" }
```

`yggdrasil_server::build(&Config).await` 返回 Router、typed `Service`、管理 API 状态和上游客户端。可直接调用 Service 的 create_user/create_profile/authenticate/refresh/join 等方法，或把 Router 挂到宿主 HTTP 服务。宿主负责 Tokio runtime、listener、周期性 `Service::cleanup`/`Clients::health_tick` 及退出信号。只需领域模型时单独依赖 `yggdrasil-core`，它不依赖 Axum、SeaORM 或 Reqwest。密码计算、PNG 解码和初始密钥生成移至 blocking pool。

## 存储与验证边界

SQLite 默认，`database_url`/`YGGDRASIL_DATABASE_URL` 也支持 PostgreSQL 和 MySQL。SeaORM repository 使用带版本号的 aggregate 行，CAS 保证并发刷新、改名、撤销和配置更新原子性；迁移版本保存在 `yggdrasil_migrations`，拒绝更高版本数据库。适合启动器和小型独立服务；高写入吞吐部署应在 Repository 接口后替换为分表实现，当前所有修改竞争同一个 revision。MySQL payload 使用 LONGTEXT。

备份时停止服务并一起备份数据库、signing-key.pem 和 textures/；缓存可重建。签名私钥必须保持不变。Unix 新建密钥权限为 0600，Windows 继承数据目录 ACL。

具体测试和未验证项见 [IMPLEMENTATION_MATRIX.md](IMPLEMENTATION_MATRIX.md)。当前不声明安全聊天/profile-key 证书支持：metadata 中 feature.enable_profile_key=false，证书接口返回不支持。真实 Minecraft/authlib-injector 版本联调和真实官方令牌镜像尚需相应游戏环境/凭据。PostgreSQL/MySQL 驱动编译通过，运行测试需要单独数据库：

```sh
YGGDRASIL_TEST_DATABASE_URL='postgres://...' cargo test --locked \
  -p yggdrasil-server external_database -- --ignored
```

## 参考与许可

实现参考用户提供的 `yggdrasil-rs-handoff.zip` 设计资料；附件中的模型提示词未作为工程指令执行。API 使用实际代码定义，不包含旧私有配置兼容层。代码按 GPL-3.0-only 分发，见 [LICENSE](LICENSE)。
