# 实现与验证记录

参考交接包的能力分组。`tested` 表示有本地自动测试通过；`implemented-unverified` 表示代码存在但对应外部环境未验证；`partial` 和 `missing` 不宣称完成。

2026-09-28 至 09-29 本地验证：25 项 Rust 测试通过（`--test-threads=2`，避免多个调试 RSA 初始化竞争导致子进程握手超时），1 项外部数据库测试按条件跳过；Cargo fmt、Clippy `-D warnings` 通过。Qt `AccountSnapshot_test` 两项用例通过，覆盖无凭据快照、UUID/纹理隔离和本地认证会话。真实 CLI + Rust 子进程冒烟通过：重启后源站 503 时读取持久元数据缓存；正版认证连接失败出现本地认证选项，缺少 authlib-injector 时明确失败。Windows/MSVC 启动器全量构建、最终增量编译及链接、Meson 可选服务目标/安装位置检查通过。独立 release 构建通过，实际 release 进程的 HTTP 冒烟覆盖握手、metadata、创建用户/角色、认证、OpenAPI 和正常退出；新增独立 release 进程注册、查看自身资料、改密及选源登录冒烟通过。已验证可执行文件位于 `target/release/yggdrasil-server.exe`，并复制到已编译启动器旁。

| 能力 | 状态 | 证据 / 边界 |
| --- | --- | --- |
| 独立 workspace、typed 嵌入 API、daemon | tested | workspace build；子进程握手与 EOF 退出集成测试 |
| SQLite 与版本迁移 | tested | 文件数据库重启、原子 CAS、过期清理 |
| PostgreSQL / MySQL | implemented-unverified | SeaORM 三驱动编译；外部 DB 集成测试按环境变量显式执行 |
| 多用户/多角色、每角色 UUID 方案 | tested | Java 离线 UUID 向量、多角色归属、改名不变 UUID |
| 自助注册、本地用户资料与改密 | tested | 注册开关、并发唯一性、角色冲突原子回滚、字段拒绝、跨站拒绝、IP 限流；用户/管理员权限隔离；改密撤销旧令牌和 join；启动器模式关闭公开注册 |
| Authenticate / refresh / validate / invalidate / signout | tested | HTTP 状态、失败刷新不破坏旧令牌、并发刷新单一胜者、signout 撤销 |
| Join / hasJoined | tested | token binding、名称、IP、过期、RSA 签名 |
| Profile / batch / username / history | tested | HTTP 查询、多角色改名和历史、批量合并 |
| 皮肤披风存储、净化、尺寸和字节限制 | tested | PNG 净化、非法尺寸拒绝、22×17 披风补边、哈希寻址 |
| 持久签名私钥、properties 签名 | tested | 重启公钥不变、SHA1withRSA 验证 |
| 协议 multipart 纹理端点 | tested | 实际 multipart HTTP 请求上传 skin/model；错误令牌拒绝 |
| legacy skin、privileges/attributes/blocklist | implemented-unverified | 路由与适配实现；真实游戏兼容性另需联调 |
| 官方 UUID/名称/纹理镜像 | implemented-unverified | 固定 Minecraft Services 调用、官方 UUID 校验、纹理域名限制、无官方令牌持久化；未提供真实令牌 |
| 多上游、优先级、规则、能力过滤 | tested | pin / merge / priority conflict 集成测试 |
| 本地身份源与多个服务端代理源共存 | tested | 实际启动两台上游，通过网关注册本地账号；本地/显式选源登录、refresh、join/hasJoined 重签名、signout、三源资料合并与查询回退；上游停机后本地仍可登录 |
| Direct / HTTP proxy / SOCKS5 / SOCKS5h | tested | 本地假代理实际连接；验证 SOCKS 地址类型、代理 DNS，以及启动器 User-Agent 经缓存/代理原样到达上游 |
| HTTPS proxy | implemented-unverified | Reqwest TLS proxy 实现；未配置真实 HTTPS 代理 |
| 安全重试、健康阈值 | tested | 写操作不重试，安全操作重试，连续失败阈值 |
| Token / join affinity 持久化 | tested | 重建服务后 refresh / join / hasJoined 仍到原上游 |
| 联邦属性重新签名、本地纹理镜像 | partial | 重新签名测试通过；受限 CDN 纹理下载路径已实现，真实上游联调未完成 |
| 固定反向路由、磁盘 cache、TTL/负 TTL | tested | 缓存读写与固定路径/遍历拒绝测试 |
| ETag / 304 / stale-if-error | tested | 重验证及上游 5xx 过期回退 |
| 启动器元数据缓存配置 | tested | 配置重放保留磁盘缓存；重启后重新验证、503/连接失败回退、401 不回退、切换源隔离 |
| 正版账号私有快照与本地会话 | tested | 原 UUID、本地令牌、join/hasJoined、签名 skin/cape；无凭据导入、角色冲突拒绝、删除撤销；Qt 快照/会话测试、CLI 弱网选项与认证取消回归通过；真实正版账号未联调 |
| Last-Modified | implemented-unverified | 同一重验证路径写入 If-Modified-Since；未单列真实源测试 |
| Cache-Control / Set-Cookie / auth 绕过 / Vary | tested | private、Set-Cookie、Authorization 绕过；304 更新为 private 后移除缓存 |
| HEAD / purge / stats | tested | HEAD 不污染 GET，路由 purge、计数 |
| 单对象限制 / 2 GiB、10k 项淘汰 / blob GC | implemented-unverified | 有大小保护与宽限回收；未做磁盘压力测试 |
| Admin login / cookie / Bearer / CSRF / 撤销 | tested | 未授权拒绝、同源检查、哈希存储、退出撤销、最后管理员保护 |
| 用户、角色、上游、规则、缓存路由 CRUD | tested | 管理资源创建、修改、引用完整性、级联删除与秘密脱敏集成测试 |
| OpenAPI / cursor 分页 / secret redaction | partial | 从 Rust DTO 生成字段；分页和脱敏实现；未生成外部前端客户端 |
| 后台任务退出 | tested | daemon 子进程通过 stdin EOF 清理退出并撤销管理令牌 |
| 可选静态目录 | implemented-unverified | 使用 tower-http ServeDir；未运行真实 SPA |
| Qt 启动器接入、构建和安装规则 | partial | Windows/MSVC 编译链接、快照/会话 Qt 测试、实际 CLI 缓存与弱网本地认证选项冒烟通过；账号入口、进程生命周期、持久 managedLocal 标记、多角色绑定已实现；真实 GUI/游戏联调未完成 |
| Secure chat / player certificates | missing | 明确禁用 feature.enable_profile_key；没有伪造/占位证书 |
| 实际 Minecraft/authlib-injector 客户端与服务端联调 | missing | 本地协议和模拟上游测试不等价于实际游戏版本验证 |

本版持久化以单个版本化 aggregate 做原子提交；这不是面向大型公共皮肤站的分表、高并发存储设计。管理 API 无前端依赖，前端 UI 可后续单独开发。
