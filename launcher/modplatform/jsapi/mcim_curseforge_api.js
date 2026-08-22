/** @type {ResourceAPI} */
var api = {
    metadata: {
        id: "mcim-curseforge",
        displayName: "MCIM (CurseForge)",
        version: "1.0.0",
        provider: "CURSEFORGE",
        supportedTypes: [0],
        icon: "flame",
        description: "MCIM CurseForge mirror",
        homepage: "https://mod.mcimirror.top/docs",
        enabled: true,
        priority: 80
    },

    baseUrl: "https://mod.mcimirror.top/curseforge/v1",

    getSearchURL(args) {
        const params = ["gameId=432", "classId=6", "index=" + (args.offset || 0), "pageSize=25"];
        if (args.search)
            params.push("searchFilter=" + encodeURIComponent(args.search));
        if (args.sorting && args.sorting.name)
            params.push("sortField=" + encodeURIComponent(args.sorting.name));
        if (args.versions && args.versions.length)
            params.push("gameVersions=" + encodeURIComponent(args.versions.join(",")));
        const loaders = this.getLoaderIds(args.modLoaders || 0);
        if (loaders.length)
            params.push("modLoaderTypes=" + loaders.join(","));
        if (args.categories && args.categories.length)
            params.push("categoryIds=" + encodeURIComponent(args.categories.join(",")));
        return this.baseUrl + "/mods/search?" + params.join("&");
    },

    getInfoURL(id) {
        return this.baseUrl + "/mods/" + encodeURIComponent(id);
    },

    getVersionsURL(args) {
        const params = ["pageSize=10000"];
        if (args.versions && args.versions.length)
            params.push("gameVersion=" + encodeURIComponent(args.versions[0]));
        const loaders = this.getLoaderIds(args.modLoaders || 0);
        if (loaders.length === 1)
            params.push("modLoaderType=" + loaders[0]);
        return this.baseUrl + "/mods/" + encodeURIComponent(args.addonId) + "/files?" + params.join("&");
    },

    getDependencyURL(args) {
        const params = ["pageSize=10000"];
        if (args.minecraftVersion)
            params.push("gameVersion=" + encodeURIComponent(args.minecraftVersion));
        const loaders = this.getLoaderIds(args.modLoaders || 0);
        if (loaders.length === 1)
            params.push("modLoaderType=" + loaders[0]);
        return this.baseUrl + "/mods/" + encodeURIComponent(args.addonId) + "/files?" + params.join("&");
    },

    loadIndexedPack(obj) {
        const links = obj.links || {};
        return {
            name: obj.name || "",
            slug: obj.slug || "",
            description: obj.summary || "",
            addonId: obj.id || "",
            websiteUrl: links.websiteUrl || "https://www.curseforge.com/minecraft/mc-mods/" + (obj.slug || ""),
            logoUrl: obj.logo ? (obj.logo.thumbnailUrl || obj.logo.url || "") : "",
            provider: "CURSEFORGE",
            authors: (obj.authors || []).map(author => ({ name: author.name || "", url: author.url || "" })),
            clientSide: "optional",
            serverSide: "optional"
        };
    },

    loadIndexedPackVersion(obj) {
        const releaseTypes = { 1: "release", 2: "beta", 3: "alpha" };
        const relationTypes = { 1: "embedded", 2: "optional", 3: "required", 5: "incompatible" };
        const hashes = obj.hashes || [];
        const sha1 = hashes.find(hash => hash.algo === 1);
        const md5 = hashes.find(hash => hash.algo === 2);
        const gameVersions = (obj.gameVersions || []).filter(version => /^(?:\d|[A-Za-z]+\d)/.test(version));
        const loaderNames = (obj.gameVersions || []).map(this.normalizeLoader).filter(Boolean);
        return {
            version: obj.displayName || obj.fileName || "",
            versionNumber: obj.displayName || "",
            versionId: obj.id || "",
            versionType: releaseTypes[obj.releaseType] || "release",
            date: obj.fileDate || "",
            downloadUrl: obj.downloadUrl || "",
            fileName: obj.fileName || "",
            hash: sha1 ? sha1.value : (md5 ? md5.value : ""),
            hashType: sha1 ? "sha1" : (md5 ? "md5" : ""),
            gameVersions: gameVersions,
            loaders: loaderNames,
            dependencies: (obj.dependencies || []).map(dep => ({
                projectId: dep.modId || "",
                dependencyType: relationTypes[dep.relationType] || "unknown"
            }))
        };
    },

    loadExtraPackInfo(pack, obj) {
        const links = obj.links || {};
        pack.logoUrl = obj.logo ? (obj.logo.url || obj.logo.thumbnailUrl || pack.logoUrl) : pack.logoUrl;
        pack.issuesUrl = links.issuesUrl || "";
        pack.sourceUrl = links.sourceUrl || "";
        pack.wikiUrl = links.wikiUrl || "";
        pack.status = String(obj.status || "");
        return pack;
    },

    documentToArray(doc) {
        return doc.data || [];
    },

    getSortingMethods() {
        return [
            { index: 2, name: "2", displayName: "Popularity" },
            { index: 6, name: "6", displayName: "Total Downloads" },
            { index: 3, name: "3", displayName: "Recently Updated" },
            { index: 4, name: "4", displayName: "Name" }
        ];
    },

    getLoaderIds(loaders) {
        const result = [];
        if (loaders & 1) result.push(6);
        if (loaders & 2) result.push(1);
        if (loaders & 4) result.push(2);
        if (loaders & 8) result.push(3);
        if (loaders & 16) result.push(4);
        if (loaders & 32) result.push(5);
        if (loaders & 4096) result.push(1);
        return result.filter((value, index, values) => values.indexOf(value) === index);
    },

    normalizeLoader(value) {
        const loaders = {
            "neoforge": "neoforge", "forge": "forge", "cauldron": "cauldron",
            "liteloader": "liteloader", "fabric": "fabric", "quilt": "quilt"
        };
        return loaders[String(value).toLowerCase()] || "";
    }
};
