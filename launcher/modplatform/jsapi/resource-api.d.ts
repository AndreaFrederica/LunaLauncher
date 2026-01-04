/**
 * Luna Launcher - JavaScript Resource API 类型定义
 *
 * 为 JavaScript API 开发提供完整的 TypeScript 类型支持
 */

// ============================================================================
// 枚举类型
// ============================================================================

/**
 * 资源类型
 */
declare enum ResourceType {
    MOD = 0,
    PLUGIN = 1,
    RESOURCEPACK = 2,
    SHADERPACK = 3,
    MODPACK = 4,
    DATAPACK = 5,
    WORLD = 6,
    SCREENSHOTS = 7,
    TEXTUREPACK = 8,
    UNKNOWN = 9
}

/**
 * 资源提供商
 */
declare enum ResourceProvider {
    MODRINTH = 0,
    FLAME = 1,
    HANGAR = 2
}

/**
 * 插件加载器位标志
 */
declare enum PluginLoaderType {
    Paper = 0x01,
    Spigot = 0x02,
    Bukkit = 0x04,
    Purpur = 0x08,
    Sponge = 0x10,
    Velocity = 0x20,
    Waterfall = 0x40,
    BungeeCord = 0x80
}

/**
 * Mod 加载器位标志
 */
declare enum ModLoaderType {
    Forge = 0x01,
    Cauldron = 0x02,
    LiteLoader = 0x04,
    Fabric = 0x08,
    Quilt = 0x10,
    NeoForge = 0x20
}

// ============================================================================
// API 参数接口
// ============================================================================

/**
 * 搜索参数
 */
interface SearchArgs {
    /** 分页偏移量 */
    offset: number;

    /** 搜索关键词 */
    search?: string;

    /** 排序方式 */
    sorting?: SortingMethod;

    /** 插件加载器过滤（位标志） */
    pluginLoaders?: number;

    /** Mod 加载器过滤（位标志） */
    modLoaders?: number;

    /** Minecraft 版本过滤 */
    versions?: string[];

    /** 分类标签过滤 */
    categories?: string[];
}

/**
 * 版本搜索参数
 */
interface VersionSearchArgs {
    /** 项目 ID */
    addonId: string;

    /** 插件加载器过滤 */
    pluginLoaders?: number;

    /** Mod 加载器过滤 */
    modLoaders?: number;

    /** Minecraft 版本过滤 */
    versions?: string[];
}

/**
 * 依赖搜索参数
 */
interface DependencySearchArgs {
    /** 项目 ID */
    addonId: string;

    /** 版本 ID */
    versionId?: string;
}

// ============================================================================
// 数据结构接口
// ============================================================================

/**
 * 排序方法
 */
interface SortingMethod {
    /** 排序名称（用于 API 请求） */
    name: string;

    /** 显示名称（用于 UI） */
    displayName: string;
}

/**
 * 作者信息
 */
interface Author {
    /** 作者名称 */
    name: string;

    /** 作者主页 URL */
    url?: string;
}

/**
 * 项目/资源包
 */
interface IndexedPack {
    /** 项目名称 */
    name: string;

    /** URL slug */
    slug: string;

    /** 简短描述 */
    description: string;

    /** 项目 ID */
    addonId: string;

    /** 项目网页 URL */
    websiteUrl: string;

    /** Logo/图标 URL */
    logoUrl?: string;

    /** 资源提供商 */
    provider: string;

    /** 作者列表 */
    authors: Author[];

    /** 下载量 */
    downloads?: number;

    /** 关注/收藏数 */
    follows?: number;

    /** 分类标签 */
    categories?: string[];

    /** 客户端支持 (required/optional/unsupported) */
    clientSide?: string;

    /** 服务端支持 (required/optional/unsupported) */
    serverSide?: string;

    /** 详细描述/正文（Markdown） */
    body?: string;

    /** 创建日期 */
    dateCreated?: string;

    /** 修改日期 */
    dateModified?: string;

    /** 许可证 */
    license?: string;

    /** 源代码 URL */
    sourceUrl?: string;

    /** 问题追踪 URL */
    issuesUrl?: string;

    /** Wiki URL */
    wikiUrl?: string;

    /** Discord URL */
    discordUrl?: string;

    /** 捐赠链接 */
    donationUrls?: Array<{ id: string; platform: string; url: string }>;
}

/**
 * 版本信息
 */
interface IndexedVersion {
    /** 版本号 */
    version: string;

    /** 版本 ID */
    versionId: string;

    /** 版本类型 (release/beta/alpha) */
    versionType?: string;

    /** 下载 URL */
    downloadUrl: string;

    /** 文件名 */
    fileName: string;

    /** 文件大小（字节） */
    fileSize?: number;

    /** 支持的 Minecraft 版本 */
    gameVersions: string[];

    /** 支持的加载器 */
    loaders: string[];

    /** 依赖列表 */
    dependencies?: Dependency[];

    /** 发布日期 */
    date?: string;

    /** 更新日志 */
    changelog?: string;

    /** 文件哈希值 */
    hash?: string;

    /** 哈希算法 (sha1/sha256/sha512) */
    hashType?: string;

    /** 是否为主要文件 */
    isPrimary?: boolean;
}

/**
 * 依赖关系
 */
interface Dependency {
    /** 依赖项目 ID 或版本 ID */
    projectId: string;

    /** 依赖类型 (required/optional/incompatible/embedded) */
    dependencyType: string;

    /** 依赖项目名称（可选） */
    projectName?: string;
}

// ============================================================================
// API 元数据接口
// ============================================================================

/**
 * API 元数据声明
 * 用于自动注册和配置
 */
interface ApiMetadata {
    /** API 唯一标识符 */
    id: string;

    /** API 显示名称 */
    displayName: string;

    /** API 版本 */
    version: string;

    /** 资源提供商类型 */
    provider: ResourceProvider | string;

    /** 支持的资源类型 */
    supportedTypes: ResourceType[];

    /** 图标名称或 URL */
    icon?: string;

    /** API 描述 */
    description?: string;

    /** 主页 URL */
    homepage?: string;

    /** 是否默认启用 */
    enabled?: boolean;

    /** 优先级（用于排序，数字越大优先级越高） */
    priority?: number;
}

// ============================================================================
// 全局 API 对象接口
// ============================================================================

/**
 * 资源 API 接口
 * 所有 JavaScript API 实现必须导出此接口
 */
interface ResourceAPI {
    /**
     * API 元数据（必需）
     * 用于自动注册和配置
     */
    readonly metadata: ApiMetadata;

    /**
     * 获取搜索 URL
     * @param args 搜索参数
     * @returns 搜索 URL，如果不支持返回 null
     */
    getSearchURL(args: SearchArgs): string | null;

    /**
     * 获取项目信息 URL
     * @param id 项目 ID 或 slug
     * @returns 项目详情 URL，如果不支持返回 null
     */
    getInfoURL(id: string): string | null;

    /**
     * 获取版本列表 URL
     * @param args 版本搜索参数
     * @returns 版本列表 URL，如果不支持返回 null
     */
    getVersionsURL(args: VersionSearchArgs): string | null;

    /**
     * 获取依赖信息 URL（可选）
     * @param args 依赖搜索参数
     * @returns 依赖信息 URL，如果不支持返回 null
     */
    getDependencyURL?(args: DependencySearchArgs): string | null;

    /**
     * 解析搜索结果中的单个项目
     * @param obj API 返回的原始 JSON 对象
     * @returns 标准化的项目对象
     */
    loadIndexedPack(obj: any): IndexedPack;

    /**
     * 解析版本信息
     * @param obj API 返回的版本 JSON 对象
     * @param type 资源类型
     * @returns 标准化的版本对象
     */
    loadIndexedPackVersion(obj: any, type: ResourceType): IndexedVersion;

    /**
     * 加载项目的额外详情（可选）
     * @param pack 当前项目对象
     * @param obj API 返回的项目详情 JSON
     * @returns 更新后的项目对象
     */
    loadExtraPackInfo?(pack: IndexedPack, obj: any): IndexedPack;

    /**
     * 将 API 响应转换为结果数组
     * @param doc API 返回的 JSON 文档
     * @returns 结果数组
     */
    documentToArray(doc: any): any[];

    /**
     * 获取支持的排序方法列表
     * @returns 排序方法数组
     */
    getSortingMethods(): SortingMethod[];
}

// ============================================================================
// 全局对象和工具函数
// ============================================================================

/**
 * Console 对象
 * 用于日志输出
 */
declare const console: {
    log(...args: any[]): void;
    warn(...args: any[]): void;
    error(...args: any[]): void;
};

/**
 * JSON 对象
 * 用于 JSON 解析和序列化
 */
declare const JSON: {
    parse(text: string): any;
    stringify(value: any, replacer?: any, space?: string | number): string;
};

/**
 * HTTP 客户端（未来支持）
 */
declare const http: {
    get(url: string, options?: any): Promise<any>;
    post(url: string, data?: any, options?: any): Promise<any>;
};

// ============================================================================
// 工具函数
// ============================================================================

/**
 * URL 编码
 */
declare function encodeURIComponent(str: string): string;

/**
 * URL 解码
 */
declare function decodeURIComponent(str: string): string;

// ============================================================================
// 导出声明
// ============================================================================

/**
 * 导出的 API 对象
 * JavaScript 文件必须导出名为 'api' 的对象
 */
declare const api: ResourceAPI;
