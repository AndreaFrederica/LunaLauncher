# Qt / Neo UI 功能对照

> PCL-CE 视觉复刻的逐项差异、真实窗口截图和当前状态见
> [`docs/pcl-neoui-theme-diff.md`](../docs/pcl-neoui-theme-diff.md)。

这份清单以 `launcher/ui/pages/global`、`MinecraftSettingsWidget`、`JavaSettingsWidget` 和实例设置对象为基准。每一项都必须通过 Launcher API 读写；Qt 专属选项单独放在 Qt 兼容页。

| Qt 功能 | API 对应 | Neo UI 入口 | 状态 |
| --- | --- | --- | --- |
| 实例 Java 路径、兼容性、自动选择 | `settings.*`、`java.refresh`、`java.select`、`java.diagnose` | 客户端设置 → Java 与内存 | 已接入 |
| 实例最小/最大内存、PermGen、内存警告 | `settings.*` | 客户端设置 → Java 与内存 | 已接入 |
| JVM 参数与覆盖开关 | `settings.*` | 客户端设置 → Java 与内存 | 已接入 |
| 窗口大小、最大化、退出行为 | `settings.*` | 客户端设置 → 启动与快速加入 | 已接入 |
| 启动前、包装、退出命令 | `settings.*` | 客户端设置 → 命令与环境 | 已接入 |
| 实例环境变量 | `settings.*` | 客户端设置 → 命令与环境 | 已接入 |
| 控制台显示、自动关闭、错误显示、行数 | `settings.*` | 客户端设置 → 控制台 | 已接入 |
| GLFW/OpenAL 原生库与性能选项 | `settings.*` | 客户端设置 → 兼容与性能 | 已接入 |
| 资源校验和缓存 | `settings.*` | 客户端设置 → 兼容与性能 | 已接入 |
| 快速加入服务器/世界与实例账户 | `settings.*` | 客户端设置 → 启动与快速加入 | 已接入 |
| 模组加载器筛选、全局数据包 | `settings.*` | 客户端设置 → 下载与资源 | 已接入 |
| 实例所有未专门布局的字段 | `settings.list/get/set/reset` | 客户端设置 → 完整 API 设置 | 已接入 |
| 实例图标、目录、组件、模组和资源 | `instance.*`、`resource.*`、`component.*` | 实例工作区 | 已接入 |
| 启动器目录和书签 | `settings.*` | 启动器设置 → 常规与路径 | 已接入 |
| 启动器 Java 管理 | `java.*`、`settings.*` | 启动器设置 → Java 管理 | 已接入 |
| Qt / Aria2 / PCL 下载核心 | `settings.*`、`aria2.*` | 启动器设置 → 下载核心 | 已接入 |
| 下载并发、分段、重试、限速 | `settings.*` | 启动器设置 → 下载核心 | 已接入 |
| Aria2 队列、取消、清理 | `aria2.downloads/cancel/clear-finished` | 启动器设置 → 下载核心 | 已接入 |
| Mojang、Modrinth、CurseForge 等镜像 | `settings.*` | 启动器设置 → 镜像与资源 | 已接入 |
| Yggdrasil 账户和预设 | `account.*`、`account.preset.*`、`authentication.yggdrasil.metadata` | 账户管理 | 已接入 |
| authlib-injector / nide8auth | `authentication.helper.*` | 启动器设置 → 认证与工具 | 已接入 |
| JProfiler、JVisualVM、MCEdit、CurseForge 工具 | `external-tool.check`、`settings.*` | 启动器设置 → 认证与工具 | 已接入 |
| 代理、更新、主题、语言、维护 | `network.proxy.*`、`launcher.update.*`、`appearance.*`、`language.*` | 启动器设置 → 网络与界面 / Qt 兼容设置 | 已接入 |
| Qt 工具栏、菜单、旧界面专属字段 | `settings.*`（`surface=qt`） | Qt 兼容设置 | 已隔离 |

## 生命周期约定

- 每次设置保存都调用 `settings.set`，重置调用 `settings.reset`，完成后刷新父页面。
- Java、Aria2、认证辅助工具和外部工具操作都使用已有 API 任务生命周期，不在 UI 中伪造状态。
- 运行实例由实例列表状态、侧栏运行标记、Dashboard 状态、启动队列和控制台订阅共同管理；顶部重复标签已移除。
- 下载任务详情通过任务快照和 Aria2 队列提供，前端可以显示分步进度、状态和取消操作。
