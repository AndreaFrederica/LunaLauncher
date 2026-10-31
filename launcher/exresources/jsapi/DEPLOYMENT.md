# JavaScript API 部署指南

JavaScript Resource API 属于可更新的运行时数据，统一放在启动器资源目录中，
由 `LauncherResources` 解析用户目录、安装目录和开发目录。它不再编译进 Qt
资源系统，因此可以随版本更新或由用户扩展。

## 目录布局

安装包将 `launcher/exresources/jsapi` 安装到启动器资源根目录下：

```text
Windows: <启动器目录>/resource/jsapi/
macOS:   LunaLauncher.app/Contents/Resources/jsapi/
Unix:    <prefix>/share/<launcher>/resources/jsapi/
```

用户覆盖目录位于 `Application::dataRoot()/jsapi`。开发构建还会查找源码树
中的 `launcher/exresources/jsapi`。

## 加载顺序

`JSAPIManager::getDefaultAPIDirectories()` 使用统一资源组件返回以下顺序：

1. 用户数据目录，允许覆盖或添加 API；
2. 安装目录，包含官方随包发布的 API；
3. 开发源码目录，仅用于本地开发。

每个目录中的 `.js` 文件都由管理器读取并根据 `metadata` 注册。API 文件名
相同的时候，排在前面的目录优先。

## 添加或更新 API

官方 API 放在 `launcher/exresources/jsapi/`，然后由构建系统自动安装：

```meson
install_subdir('exresources/jsapi', install_dir: exresource_dest_dir)
```

```cmake
install(DIRECTORY exresources/jsapi
        DESTINATION ${EXRESOURCE_DEST_DIR}
        USE_SOURCE_PERMISSIONS
        COMPONENT Runtime)
```

用户 API 直接复制到 `Application::dataRoot()/jsapi`。应用重新启动后会自动
扫描；需要立即生效时可调用：

```cpp
JSAPIManager::instance().reloadAll();
```

## C++ 加载文件

业务代码不要拼接平台路径，也不要直接依赖 Qt 资源路径。使用统一解析器：

```cpp
#include "LauncherResources.h"

const auto path = LauncherResources::locateFile(
    QStringLiteral("jsapi/modrinth_plugin_api.js"));
auto api = JSResourceAPI::fromFile(path, "ModrinthPluginAPI");
```

## 用户目录示例

Windows：

```powershell
$apiDir = Join-Path $env:APPDATA 'LunaLauncher/jsapi'
New-Item -ItemType Directory -Force -Path $apiDir
Copy-Item '.\my_custom_api.js' $apiDir
```

Linux/macOS：

```bash
mkdir -p "$HOME/.local/share/lunalauncher/jsapi"
cp my_custom_api.js "$HOME/.local/share/lunalauncher/jsapi/"
```

实际数据根目录以 `Application::dataRoot()` 为准，发行版或用户配置改变
目录时无需修改 API 加载代码。

## 故障排除

- 查看日志中的 `[JSAPIManager] API search paths`，确认目录顺序；
- 确认文件扩展名为 `.js` 且 `metadata` 字段完整；
- 检查安装包是否包含 `resource/jsapi` 或 `share/.../resources/jsapi`；
- 用户目录中的同名文件会覆盖安装目录文件。
