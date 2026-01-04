# JavaScript Resource API 框架

这是一个基于 QuickJS 的插件化资源 API 框架，允许使用 JavaScript 快速开发和测试新的资源平台支持（如 Modrinth、CurseForge 等）。

## 特性

- ✨ **快速开发**: 使用 JavaScript 编写 API，无需重新编译 C++
- 🔌 **插件化**: 动态加载 JS 文件，支持热更新
- 🌐 **完整功能**: 支持搜索、项目信息、版本列表等完整 API 功能
- 🛠️ **易于测试**: 可以快速迭代和调试 API 实现
- 📦 **社区友好**: 第三方开发者可以轻松贡献新平台支持
- 🔍 **TypeScript 支持**: 提供完整的类型定义文件（.d.ts）
- 🚀 **自动注册**: 通过元数据声明自动注册到对应资源类型

## 架构

```
┌─────────────────┐
│   C++ 应用层    │  (ResourceDownloadDialog, PluginPage 等)
└────────┬────────┘
         │
┌────────▼────────┐
│  JSResourceAPI  │  (C++ 桥接层)
└────────┬────────┘
         │
┌────────▼────────┐
│  QuickJS 引擎   │
└────────┬────────┘
         │
┌────────▼────────┐
│  JavaScript API │  (modrinth_plugin_api.js 等)
└─────────────────┘
```

## 使用方法

### 1. 创建 JavaScript API 实现

创建一个 `.js` 文件，导出一个 `api` 对象，**必须包含 `metadata` 属性**：

```javascript
/**
 * @type {ResourceAPI}
 */
const api = {
    // ===== 元数据声明（必需）=====
    metadata: {
        id: "example-plugin",              // 唯一标识符（必需）
        displayName: "Example Platform",   // 显示名称（必需）
        version: "1.0.0",                  // API 版本（必需）
        provider: "EXAMPLE",               // 资源提供商（必需）
        supportedTypes: [3],               // 支持的资源类型（必需）
                                          // 0=MOD, 1=RESOURCEPACK, 2=SHADERPACK, 3=PLUGIN
        icon: "example",                   // 图标名称（可选）
        description: "Example API",        // 描述（可选）
        homepage: "https://example.com",   // 主页（可选）
        enabled: true,                     // 是否启用（可选，默认 true）
        priority: 100                      // 优先级（可选，默认 0，越大越优先）
    },

    // 获取搜索 URL
    getSearchURL(args) {
        return `https://api.example.com/search?q=${args.search}`;
    },

    // 获取项目信息 URL
    getInfoURL(id) {
        return `https://api.example.com/project/${id}`;
    },

    // 获取版本列表 URL
    getVersionsURL(args) {
        return `https://api.example.com/project/${args.addonId}/versions`;
    },

    // 解析搜索结果中的单个项目
    loadIndexedPack(obj) {
        return {
            name: obj.title,
            addonId: obj.id,
            description: obj.description,
            websiteUrl: obj.url,
            logoUrl: obj.icon,
            provider: "EXAMPLE"
        };
    },

    // 解析版本信息
    loadIndexedPackVersion(obj, type) {
        return {
            version: obj.version_number,
            downloadUrl: obj.files[0].url,
            fileName: obj.files[0].filename,
            gameVersions: obj.game_versions,
            loaders: obj.loaders
        };
    },

    // 加载额外项目信息
    loadExtraPackInfo(pack, obj) {
        pack.body = obj.description;
        pack.logoUrl = obj.icon_url;
        return pack;
    },

    // 转换 API 响应为数组
    documentToArray(doc) {
        return doc.hits || doc.results || [];
    },

    // 获取排序方法列表
    getSortingMethods() {
        return [
            { name: "relevance", displayName: "Relevance" },
            { name: "downloads", displayName: "Downloads" }
        ];
    }
};

console.log("[ExampleAPI] Initialized");
```

### 2. 在 C++ 中加载和使用

#### 方式一：直接加载单个 API
```cpp
#include "modplatform/jsapi/JSResourceAPI.h"

// 从文件加载
auto api = JSResourceAPI::fromFile(
    ":/jsapi/modrinth_plugin_api.js",
    "ModrinthPluginAPI"
);

// 或从字符串加载
auto api = JSResourceAPI::fromString(jsCode, "CustomAPI");

if (api) {
    // 使用 API
    ResourceAPI::SearchArgs args;
    args.search = "worldedit";

    auto url = api->getSearchURL(args);
    // 发起网络请求...
}
```

#### 方式二：使用 API 管理器（推荐）
```cpp
#include "modplatform/jsapi/JSAPIManager.h"

// 初始化管理器（应用启动时调用一次）
JSAPIManager::instance().initialize();

// 获取特定类型的所有 API
auto apis = JSAPIManager::instance().getAPIsForType(
    ModPlatform::ResourceType::PLUGIN
);

// 根据 ID 获取 API
auto api = JSAPIManager::instance().getAPIById("modrinth-plugin");

// 根据提供商获取 API
auto apis = JSAPIManager::instance().getAPIsByProvider("MODRINTH");
```

### 3. 集成到下载对话框

```cpp
// 在 ResourceDownloadDialog 或 PluginPage 中
auto modrinthAPI = JSResourceAPI::fromFile(
    ":/jsapi/modrinth_plugin_api.js",
    "ModrinthPluginAPI"
);

m_model = new PluginModel(instance, modrinthAPI.get(),
                          "Modrinth", "ModrinthPlugins");
```

## API 对象接口

JavaScript `api` 对象需要实现以下方法：

### 元数据属性（必需）

#### `metadata`
API 元数据对象，用于自动注册和配置。

**必需字段**:
- `id`: string - API 唯一标识符
- `displayName`: string - 显示名称
- `version`: string - API 版本
- `provider`: string - 资源提供商（MODRINTH, HANGAR, FLAME 等）
- `supportedTypes`: number[] - 支持的资源类型数组
  - `0` = MOD
  - `1` = RESOURCEPACK
  - `2` = SHADERPACK
  - `3` = PLUGIN

**可选字段**:
- `icon`: string - 图标名称或 URL
- `description`: string - API 描述
- `homepage`: string - 主页 URL
- `enabled`: boolean - 是否启用（默认 true）
- `priority`: number - 优先级，数字越大越优先（默认 0）

**示例**:
```javascript
metadata: {
    id: "modrinth-plugin",
    displayName: "Modrinth",
    version: "2.0.0",
    provider: "MODRINTH",
    supportedTypes: [3],  // 只支持插件
    icon: "modrinth",
    description: "Modrinth 服务器插件平台 API",
    homepage: "https://modrinth.com",
    enabled: true,
    priority: 100
}
```

### 必需方法

#### `getSearchURL(args)`
生成搜索 URL。

**参数**:
- `args.offset`: 分页偏移量
- `args.search`: 搜索关键词（可选）
- `args.sorting`: 排序方式对象（可选）
- `args.pluginLoaders`: 插件加载器位标志（可选）
- `args.versions`: Minecraft 版本数组（可选）

**返回**: URL 字符串

#### `getInfoURL(id)`
生成项目详情 URL。

**参数**:
- `id`: 项目 ID 或 slug

**返回**: URL 字符串

#### `getVersionsURL(args)`
生成版本列表 URL。

**参数**:
- `args.addonId`: 项目 ID
- `args.pluginLoaders`: 插件加载器过滤（可选）
- `args.versions`: Minecraft 版本过滤（可选）

**返回**: URL 字符串

#### `loadIndexedPack(obj)`
解析搜索结果中的单个项目。

**参数**:
- `obj`: API 返回的原始 JSON 对象

**返回**: 标准化的项目对象
```javascript
{
    name: string,           // 项目名称
    slug: string,           // URL slug
    description: string,    // 简短描述
    addonId: string,        // 项目 ID
    websiteUrl: string,     // 项目网页 URL
    logoUrl: string,        // Logo URL
    provider: string,       // 平台名称 (MODRINTH, HANGAR 等)
    authors: [...],         // 作者列表
    downloads: number,      // 下载量（可选）
    follows: number         // 关注数（可选）
}
```

#### `loadIndexedPackVersion(obj, type)`
解析版本信息。

**参数**:
- `obj`: API 返回的版本 JSON 对象
- `type`: 资源类型（数字）

**返回**: 标准化的版本对象
```javascript
{
    version: string,        // 版本号
    versionId: string,      // 版本 ID
    downloadUrl: string,    // 下载 URL
    fileName: string,       // 文件名
    fileSize: number,       // 文件大小
    gameVersions: [...],    // 支持的 MC 版本
    loaders: [...],         // 支持的加载器
    dependencies: [...],    // 依赖列表（可选）
    date: string,           // 发布日期（可选）
    changelog: string       // 更新日志（可选）
}
```

#### `documentToArray(doc)`
将 API 响应转换为结果数组。

**参数**:
- `doc`: API 返回的 JSON 文档

**返回**: 结果数组

#### `getSortingMethods()`
获取支持的排序方法列表。

**返回**: 排序方法数组
```javascript
[
    { name: "relevance", displayName: "Relevance" },
    { name: "downloads", displayName: "Downloads" }
]
```

### 可选方法

#### `loadExtraPackInfo(pack, obj)`
加载项目的额外详情（Logo、完整描述、链接等）。

**参数**:
- `pack`: 当前项目对象
- `obj`: API 返回的项目详情 JSON

**返回**: 更新后的项目对象

## 辅助工具

### JavaScript 环境提供的全局对象

#### `console`
```javascript
console.log("Debug message");
```

#### `JSON`
```javascript
const obj = JSON.parse(jsonString);
const str = JSON.stringify(obj);
```

#### `http` (规划中)
```javascript
const response = await http.get("https://api.example.com/data");
```

## 插件加载器位标志

用于过滤插件加载器的位标志常量：

```javascript
// 位标志值
0x01  // Paper
0x02  // Spigot
0x04  // Bukkit
0x08  // Purpur
0x10  // Sponge
0x20  // Velocity
0x40  // Waterfall
0x80  // BungeeCord

// 转换为字符串数组示例
function getLoaderStrings(loaders) {
    const strings = [];
    if (loaders & 0x01) strings.push("paper");
    if (loaders & 0x02) strings.push("spigot");
    // ...
    return strings;
}
```

## 示例

### Modrinth Plugin API
参见 [modrinth_plugin_api.js](modrinth_plugin_api.js)

### 测试

可以使用 Node.js 独立测试 API 实现：

```bash
node tests/test_modrinth_api.js
```

测试文件模拟了 C++ 环境提供的全局对象（`console`, `http`），可以验证 API 的所有功能是否正常工作。

## 开发建议

1. **使用 TypeScript**: 项目提供了完整的类型定义文件 [resource-api.d.ts](resource-api.d.ts)，建议使用 TypeScript 或在 JavaScript 中添加 JSDoc 注释以获得 IDE 支持
2. **元数据优先**: 确保 `metadata` 对象完整且正确，这是自动注册的基础
3. **保持简洁**: JavaScript 实现应该专注于 URL 生成和数据转换
4. **错误处理**: 使用 `console.log()` 记录关键信息，便于调试
5. **兼容性**: 确保返回的数据结构符合标准格式
6. **测试**: 在 examples.cpp 中添加测试用例
7. **文档**: 注释清楚每个方法的用途和参数

## TypeScript 支持

项目提供了完整的 TypeScript 定义文件，在你的 JS 文件顶部添加：

```javascript
/// <reference path="resource-api.d.ts" />

/**
 * @type {ResourceAPI}
 */
const api = {
    // TypeScript 会提供完整的类型提示和检查
};
```

或者直接使用 TypeScript 编写，然后编译为 JavaScript。

## 未来计划

- [ ] 异步网络请求支持
- [ ] Promise/async-await 支持
- [ ] 更多内置工具函数
- [ ] 调试器集成
- [ ] 热重载功能
- [ ] 性能监控

## 贡献

欢迎贡献新的平台 API 实现！只需：

1. 创建新的 `.js` 文件
2. 实现 `api` 对象和必需方法
3. 在 examples.cpp 中添加测试
4. 提交 Pull Request

## 许可证

GPL-3.0-only - 与 LunaLauncher 主项目相同
