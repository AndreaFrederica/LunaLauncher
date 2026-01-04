# JavaScript API 部署指南

本文档说明如何部署和管理 JavaScript Resource API。

## 部署架构

LunaLauncher 使用 **混合部署模式**：

```
┌─────────────────────────────────────────────────────────┐
│               JavaScript API 加载顺序                    │
├─────────────────────────────────────────────────────────┤
│ 1. 内置 API (qrc:/jsapi/)                              │
│    ├─ 编译进可执行文件                                   │
│    ├─ 始终可用，无法修改                                 │
│    └─ 用于核心平台支持（Modrinth、Hangar）              │
├─────────────────────────────────────────────────────────┤
│ 2. 安装目录 API                                         │
│    ├─ Windows: <exe_dir>/resource/jsapi/               │
│    ├─ Linux: /usr/share/lunalauncher/resources/jsapi/  │
│    ├─ macOS: LunaLauncher.app/.../Resources/jsapi/     │
│    ├─ 随程序安装一起部署                                │
│    └─ 由包管理器或安装程序管理                          │
├─────────────────────────────────────────────────────────┤
│ 3. 用户数据目录 API (推荐用户自定义 API)                │
│    ├─ Windows: %APPDATA%/LunaLauncher/jsapi/           │
│    ├─ Linux: ~/.local/share/lunalauncher/jsapi/        │
│    ├─ macOS: ~/Library/Application Support/.../jsapi/  │
│    ├─ 用户可添加、修改、删除                            │
│    ├─ 支持热重载                                        │
│    └─ 用于第三方平台和测试                              │
├─────────────────────────────────────────────────────────┤
│ 4. 开发目录 (仅开发模式)                                │
│    ├─ <exe_dir>/jsapi/                                 │
│    ├─ <current_dir>/jsapi/                             │
│    └─ 用于开发和调试                                    │
└─────────────────────────────────────────────────────────┘
```

## 1. 内置 API（编译进程序）

### 配置位置
- 源码: `launcher/modplatform/jsapi/*.js`
- 资源文件: `launcher/modplatform/jsapi/jsapi.qrc`

### 添加新的内置 API
编辑 `jsapi.qrc`:
```xml
<RCC>
    <qresource prefix="/jsapi">
        <file>your_new_api.js</file>
    </qresource>
</RCC>
```

### 优点
- ✅ 始终可用，不依赖外部文件
- ✅ 无法被用户误删或修改
- ✅ 适合核心功能

### 缺点
- ❌ 修改需要重新编译
- ❌ 增加可执行文件大小
- ❌ 无法热更新

### 使用场景
- Modrinth、Hangar 等主流平台
- 稳定的核心 API

## 2. 安装目录部署

### Windows
```
LunaLauncher/
├── LunaLauncher.exe
└── resource/
    └── jsapi/
        ├── modrinth_plugin_api.js
        ├── hangar_plugin_api.js
        ├── resource-api.d.ts
        └── README.md
```

### Linux
```
/usr/share/lunalauncher/resources/jsapi/
├── modrinth_plugin_api.js
├── hangar_plugin_api.js
├── resource-api.d.ts
└── README.md
```

### macOS
```
LunaLauncher.app/
└── Contents/
    └── Resources/
        └── jsapi/
            ├── modrinth_plugin_api.js
            ├── hangar_plugin_api.js
            ├── resource-api.d.ts
            └── README.md
```

### CMake 安装规则
```cmake
install(
    FILES
        modplatform/jsapi/modrinth_plugin_api.js
        modplatform/jsapi/hangar_plugin_api.js
        modplatform/jsapi/resource-api.d.ts
        modplatform/jsapi/README.md
    DESTINATION ${EXRESOURCE_DEST_DIR}/jsapi
    COMPONENT Runtime
)
```

### 使用场景
- 官方支持的平台 API
- 随版本更新一起分发
- 由系统包管理器管理（Linux）

## 3. 用户数据目录（推荐用户自定义）

### 创建用户 API 目录

**Windows (PowerShell)**:
```powershell
$apiDir = "$env:APPDATA\LunaLauncher\jsapi"
New-Item -ItemType Directory -Force -Path $apiDir
Copy-Item "my_custom_api.js" "$apiDir\"
```

**Linux/macOS (Bash)**:
```bash
API_DIR="$HOME/.local/share/lunalauncher/jsapi"
mkdir -p "$API_DIR"
cp my_custom_api.js "$API_DIR/"
```

### 目录结构示例
```
~/.local/share/lunalauncher/jsapi/
├── custom_platform_api.js       # 自定义平台
├── curseforge_plugin_api.js     # 社区贡献
├── testing/
│   └── experimental_api.js      # 测试 API
└── disabled/
    └── old_api.js               # 禁用的 API
```

### 热重载
在应用内调用（如果实现了 UI）：
```cpp
JSAPIManager::instance().reloadAll();
```

### 使用场景
- 第三方平台支持（CurseForge、自建平台等）
- 测试和开发新 API
- 个人定制和实验性功能

## 4. 开发目录

### 用途
仅用于开发和调试，不应在生产环境使用。

### 路径
- `<exe_dir>/jsapi/` - 可执行文件同目录
- `<current_dir>/jsapi/` - 当前工作目录

### 使用场景
- IDE 中开发和调试
- 快速测试新 API

## API 加载优先级

当多个目录存在同名 API 时，按以下优先级加载：

1. **内置 API** (qrc:/jsapi/) - 最高优先级
2. **安装目录** API
3. **用户数据目录** API - 可覆盖安装目录
4. **开发目录** - 仅开发环境

## 社区分发

### 为用户提供安装脚本

**Windows (PowerShell)**:
```powershell
# install-api.ps1
param([string]$ApiUrl)

$apiDir = "$env:APPDATA\LunaLauncher\jsapi"
New-Item -ItemType Directory -Force -Path $apiDir

$fileName = [System.IO.Path]::GetFileName($ApiUrl)
Invoke-WebRequest -Uri $ApiUrl -OutFile "$apiDir\$fileName"

Write-Host "API installed successfully to $apiDir\$fileName"
Write-Host "Restart LunaLauncher to load the new API."
```

使用:
```powershell
.\install-api.ps1 -ApiUrl "https://example.com/custom_api.js"
```

**Linux/macOS (Bash)**:
```bash
#!/bin/bash
# install-api.sh

API_URL="$1"
API_DIR="$HOME/.local/share/lunalauncher/jsapi"

mkdir -p "$API_DIR"
cd "$API_DIR"

curl -O "$API_URL"

echo "API installed successfully to $API_DIR"
echo "Restart LunaLauncher to load the new API."
```

使用:
```bash
./install-api.sh "https://example.com/custom_api.js"
```

### API 仓库（未来）

考虑创建中央 API 仓库：
```
https://api-repo.lunalauncher.com/
├── registry.json
├── apis/
│   ├── curseforge-mod-1.0.0.js
│   ├── custom-platform-2.1.0.js
│   └── ...
└── metadata/
    ├── curseforge-mod.json
    └── custom-platform.json
```

## 故障排除

### API 未加载
1. 检查文件位置是否正确
2. 查看控制台日志: `[JSAPIManager]`
3. 验证 JS 语法是否正确
4. 确保 `metadata` 对象完整

### API 冲突
- 同一提供商的多个 API 会按优先级排序
- 设置不同的 `metadata.priority` 值

### 重新加载 API
```cpp
// 在应用代码中
JSAPIManager::instance().reloadAll();
```

## 最佳实践

### 对于核心开发者
- ✅ 稳定的 API 编译进程序（qrc）
- ✅ 同时部署到安装目录，方便独立更新
- ✅ 提供完整的 TypeScript 定义

### 对于插件开发者
- ✅ 使用用户数据目录测试
- ✅ 遵循 API 版本规范
- ✅ 提供安装脚本
- ✅ 编写详细的 README

### 对于最终用户
- ✅ 使用用户数据目录安装第三方 API
- ✅ 定期备份自定义 API
- ✅ 验证 API 来源安全性

## 参考

- [README.md](README.md) - API 开发指南
- [resource-api.d.ts](resource-api.d.ts) - TypeScript 定义
- [examples.cpp](examples.cpp) - C++ 集成示例
