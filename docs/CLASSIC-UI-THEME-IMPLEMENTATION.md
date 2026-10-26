# Classic UI 主题系统实现

实现日期：2026-10-26。正式入口是 **Classic UI → 设置 → 外观 → 主题中心…**。窗口完全由 C++ / Qt Widgets 构成；`docs/design/classic-theme-center.html` 仍是设计原型。

## 使用

- 在线主题：默认进入 Kvantum 控件分类，可下载安装真实 SVG 控件皮肤；另提供 KDE 配色、图标、Plasma 5 / 6 成套组合。显示当前页及目录总数。保留原有 GitHub Release 来源，包括 PrismLauncher/Themes。
- 已安装：本地文件或目录导入、来源更新、卸载、打开目录。旧 `themes`、`iconthemes`、`catpacks` 和内置组件出现在同一目录中。
- 外观组合：选择系统或独立模式，组合控件、固定/随系统明暗变化的配色、图标、背景和窗口装饰。**安装不会自动应用**。
- 下载任务：显示解析下载地址、下载、校验/解压/扫描、发布的阶段；可以取消或重试。隐藏主题中心不取消安装。

“已安装 → 使用所选主题包…”会读取包内组件及 KDE 全局主题的 `contents/defaults`，生成控件、图标、配色的组合草稿。缺失组件会明确列出；多个变体需选择后再预览和应用。只有 Plasma 桌面内容的包不会被标记为完整的应用主题。

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
| Luna v2 | 控件 QSS 或 Kvantum、独立配色、图标、Prism 背景、声明式 Luna 标题栏 |
| Kvantum `.kvconfig` + 同名 `.svg` | 内建固定版本的原生 QStyle 引擎，改变控件绘制、尺寸与各状态效果；使用主题自己的配色，无需系统安装 Kvantum |
| KDE 全局主题包 | 识别控件、配色、图标引用和声明的 KNS 依赖；检查组件是否齐全；导入包内兼容组件，不执行安装脚本 |
| Breeze、QtCurve 等其他控件插件 | 仅在本机已有对应 QStyle 时可使用；当前发布包没有内建这些引擎 |
| Aurorae、KDecoration、Plasma 桌面效果 | 可浏览，缺少对应渲染环境；不能宣称完整复现这些效果 |

Fusion 是可组合配色的通用基线。显式写死颜色的旧 QSS/Prism 皮肤不接受独立配色；Luna QSS 使用 `palette(...)` 并声明 `paletteBinding: semantic` 时允许叠加配色。KDE 颜色效果采用兼容映射，并非完整嵌入 KColorScheme。

Kvantum 引擎来源和改动见 [`libraries/kvantum/README.luna.md`](../libraries/kvantum/README.luna.md)。引擎只从显式传入的包内路径读取配置，不读取全局 Kvantum 选择器，不设置 `XDG_CONFIG_HOME`，不修改用户桌面设置。Qt 6.6 及以上构建包含该引擎；保留 Qt 6.4 / 6.5 构建时不编译该引擎，组件会报告缺少引擎。

支持 `X-KPackage-Dependencies` 中的 `kns://icons.knsrc/...`、`kns://colorschemes.knsrc/...`、`kns://kvantum.knsrc/...` 精确商店 ID。只有一个下载附件时自动安装；多个附件会报告待选择，已安装树中提供“选择并下载此依赖…”入口。API 可通过 `dependencyFiles: {"内容ID":"附件ID"}` 指定变体。上游未声明来源或使用其他插件/脚本的依赖仍需手动补齐，不会按相似名称随意下载安装。

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
| `appearance.package.configuration` | `id` 为包或具体组件；返回组合草稿、`missing`、`choices`、`complete` 和依赖 |
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

Kvantum 控件组件声明为 `"widgets": {"engine":"Kvantum", "config":"widgets/MyTheme.kvconfig"}`，同目录需要 `MyTheme.svg`。也可直接导入上游 Kvantum 文件夹或归档，无需重新打包为 Luna 格式。SVG 在安装时检查语法、大小及外部资源引用。

## 验证记录

- Windows / Qt 6.10.1 / MSVC Release：GUI、CLI、主题单元测试和原生窗口测试编译通过。
- 包测试覆盖配色、QSS 资源、Luna 组件资源根目录、OCS 业务错误/分页、GitHub 来源、重复安装与取消。
- 原生窗口测试覆盖常驻导航、保留搜索、应用与撤销、真实文件选择状态、标题栏、对话框、最大化与还原；检查窄窗口和不同缩放下的截图。
- `tests/theme_system_smoke.py` 覆盖迁移、安装不应用、重启、热切换、revision 冲突、卸载保护、图标继承、分批注册组件、恶意归档、包内链接和初始化恢复。
- `--online --preview` 实测 KDE 目录、多附件、签名解析、校验/安装及真实 Prism GitHub 主题包，验证隔离预览保持账户和保存配置不变。
- 原有 API 的外观/语言及主题/资源回归通过。网络测试受上游可用性影响；本地和原生测试使用隔离数据目录。

首次实现基线：主题包测试 **9/9**，原生窗口测试 **5/5**（100%、150%、200% 各一轮），既有 API 回归 **2/2**。该基线只覆盖当时的配色/QSS/图标，不代表 Kvantum 或任意 KDE 全局主题已经兼容。

本次扩展增加了原生按钮 SVG 状态绘制、KDE 嵌套默认配置、声明依赖的下载/校验/安装、多附件选择及缺失组件测试。`theme_widgets_smoke.py` 覆盖控件与图标整包组合、重启恢复、缺失引擎、纯桌面包不误报完整，以及两个未经修改的上游主题 KvArcDark / KvMojaveLight 的 Windows 原生预览。KDE Store 本次联网验证遇到请求超时，不能把本地测试记为本次商店联网通过。

最终构建的 `ThemePackage_test` 和 `ThemeCenter_test` 均通过。GUI / CLI 已部署到 `install-neo-ui` 并校对 SHA-256；直接使用安装目录的 CLI，通过控件整包测试、原有主题事务/隔离预览回归及两项既有外观 API 回归。

```powershell
pixi run meson compile -C build-meson-neo-ui lunalauncher lunalauncher-cli ThemePackage_test ThemeCenter_test -j 8
pixi run meson test -C build-meson-neo-ui --no-rebuild ThemePackage_test ThemeCenter_test
pixi run python tests/theme_widgets_smoke.py --exe install-neo-ui/lunalauncher-cli.exe
pixi run python tests/theme_system_smoke.py --exe build-meson-neo-ui/launcher/lunalauncher-cli.exe --online --preview
```

从构建目录运行 Windows 二进制时，需要 Qt bin 和已部署依赖在 PATH，并设置正确的 `QT_PLUGIN_PATH`；安装目录已包含运行依赖。
