/**
 * BMCLAPI URL mapping.
 *
 * Mirror mappings live in JS so another service can ship its own rewriteUrl
 * hook without changing the launcher binary.
 */
var api = {
    metadata: {
        id: "bmclapi-mirror",
        kind: "mirror",
        displayName: "BMCLAPI",
        version: "1.0.0",
        enabled: true,
        priority: 100
    },

    rewriteUrl(url, context) {
        if (!context || context.mirrorTypeId !== 1)
            return null;

        function replaceHost(value, host, base, pathPrefix) {
            if (!base || !value)
                return null;
            for (const scheme of ["https://", "http://"]) {
                const prefix = scheme + host;
                if (value.slice(0, prefix.length).toLowerCase() !== prefix.toLowerCase())
                    continue;
                let rest = value.slice(prefix.length);
                if (rest && rest[0] !== "/")
                    continue;
                while (rest[0] === "/")
                    rest = rest.slice(1);
                if (pathPrefix) {
                    if (rest.slice(0, pathPrefix.length).toLowerCase() !== pathPrefix.toLowerCase())
                        continue;
                    rest = rest.slice(pathPrefix.length);
                }
                return base + rest;
            }
            return null;
        }

        let result = replaceHost(url, "cdn.modrinth.com", context.modrinthCdnBase);
        if (result !== null)
            return result;
        result = replaceHost(url, "resources.download.minecraft.net", context.assetsBase);
        if (result !== null)
            return result;

        for (const host of ["libraries.minecraft.net", "maven.minecraftforge.net", "maven.fabricmc.net"])
            if ((result = replaceHost(url, host, context.librariesBase)) !== null) return result;
        result = replaceHost(url, "files.minecraftforge.net", context.librariesBase, "maven/");
        if (result !== null) return result;
        result = replaceHost(url, "maven.neoforged.net", context.librariesBase, "releases/");
        if (result !== null) return result;
        result = replaceHost(url, "maven.quiltmc.org", context.librariesBase, "repository/release/");
        if (result !== null) return result;
        result = replaceHost(url, "meta.fabricmc.net", context.fabricMetaBase);
        if (result !== null) return result;
        result = replaceHost(url, "meta.quiltmc.org", context.quiltMetaBase);
        if (result !== null) return result;
        result = replaceHost(url, "dl.liteloader.com", context.librariesBase ? context.librariesBase + "com/mumfrey/liteloader/" : null, "versions/");
        if (result !== null) return result;
        result = replaceHost(url, "authlib-injector.yushi.moe", context.authlibInjectorBase);
        if (result !== null) return result;

        for (const host of ["launcher.mojang.com", "launchermeta.mojang.com", "piston-data.mojang.com", "piston-meta.mojang.com"])
            if ((result = replaceHost(url, host, context.mojangBase)) !== null) return result;
        return replaceHost(url, "s3.amazonaws.com", context.mojangBase, "Minecraft.Download/");
    }
};
