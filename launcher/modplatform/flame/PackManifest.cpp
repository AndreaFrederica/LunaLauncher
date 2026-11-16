#include "PackManifest.h"
#include "Json.h"

static void loadFileV1(Flame::File& f, QJsonObject& file)
{
    f.projectId = Json::requireInteger(file, "projectID");
    f.fileId = Json::requireInteger(file, "fileID");
    if (f.projectId <= 0)
        throw JSONValidationError("projectID must be a positive integer");
    if (f.fileId <= 0)
        throw JSONValidationError("fileID must be a positive integer");
    f.required = file["required"].toBool(true);
}

static void loadModloaderV1(Flame::Modloader& m, QJsonObject& modLoader)
{
    m.id = Json::requireString(modLoader, "id");
    m.primary = modLoader["primary"].toBool();
}

static void loadMinecraftV1(Flame::Minecraft& m, QJsonObject& minecraft)
{
    m.version = Json::requireString(minecraft, "version");
    if (m.version.trimmed().isEmpty())
        throw JSONValidationError("minecraft.version must not be empty");
    // extra libraries... apparently only used for a custom Minecraft launcher in the 1.2.5 FTB retro pack
    // intended use is likely hardcoded in the 'Flame' client, the manifest says nothing
    m.libraries = minecraft["libraries"].toString();
    auto arr = minecraft["modLoaders"].toArray();
    for (QJsonValueRef item : arr) {
        auto obj = Json::requireObject(item);
        Flame::Modloader loader;
        loadModloaderV1(loader, obj);
        m.modLoaders.append(loader);
    }
    m.recommendedRAM = minecraft["recommendedRam"].toInt();
}

static void loadManifestV1(Flame::Manifest& pack, QJsonObject& manifest)
{
    auto mc = Json::requireObject(manifest, "minecraft");

    loadMinecraftV1(pack.minecraft, mc);

    pack.name = manifest["name"].toString("Unnamed");
    pack.version = manifest["version"].toString();
    pack.author = manifest["author"].toString("Anonymous");

    auto arr = Json::requireArray(manifest, "files");
    int index = 0;
    for (auto item : arr) {
        try {
            auto obj = Json::requireObject(item);
            Flame::File file;
            loadFileV1(file, obj);
            if (pack.files.contains(file.fileId))
                throw JSONValidationError(QString("Duplicate fileID %1").arg(file.fileId));
            pack.files.insert(file.fileId, file);
        } catch (const Json::JsonException& e) {
            throw JSONValidationError(QString("files[%1]: %2").arg(index).arg(e.cause()));
        }
        ++index;
    }

    pack.overrides = manifest["overrides"].toString("overrides");

    pack.is_loaded = true;
}

void Flame::loadManifest(Flame::Manifest& m, const QString& filepath)
{
    auto doc = Json::requireDocument(filepath);
    auto obj = Json::requireObject(doc);
    m.manifestType = Json::requireString(obj, "manifestType");
    if (m.manifestType != "minecraftModpack") {
        throw JSONValidationError("Not a modpack manifest!");
    }
    m.manifestVersion = Json::requireInteger(obj, "manifestVersion");
    if (m.manifestVersion != 1) {
        throw JSONValidationError(QString("Unknown manifest version (%1)").arg(m.manifestVersion));
    }
    loadManifestV1(m, obj);
}
