/**
 * HKMC's BMCLAPI-compatible mirror.
 *
 * Main line: https://bsmirror.hkmc.online
 * Edge line: https://mirror.hkmc.online (use Custom mode when needed).
 */
var api = {
    metadata: {
        id: "hkmc-mirror",
        kind: "mirror",
        displayName: "HKMC Mirror (Main)",
        version: "1.0.0",
        homepage: "https://bsmirror.hkmc.online",
        enabled: true,
        priority: 105
    },

    rewriteUrl(url, context) {
        if (!context || context.mirrorTypeId !== 4)
            return null;

        function replaceHost(value, host, base, pathPrefix) {
            if (!value || !base)
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
        for (const host of ["libraries.minecraft.net", "libraries.mojang.com", "maven.minecraftforge.net", "maven.fabricmc.net"])
            if ((result = replaceHost(url, host, context.librariesBase)) !== null) return result;
        result = replaceHost(url, "files.minecraftforge.net", context.librariesBase, "maven/");
        if (result !== null)
            return result;
        result = replaceHost(url, "maven.neoforged.net", context.librariesBase, "releases/");
        if (result !== null)
            return result;
        result = replaceHost(url, "maven.quiltmc.org", context.librariesBase, "repository/release/");
        if (result !== null)
            return result;
        result = replaceHost(url, "meta.fabricmc.net", context.fabricMetaBase);
        if (result !== null)
            return result;
        result = replaceHost(url, "meta.quiltmc.org", context.quiltMetaBase);
        if (result !== null)
            return result;
        result = replaceHost(url, "dl.liteloader.com", context.librariesBase ? context.librariesBase + "com/mumfrey/liteloader/" : null, "versions/");
        if (result !== null)
            return result;
        result = replaceHost(url, "authlib-injector.yushi.moe", context.authlibInjectorBase);
        if (result !== null)
            return result;
        for (const host of ["launcher.mojang.com", "launchermeta.mojang.com", "piston-data.mojang.com", "piston-meta.mojang.com"])
            if ((result = replaceHost(url, host, context.mojangBase)) !== null) return result;
        return replaceHost(url, "s3.amazonaws.com", context.mojangBase, "Minecraft.Download/");
    }
};
