/** @type {ResourceAPI} */
var api = {
    metadata: {
        id: "mcim-modrinth",
        displayName: "MCIM (Modrinth)",
        version: "1.0.0",
        provider: "MODRINTH",
        supportedTypes: [0],
        icon: "modrinth",
        description: "MCIM Modrinth mirror",
        homepage: "https://mod.mcimirror.top/docs",
        enabled: true,
        priority: 90
    },

    baseUrl: "https://mod.mcimirror.top/modrinth/v2",

    getSearchURL(args) {
        const params = ["offset=" + (args.offset || 0), "limit=25"];
        if (args.search)
            params.push("query=" + encodeURIComponent(args.search));
        if (args.sorting && args.sorting.name)
            params.push("index=" + encodeURIComponent(args.sorting.name));

        const facets = ['["project_type:mod"]'];
        const loaders = this.getLoaderStrings(args.modLoaders || 0);
        if (loaders.length)
            facets.push("[" + loaders.map(loader => '"categories:' + loader + '"').join(",") + "]");
        if (args.versions && args.versions.length)
            facets.push("[" + args.versions.map(version => '"versions:' + version + '"').join(",") + "]");
        if (args.categories && args.categories.length)
            facets.push("[" + args.categories.map(category => '"categories:' + category + '"').join(",") + "]");
        if (args.openSource)
            facets.push('["open_source:true"]');
        params.push("facets=" + encodeURIComponent("[" + facets.join(",") + "]"));
        return this.baseUrl + "/search?" + params.join("&");
    },

    getInfoURL(id) {
        return this.baseUrl + "/project/" + encodeURIComponent(id);
    },

    getVersionsURL(args) {
        const params = [];
        const loaders = this.getLoaderStrings(args.modLoaders || 0);
        if (loaders.length)
            params.push("loaders=" + encodeURIComponent(JSON.stringify(loaders)));
        if (args.versions && args.versions.length)
            params.push("game_versions=" + encodeURIComponent(JSON.stringify(args.versions)));
        const suffix = params.length ? "?" + params.join("&") : "";
        return this.baseUrl + "/project/" + encodeURIComponent(args.addonId) + "/version" + suffix;
    },

    getDependencyURL(args) {
        if (args.versionId)
            return this.baseUrl + "/version/" + encodeURIComponent(args.versionId);
        const params = [];
        const loaders = this.getLoaderStrings(args.modLoaders || 0);
        if (loaders.length)
            params.push("loaders=" + encodeURIComponent(JSON.stringify(loaders)));
        if (args.minecraftVersion)
            params.push("game_versions=" + encodeURIComponent(JSON.stringify([args.minecraftVersion])));
        return this.baseUrl + "/project/" + encodeURIComponent(args.addonId) + "/version" + (params.length ? "?" + params.join("&") : "");
    },

    loadIndexedPack(obj) {
        return {
            name: obj.title || obj.name || "",
            slug: obj.slug || "",
            description: obj.description || "",
            addonId: obj.project_id || obj.id || "",
            websiteUrl: "https://modrinth.com/mod/" + (obj.slug || obj.project_id || obj.id || ""),
            logoUrl: obj.icon_url || "",
            provider: "MODRINTH",
            authors: obj.author ? [{ name: obj.author, url: "https://modrinth.com/user/" + obj.author }] : [],
            clientSide: obj.client_side || "optional",
            serverSide: obj.server_side || "optional"
        };
    },

    loadIndexedPackVersion(obj) {
        const file = obj.files && obj.files.length ? (obj.files.find(item => item.primary) || obj.files[0]) : {};
        const hashes = file.hashes || {};
        return {
            version: obj.name || obj.version_number || "",
            versionNumber: obj.version_number || "",
            versionId: obj.id || "",
            versionType: obj.version_type || "release",
            date: obj.date_published || "",
            changelog: obj.changelog || "",
            downloadUrl: file.url || "",
            fileName: file.filename || "",
            hash: hashes.sha1 || hashes.sha512 || "",
            hashType: hashes.sha1 ? "sha1" : "sha512",
            gameVersions: obj.game_versions || [],
            loaders: obj.loaders || [],
            dependencies: (obj.dependencies || []).map(dep => ({
                projectId: dep.project_id || "",
                versionId: dep.version_id || "",
                dependencyType: dep.dependency_type || "required"
            }))
        };
    },

    loadExtraPackInfo(pack, obj) {
        pack.logoUrl = obj.icon_url || pack.logoUrl;
        pack.body = obj.body || "";
        pack.issuesUrl = obj.issues_url || "";
        pack.sourceUrl = obj.source_url || "";
        pack.wikiUrl = obj.wiki_url || "";
        pack.discordUrl = obj.discord_url || "";
        pack.status = obj.status || "";
        pack.donationUrls = obj.donation_urls || [];
        return pack;
    },

    documentToArray(doc) {
        return doc.hits || [];
    },

    getSortingMethods() {
        return [
            { index: 0, name: "relevance", displayName: "Relevance" },
            { index: 1, name: "downloads", displayName: "Downloads" },
            { index: 2, name: "follows", displayName: "Follows" },
            { index: 3, name: "newest", displayName: "Newest" },
            { index: 4, name: "updated", displayName: "Recently Updated" }
        ];
    },

    getLoaderStrings(loaders) {
        const result = [];
        if (loaders & 1) result.push("neoforge");
        if (loaders & 2) result.push("forge");
        if (loaders & 4) result.push("forge");
        if (loaders & 8) result.push("liteloader");
        if (loaders & 16) result.push("fabric");
        if (loaders & 32) result.push("quilt");
        if (loaders & 128) result.push("babric");
        if (loaders & 256) result.push("bta-babric");
        if (loaders & 512) result.push("legacy-fabric");
        if (loaders & 1024) result.push("ornithe");
        if (loaders & 2048) result.push("rift");
        if (loaders & 4096) result.push("forge");
        return result.filter((value, index, values) => values.indexOf(value) === index);
    }
};
