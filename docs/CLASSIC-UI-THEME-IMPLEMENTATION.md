# Classic UI 主题系统实现

实现日期：2026-10-26。正式入口是 **Classic UI → 设置 → 外观 → 主题中心…**。窗口完全由 C++ / Qt Widgets 构成；`docs/design/classic-theme-center.html` 仍是设计原型。

## 使用

- 在线主题：KDE Store 配色、图标与组合包；保留原有 GitHub Release 来源，包括 PrismLauncher/Themes。来源设置沿用 `ThemeDownloadSources` / `ThemeDownloadSource`。
- 已安装：本地文件或目录导入、来源更新、卸载、打开目录。旧 `themes`、`iconthemes`、`catpacks` 和内置组件出现在同一目录中。
- 外观组合：选择系统或独立模式，组合控件、固定/随系统明暗变化的配色、图标、背景和窗口装饰。**安装不会自动应用**。
- 下载任务：显示解析下载地址、下载、校验/解压/扫描、发布的阶段；可以取消或重试。隐藏主题中心不取消安装。

配色、图标与背景可以即时生效；控件风格、系统/独立模式和窗口装饰在下次启动生效。待重启组合可撤销；界面同时显示当前生效值和下次启动值。在线分页、搜索、选中项和草稿保留在常驻页面中。搜索明确筛选已载入的项目。

独立预览由同一个可执行文件以 `--theme-preview` 启动普通 QApplication，不创建启动器 Application、不加载账户、不取得数据目录锁。预览包含真实 Qt 控件、所选图标、背景、日志配色和装饰。预览失败会反馈到页面和外观事件流。

可以用以下命令测试 Classic UI 的独立数据目录：

```powershell
& 'D:\Projcets\PrismLauncher\install-neo-ui\lunalauncher.exe' --dir 'D:\Projcets\PrismLauncher\theme-test-profile'
```

若某个自定义外观无法正常显示，加 `--safe-theme` 临时使用系统外观。保存的组合会保留，下次普通启动仍可重新选择。

## 实际兼容范围

| 格式 / 组件 | 行为 |
| --- | --- |
| Prism `theme.json`、QSS/CSS、catpack | 继续支持原有加载器，下载后注册到统一目录 |
| KDE `.colors`、KPackage 的 `colors` | 映射 QPalette 的活动、非活动、禁用、选中、链接等角色；保留语义颜色元数据 |
| freedesktop `index.theme` | Qt 图标主题、继承与内置图标回退；包内文件/目录链接解压成普通文件 |
| Luna v2 | 控件 QSS、独立配色、图标、Prism 背景、声明式 Luna 标题栏 |
| KDE 全局主题包 | 只导入实际包含的兼容组件；不会拉取或执行其脚本、桌面服务与外部依赖 |
| Kvantum、Aurorae、KDecoration、Plasma 效果 | 可浏览，显示缺失引擎说明；不转换成 Windows 主题 |

Fusion 是可组合配色的通用基线。显式写死颜色的旧 QSS/Prism 皮肤不接受独立配色；Luna QSS 使用 `palette(...)` 并声明 `paletteBinding: semantic` 时允许叠加配色。KDE 颜色效果采用兼容映射，并非完整嵌入 KColorScheme。

Windows 提供应用内 Luna 标题栏及移动、缩放、最大化、系统菜单和 Snap 命中处理。Linux 使用 Qt 的系统移动/缩放接口；KDE 可向支持的平台插件传递应用配色。macOS 保留原生窗口按钮和全屏行为，退出独立配色时恢复原始 NSWindow 属性；自定义装饰能力为 false。**Linux/macOS 的原生窗口行为尚未在本机运行验证。** 不改动操作系统全局主题。

## 状态与安装

`classic-appearance.json` 使用 QSaveFile 原子保存，包含 `requested`、`effective`、`pending`、`lastKnownGood`、`revision` 和初始化标记。旧设置首次打开时迁入；之后旧下拉框和 API 都通过同一服务提交。提交必须提供 `baseRevision`，过期草稿不会覆盖新状态。初始化中断或组件缺失时恢复可用外观。

包安装在 `theme-packages/<sha256>`。下载后校验大小及来源提供的 SHA-256/MD5；在工作线程完成限额解压与扫描，再以安装锁和目录重命名发布。相同内容重复导入不会复制资源，可追加注册其他组件。新版本保留独立内容目录；使用中、待重启、恢复点及图标继承引用受到卸载保护。

目录响应缓存位于 `cache/theme-store`，一小时内复用，离线失败可返回带时间戳的旧数据；KDE 签名下载地址每次安装重新解析。目录缓存上限 100 个文件 / 32 MiB；截图缓存七天、128 个文件 / 32 MiB。清理缓存不删除已安装主题。

## API

| 操作 | 主要参数 / 结果 |
| --- | --- |
| `appearance.capabilities` | 格式、安装的 QStyle、平台装饰能力 |
| `appearance.state` | 状态、revision、组件、安装包 |
| `appearance.apply` | `configuration`, `baseRevision` |
| `appearance.undo` | `baseRevision` |
| `appearance.preview` | `configuration` |
| `appearance.query` / `appearance.files` | `provider`, `source`, `category`, `sort`, `page`, `contentId`, `refresh` |
| `appearance.import` | 本地 `path`、HTTPS `url` 或 provider 身份三选一；可指定 `components` |
| `appearance.package.remove` | `id`, `confirm` |
| `appearance.tasks` | 带 `operationId` 的保留任务快照 |
| `appearance.subscribe` | 初始状态和目录，以及后续状态、目录、任务、预览错误；使用既有 `event.poll` / `event.unsubscribe` |
| `appearance.cache.clear` | 清理主题商店缓存 |

旧 `catalog`、`refresh`、`select`、`install(kind=...)`、`remove`、`sources`、`release` 兼容保留。无 `kind` 的 install 使用新安装机制。

## Luna v2 示例

可直接在“已安装 → 导入文件夹”选择 [`examples/themes/luna-sample`](../examples/themes/luna-sample)。安装后在组合页分别选择其控件、配色和装饰。

```json
{
  "schemaVersion": 2,
  "name": "Luna Sample",
  "components": {
    "widgets": {"engine": "Fusion", "qss": "skin.qss", "paletteBinding": "semantic"},
    "palettes": {"dark": {"format": "kde-colors", "file": "dark.colors"}},
    "decoration": {"renderer": "luna-widgets", "config": "decoration.json"}
  }
}
```

图标组件可声明 `"icons": {"directory": "icons/my-theme"}`；背景组件可声明 `"background": {"manifest": "background/catpack.json"}`。路径必须在包内；QSS 只引用包内资源或 Qt 资源，不允许外部导入。装饰只接收高度、活动/非活动前景背景色和标题加粗配置。

## 验证记录

- Windows / Qt 6.10.1 / MSVC Release：GUI、CLI、主题单元测试和原生窗口测试编译通过。
- 包测试覆盖配色、QSS 资源、Luna 组件资源根目录、OCS 业务错误/分页、GitHub 来源、重复安装与取消。
- 原生窗口测试覆盖常驻导航、保留搜索、应用与撤销、真实文件选择状态、标题栏、对话框、最大化与还原；检查窄窗口和不同缩放下的截图。
- `tests/theme_system_smoke.py` 覆盖迁移、安装不应用、重启、热切换、revision 冲突、卸载保护、图标继承、分批注册组件、恶意归档、包内链接和初始化恢复。
- `--online --preview` 实测 KDE 目录、多附件、签名解析、校验/安装及真实 Prism GitHub 主题包，验证隔离预览保持账户和保存配置不变。
- 原有 API 的外观/语言及主题/资源回归通过。网络测试受上游可用性影响；本地和原生测试使用隔离数据目录。

最终结果：主题包测试 **9/9**，原生窗口测试 **5/5**（100%、150%、200% 各一轮），既有 API 回归 **2/2**。部署后的 `install-neo-ui/lunalauncher-cli.exe` 另跑完整联网及隔离预览测试通过；GUI/CLI 安装产物 SHA-256 与构建目录逐一一致，Qt 运行依赖已部署。

```powershell
pixi run meson compile -C build-meson-neo-ui lunalauncher lunalauncher-cli ThemePackage_test ThemeCenter_test -j 8
pixi run python tests/theme_system_smoke.py --exe build-meson-neo-ui/launcher/lunalauncher-cli.exe --online --preview
```

从构建目录运行 Windows 二进制时，需要 Qt bin 和已部署依赖在 PATH，并设置正确的 `QT_PLUGIN_PATH`；安装目录已包含运行依赖。
