/**
 * Modrinth Plugin API - JavaScript 实现
 *
 * 这是一个用 JavaScript 实现的 Modrinth 插件 API，展示了如何使用
 * JSResourceAPI 框架快速开发新的资源平台支持。
 *
 * API 文档: https://docs.modrinth.com/api-spec/
 *
 * @type {ResourceAPI}
 */

var api = {
    /**
     * API 元数据
     * 用于自动注册到对应的资源类型提供器
     */
    metadata: {
        id: "modrinth-plugin",
        displayName: "Modrinth",
        version: "2.0.0",
        provider: "MODRINTH",
        supportedTypes: [1],  // ResourceType.PLUGIN
        icon: "modrinth",
        description: "Modrinth 服务器插件平台 API",
        homepage: "https://modrinth.com",
        enabled: true,
        priority: 100
    },
    /**
     * 获取搜索 URL
     * @param {Object} args - 搜索参数
     * @returns {string} 搜索 URL
     */
    getSearchURL(args) {
        const baseURL = "https://api.modrinth.com/v2/search";
        const params = [];

        // 分页
        params.push(`offset=${args.offset || 0}`);
        params.push(`limit=25`);

        // 搜索词
        if (args.search) {
            params.push(`query=${encodeURIComponent(args.search)}`);
        }

        // 项目类型 - 只搜索插件
        const facets = [];
        facets.push('["project_type:plugin"]');

        // 插件加载器过滤
        if (args.pluginLoaders) {
            const loaders = this.getLoaderFacets(args.pluginLoaders);
            if (loaders.length > 0) {
                facets.push(`[${loaders.join(',')}]`);
            }
        }

        // Minecraft 版本过滤
        if (args.versions && args.versions.length > 0) {
            const versions = args.versions.map(v => `"versions:${v}"`);
            facets.push(`[${versions.join(',')}]`);
        }

        if (facets.length > 0) {
            params.push(`facets=[${facets.join(',')}]`);
        }

        // 排序方式
        if (args.sorting) {
            params.push(`index=${args.sorting.name}`);
        }

        const url = `${baseURL}?${params.join('&')}`;
        // console.log(`[ModrinthAPI] Search URL: ${url}`);
        return url;
    },

    /**
     * 获取项目信息 URL
     * @param {string} id - 项目 ID 或 slug
     * @returns {string} 项目详情 URL
     */
    getInfoURL(id) {
        return `https://api.modrinth.com/v2/project/${id}`;
    },

    /**
     * 获取版本列表 URL
     * @param {Object} args - 版本搜索参数
     * @returns {string} 版本列表 URL
     */
    getVersionsURL(args) {
        const projectId = args.addonId;
        const baseURL = `https://api.modrinth.com/v2/project/${projectId}/version`;
        const params = [];

        // 插件加载器过滤
        if (args.pluginLoaders) {
            const loaders = this.getLoaderStrings(args.pluginLoaders);
            if (loaders.length > 0) {
                params.push(`loaders=${JSON.stringify(loaders)}`);
            }
        }

        // Minecraft 版本过滤
        if (args.versions && args.versions.length > 0) {
            params.push(`game_versions=${JSON.stringify(args.versions)}`);
        }

        const url = params.length > 0 ? `${baseURL}?${params.join('&')}` : baseURL;
        // console.log(`[ModrinthAPI] Versions URL: ${url}`);
        return url;
    },

    /**
     * 解析搜索结果中的单个项目
     * @param {Object} obj - Modrinth 项目 JSON 对象
     * @returns {Object} 标准化的项目对象
     */
    loadIndexedPack(obj) {
        const pack = {
            name: obj.title || obj.name || "",
            slug: obj.slug || "",
            description: obj.description || "",
            addonId: obj.project_id || obj.id || "",
            websiteUrl: `https://modrinth.com/plugin/${obj.slug}`,
            logoUrl: obj.icon_url || obj.logo_url || "",
            provider: "MODRINTH",
            authors: []
        };

        // 作者信息
        if (obj.author) {
            pack.authors.push({
                name: obj.author,
                url: `https://modrinth.com/user/${obj.author}`
            });
        }

        // 下载量和关注数
        if (obj.downloads !== undefined) {
            pack.downloads = obj.downloads;
        }
        if (obj.followers !== undefined) {
            pack.follows = obj.followers;
        }

        // 分类标签
        if (obj.categories && Array.isArray(obj.categories)) {
            pack.categories = obj.categories;
        }

        // 客户端/服务端支持
        if (obj.client_side) {
            pack.clientSide = obj.client_side;
        }
        if (obj.server_side) {
            pack.serverSide = obj.server_side;
        }

        // console.log(`[ModrinthAPI] Loaded pack: ${pack.name} (${pack.addonId})`);
        return pack;
    },

    /**
     * 解析版本信息
     * @param {Object} obj - Modrinth 版本 JSON 对象
     * @param {number} type - 资源类型
     * @returns {Object} 标准化的版本对象
     */
    loadIndexedPackVersion(obj, type) {
        const version = {
            version: obj.version_number || obj.name || "",
            versionId: obj.id || "",
            versionType: obj.version_type || "release",
            date: obj.date_published || "",
            changelog: obj.changelog || "",
            downloadUrl: "",
            fileName: "",
            fileSize: 0,
            dependencies: [],
            gameVersions: obj.game_versions || [],
            loaders: obj.loaders || []
        };

        // 查找主文件（primary file）
        if (obj.files && Array.isArray(obj.files) && obj.files.length > 0) {
            const primaryFile = obj.files.find(f => f.primary) || obj.files[0];

            version.downloadUrl = primaryFile.url || "";
            version.fileName = primaryFile.filename || "";
            version.fileSize = primaryFile.size || 0;

            // 文件哈希
            if (primaryFile.hashes) {
                version.hash = primaryFile.hashes.sha1 || primaryFile.hashes.sha512 || "";
                version.hashType = primaryFile.hashes.sha1 ? "sha1" : "sha512";
            }
        }

        // 依赖关系
        if (obj.dependencies && Array.isArray(obj.dependencies)) {
            for (const dep of obj.dependencies) {
                version.dependencies.push({
                    projectId: dep.project_id || dep.version_id || "",
                    dependencyType: dep.dependency_type || "required"
                });
            }
        }

        // console.log(`[ModrinthAPI] Loaded version: ${version.version} - ${version.downloadUrl}`);
        return version;
    },

    /**
     * 加载额外的项目信息（详情页）
     * @param {Object} pack - 当前项目对象
     * @param {Object} obj - Modrinth 项目详情 JSON
     * @returns {Object} 更新后的项目对象
     */
    loadExtraPackInfo(pack, obj) {
        // console.log(`[ModrinthAPI] loadExtraPackInfo called for ${pack.name}`);
        // if (obj.body) {
        //     console.log(`[ModrinthAPI] obj.body present, length: ${obj.body.length}`);
        // } else {
        //     console.log(`[ModrinthAPI] obj.body MISSING`);
        // }

        // Logo URL
        if (obj.icon_url) {
            pack.logoUrl = obj.icon_url;
        }

        // 项目主体内容（Markdown）
        if (obj.body) {
            pack.body = obj.body;
        }

        // 更新日期
        if (obj.updated) {
            pack.dateModified = obj.updated;
        }
        if (obj.published) {
            pack.dateCreated = obj.published;
        }

        // 许可证
        if (obj.license) {
            pack.license = obj.license.id || obj.license.name || "";
        }

        // 源代码和问题追踪
        if (obj.source_url) {
            pack.sourceUrl = obj.source_url;
        }
        if (obj.issues_url) {
            pack.issuesUrl = obj.issues_url;
        }
        if (obj.wiki_url) {
            pack.wikiUrl = obj.wiki_url;
        }
        if (obj.discord_url) {
            pack.discordUrl = obj.discord_url;
        }

        // 捐赠链接
        if (obj.donation_urls && Array.isArray(obj.donation_urls)) {
            pack.donationUrls = obj.donation_urls;
        }

        // 状态
        if (obj.status) {
            pack.status = obj.status;
        }

        // 画廊/截图
        if (obj.gallery && Array.isArray(obj.gallery) && obj.gallery.length > 0) {
            let galleryMd = "\n\n## Gallery\n";
            for (const img of obj.gallery) {
                galleryMd += `![${img.title || ''}](${img.url})\n`;
                if (img.description) {
                    galleryMd += `*${img.description}*\n`;
                }
                galleryMd += "\n";
            }

            if (pack.body) {
                pack.body += galleryMd;
            } else {
                pack.body = galleryMd;
            }
        }

        // console.log(`[ModrinthAPI] Loaded extra info for: ${pack.name}`);
        return pack;
    },

    /**
     * 将 API 响应转换为数组
     * @param {Object} doc - JSON 文档
     * @returns {Array} 结果数组
     */
    documentToArray(doc) {
        // Modrinth 搜索 API 返回 { hits: [...], ... }
        if (doc.hits && Array.isArray(doc.hits)) {
            return doc.hits;
        }

        // 版本列表直接是数组
        if (Array.isArray(doc)) {
            return doc;
        }

        return [];
    },

    /**
     * 获取支持的排序方法
     * @returns {Array} 排序方法列表
     */
    getSortingMethods() {
        return [
            { name: "relevance", displayName: "Relevance" },
            { name: "downloads", displayName: "Downloads" },
            { name: "follows", displayName: "Follows" },
            { name: "newest", displayName: "Newest" },
            { name: "updated", displayName: "Recently Updated" }
        ];
    },

    /**
     * 将插件加载器类型转换为 Modrinth facet 格式
     * @param {number} loaders - 插件加载器位标志
     * @returns {Array} facet 字符串数组
     */
    getLoaderFacets(loaders) {
        const facets = [];
        const categories = [];

        // 兼容性处理：Paper > Spigot > Bukkit
        if (loaders & 0x01) { // Paper
            categories.push("paper");
            categories.push("spigot");
            categories.push("bukkit");
        }
        else if (loaders & 0x02) { // Spigot
            categories.push("spigot");
            categories.push("bukkit");
        }
        else if (loaders & 0x04) { // Bukkit
            categories.push("bukkit");
        }

        if (loaders & 0x08) categories.push("purpur");
        if (loaders & 0x10) categories.push("sponge");
        if (loaders & 0x20) categories.push("velocity");
        if (loaders & 0x40) categories.push("waterfall");
        if (loaders & 0x80) categories.push("bungeecord");

        // 去重并转换为 facet 格式
        const uniqueCategories = [...new Set(categories)];
        return uniqueCategories.map(c => `"categories:${c}"`);
    },

    /**
     * 将插件加载器类型转换为字符串数组
     * @param {number} loaders - 插件加载器位标志
     * @returns {Array} 加载器名称数组
     */
    getLoaderStrings(loaders) {
        const strings = [];

        // 兼容性处理：Paper > Spigot > Bukkit
        if (loaders & 0x01) { // Paper
            strings.push("paper");
            strings.push("spigot");
            strings.push("bukkit");
        }
        else if (loaders & 0x02) { // Spigot
            strings.push("spigot");
            strings.push("bukkit");
        }
        else if (loaders & 0x04) { // Bukkit
            strings.push("bukkit");
        }

        if (loaders & 0x08) strings.push("purpur");
        if (loaders & 0x10) strings.push("sponge");
        if (loaders & 0x20) strings.push("velocity");
        if (loaders & 0x40) strings.push("waterfall");
        if (loaders & 0x80) strings.push("bungeecord");

        return [...new Set(strings)];
    }
};

// C++ 端会通过 JS_GetPropertyStr(global, "api") 获取这个对象
// 注意: 顶部的 const api = { ... } 已经将 api 对象导出到全局作用域
// console.log("[ModrinthAPI] JavaScript API initialized");
