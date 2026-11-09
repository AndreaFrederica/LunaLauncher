/**
 * FastMinecraftMirror URL mapping.
 *
 * FastMinecraftMirror exposes one host per upstream service, so it cannot use
 * the single BMCLAPI-compatible directory layout. Keep this mapping in JS so
 * the mirror can evolve without adding another C++ rewrite branch.
 * See https://fastmcmirror.org/CN.html for the upstream configuration.
 */
var api = {
    metadata: {
        id: "fastmcmirror-mirror",
        kind: "mirror",
        displayName: "FastMinecraftMirror",
        version: "1.0.0",
        homepage: "https://fastmcmirror.org/CN.html",
        enabled: true,
        priority: 110
    },

    rewriteUrl(url, context) {
        if (!context || context.mirrorTypeId !== 3)
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

        const launchermeta = "https://launchermeta.fastmcmirror.org/";
        const launcher = "https://launcher.fastmcmirror.org/";
        const resources = "https://resources.fastmcmirror.org/";
        const libraries = "https://libraries.fastmcmirror.org/";
        const forge = "https://forge.fastmcmirror.org/";
        const fabric = "https://fabric.fastmcmirror.org/";
        const fabricMeta = "https://fabricmeta.fastmcmirror.org/";
        const liteloader = "https://liteloader.fastmcmirror.org/";

        let result = replaceHost(url, "launchermeta.mojang.com", launchermeta);
        if (result !== null)
            return result;
        result = replaceHost(url, "piston-meta.mojang.com", launchermeta);
        if (result !== null)
            return result;
        result = replaceHost(url, "launcher.mojang.com", launcher);
        if (result !== null)
            return result;
        result = replaceHost(url, "piston-data.mojang.com", launcher);
        if (result !== null)
            return result;
        result = replaceHost(url, "resources.download.minecraft.net", resources);
        if (result !== null)
            return result;
        result = replaceHost(url, "libraries.minecraft.net", libraries);
        if (result !== null)
            return result;
        result = replaceHost(url, "libraries.mojang.com", libraries);
        if (result !== null)
            return result;
        result = replaceHost(url, "files.minecraftforge.net", forge, "maven/");
        if (result !== null)
            return result;
        result = replaceHost(url, "maven.minecraftforge.net", forge);
        if (result !== null)
            return result;
        result = replaceHost(url, "meta.fabricmc.net", fabricMeta);
        if (result !== null)
            return result;
        result = replaceHost(url, "maven.fabricmc.net", fabric);
        if (result !== null)
            return result;
        result = replaceHost(url, "repo.mumfrey.com", liteloader);
        if (result !== null)
            return result;
        result = replaceHost(url, "dl.liteloader.com", liteloader);
        if (result !== null)
            return result;
        return replaceHost(url, "s3.amazonaws.com", launcher, "Minecraft.Download/");
    }
};
