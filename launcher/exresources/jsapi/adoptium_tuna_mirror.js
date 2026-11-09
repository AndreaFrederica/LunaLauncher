/**
 * Redirect Eclipse Adoptium binaries to the TUNA mirror.
 *
 * TUNA keeps the original Adoptium artifact name and checksum, but stores
 * files under /Adoptium/<major>/<jdk|jre>/<arch>/<os>/. Returning null for
 * every other URL lets the remaining mirror rules handle it.
 */
var api = {
    metadata: {
        id: "adoptium-tuna-mirror",
        kind: "mirror",
        displayName: "TUNA Adoptium",
        version: "1.0.0",
        enabled: true,
        priority: 50
    },

    rewriteUrl(url, context) {
        if (!url || typeof url !== "string")
            return null;

        const release = url.match(/^https?:\/\/github\.com\/adoptium\/temurin([0-9]+)-binaries\/releases\/download\/[^/]+\/([^/?#]+)$/i);
        if (!release)
            return null;

        const artifact = release[2];
        const parts = artifact.match(/^OpenJDK[0-9]+U-(jdk|jre)_([a-z0-9]+)_(alpine-linux|windows|linux|mac)_/i);
        if (!parts)
            return null;

        return "https://mirrors.tuna.tsinghua.edu.cn/Adoptium/" + release[1] + "/" +
            parts[1].toLowerCase() + "/" + parts[2].toLowerCase() + "/" +
            parts[3].toLowerCase() + "/" + artifact;
    }
};
