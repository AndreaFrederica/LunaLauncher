// SPDX-License-Identifier: GPL-3.0-only
#include "VanillaInstance.h"

#include "Application.h"
#include "minecraft/OneSixVersionFormat.h"
#include "minecraft/VersionFile.h"
#include "minecraft/PackProfile.h"
#include "minecraft/ImportedClientProfile.h"
#include "Exception.h"
#include "settings/Setting.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {


void writeJson(const QString& path, const QJsonObject& object)
{
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        const auto data = QJsonDocument(object).toJson(QJsonDocument::Indented);
        file.write(data);
        file.commit();
    }
}


QString standardComponentName(const QString& uid)
{
    if (uid == QStringLiteral("net.minecraft")) return QStringLiteral("Minecraft");
    if (uid == QStringLiteral("net.minecraftforge")) return QStringLiteral("Minecraft Forge");
    if (uid == QStringLiteral("net.neoforged")) return QStringLiteral("NeoForge");
    if (uid == QStringLiteral("net.fabricmc.fabric-loader")) return QStringLiteral("Fabric Loader");
    if (uid == QStringLiteral("org.quiltmc.quilt-loader")) return QStringLiteral("Quilt Loader");
    return uid;
}

std::shared_ptr<VersionFile> parseForeignComponent(const QJsonObject& source,
                                                   const QString& uid,
                                                   const QString& version,
                                                   bool loader,
                                                   const QString& filename)
{
    if (source.isEmpty() || uid.isEmpty() || version.trimmed().isEmpty())
        return {};

    auto object = VanillaCompatibleProvider::prismRuntimePatch(source, uid, version.trimmed(), loader);
    try {
        return OneSixVersionFormat::versionFileFromJson(QJsonDocument(object), filename, false);
    } catch (const Exception& error) {
        qWarning() << "Could not parse foreign component" << uid << error.cause();
        return {};
    }
}

}  // namespace

QString VanillaInstance::standardLoaderUid() const
{
    if (m_descriptor.loaderType == "fishmodloader") return {};
    return VanillaCompatibleProvider::loaderUidForType(m_descriptor.loaderType);
}

std::shared_ptr<VersionFile> VanillaInstance::loadForeignComponent(const QString& uid, const QString& version) const
{
    // Read the authoritative HMCL/PCL/Mojang profile each time.  This keeps
    // the launch graph in sync when another launcher edits the instance and
    // avoids generating a Prism patch file solely to satisfy metadata lookup.
    QFile rawFile(m_descriptor.layout.versionJson);
    if (!rawFile.open(QIODevice::ReadOnly))
        return {};

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(rawFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return {};
    const auto raw = document.object();

    if (uid == QStringLiteral("net.minecraft")) {
        auto source = sourceMinecraftVersion();
        if (!VanillaCompatibleProvider::isStandardLoaderProfile(raw,m_descriptor.loaderType,sourceLoaderProfile())) source = m_descriptor.version;
        source.insert("id",version);
        if (QFileInfo(m_descriptor.layout.clientJar).isFile() && !source.contains("mainJar")
            && !source.value("downloads").toObject().contains("client"))
            source.insert("mainJar",QJsonObject{{"name","com.mojang:minecraft:"+version+":client"}});
        return parseForeignComponent(source, uid, version, false, m_descriptor.layout.versionJson);
    }

    if (uid != standardLoaderUid())
        return {};

    const auto source = m_descriptor.loaderProfileOverride.isEmpty() ? sourceLoaderProfile() : m_descriptor.loaderProfileOverride;
    if (source.isEmpty())
        return {};

    return parseForeignComponent(source, uid, version, true, m_descriptor.layout.versionJson);
}

QJsonObject VanillaInstance::sourceMinecraftVersion() const
{
    const auto minecraftVersion = m_descriptor.minecraftVersion.isEmpty() ? m_descriptor.versionId : m_descriptor.minecraftVersion;
    QFile rawFile(m_descriptor.layout.versionJson);
    if (rawFile.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(rawFile.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const auto patches = document.object().value(QStringLiteral("patches")).toArray();
            for (const auto& value : patches) {
                const auto patch = value.toObject();
                if (patch.value(QStringLiteral("id")).toString() == QStringLiteral("game")
                    || patch.value(QStringLiteral("uid")).toString() == QStringLiteral("net.minecraft"))
                    return patch;
            }
        }
    }
    auto lookupRoot = m_descriptor.layout.sourceRoot;
    if (QFileInfo(lookupRoot).dir().dirName() == QStringLiteral("versions"))
        lookupRoot = QDir::cleanPath(QDir(lookupRoot).filePath(QStringLiteral("../..")));
    auto result = loadVanillaVersionJson(lookupRoot, minecraftVersion);
    return result.isEmpty() ? m_descriptor.version : result;
}

QJsonObject VanillaInstance::sourceLoaderProfile() const
{
    QFile rawFile(m_descriptor.layout.versionJson);
    if (rawFile.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(rawFile.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const auto patches = document.object().value(QStringLiteral("patches")).toArray();
            for (const auto& value : patches) {
                const auto patch = value.toObject();
                if (VanillaCompatibleProvider::detectedLoaderType(patch) == m_descriptor.loaderType)
                    return patch;
            }
        }
    }
    if (!m_descriptor.loaderProfileOverride.isEmpty())
        return m_descriptor.loaderProfileOverride;
    return m_descriptor.version;
}

void VanillaInstance::ensureRuntimeFiles() const
{
    if (m_runtimeReady)
        return;
    // The MMC graph is an implementation detail. Never materialize it in an
    // external vanilla instance, including when scanning or saving settings.
    if (!m_runtimeDirectory.isValid())
        return;
    const auto root = m_runtimeDirectory.path();
    const auto patchesRoot = QDir(root).filePath(QStringLiteral("patches"));
    const auto componentsPath = QDir(root).filePath(QStringLiteral("mmc-pack.json"));
    const auto patchPath = QDir(patchesRoot).filePath(QStringLiteral("net.minecraft.json"));
    // Keep the original vanilla client jar in versions/<id>/<id>.jar. Do not
    // mirror it into .minecraft/libraries: the generated Prism profile may
    // already resolve the same client jar through the launcher cache, and
    // Forge/NeoForge's module layer then sees two different JAR paths with
    // the same JPMS module name (minecraft), causing ResolutionException.
    // The normal library update path still downloads/resolves the main jar
    // when the launcher cache does not contain it.
    const auto loaderUid = standardLoaderUid();
    const auto loaderVersion = m_descriptor.loaderVersionOverride.isEmpty() ? m_descriptor.loaderVersion : m_descriptor.loaderVersionOverride;
    const bool hasStandardLoader = !loaderUid.isEmpty() && !loaderVersion.isEmpty();
    const bool hasLoader = hasStandardLoader && m_descriptor.loaderEnabled;
    QJsonObject rawProfile;
    QFile rawProfileFile(m_descriptor.layout.versionJson);
    if (rawProfileFile.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(rawProfileFile.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject())
            rawProfile = document.object();
    }
    auto loaderSource = m_descriptor.loaderProfileOverride.isEmpty() ? sourceLoaderProfile() : m_descriptor.loaderProfileOverride;
    const bool standardMetadataOnly = hasStandardLoader
        && VanillaCompatibleProvider::isStandardLoaderProfile(rawProfile, m_descriptor.loaderType, loaderSource);
    QJsonObject baseVersion = hasStandardLoader ? sourceMinecraftVersion() : m_descriptor.version;
    // Modern PCL/HMCL profiles contain a fully merged Forge JSON. Store the
    // Minecraft metadata and loader metadata as separate Prism components so
    // PackProfile can build the same LaunchProfile as HMCL.
    if (hasStandardLoader && standardMetadataOnly) {
        baseVersion.remove(QStringLiteral("mainClass"));
        baseVersion.remove(QStringLiteral("arguments"));
        baseVersion.remove(QStringLiteral("minecraftArguments"));
    }
    if (hasStandardLoader && !standardMetadataOnly && m_descriptor.loaderProfileOverride.isEmpty()) {
        // Preserve custom merged runtimes (HMCL bootstrap/RFB) as one unit.
        baseVersion = m_descriptor.version;
        loaderSource = QJsonObject{{"name", m_descriptor.loaderType}};
    }
    if (!standardMetadataOnly && QFileInfo(m_descriptor.layout.clientJar).isFile() && !baseVersion.contains("mainJar")
        && !baseVersion.value("downloads").toObject().contains("client")) {
        baseVersion.insert("mainJar",QJsonObject{{"name","com.mojang:minecraft:"+m_descriptor.minecraftVersion+":client"}});
    }
    if (standardMetadataOnly) {
        // A normal HMCL/PCL loader profile is already represented by Prism's
        // metadata index. Remove stale generated patches from older Luna
        // builds so the components are displayed without the Custom label.
        QFile::remove(patchPath);
        for (const auto& uid : { QStringLiteral("net.minecraftforge"), QStringLiteral("net.neoforged"),
                                 QStringLiteral("net.fabricmc.fabric-loader"), QStringLiteral("org.quiltmc.quilt-loader") })
            QFile::remove(QDir(patchesRoot).filePath(QStringLiteral("%1.json").arg(uid)));
        // HMCL/PCL standard profiles keep their authoritative version JSON in
        // versions/. Prism patch files are only needed for non-standard or
        // custom profiles. Remove the generated directory when it is empty so
        // opening a standard external instance does not alter its layout.
        QDir(root).rmdir(QStringLiteral("patches"));
    } else {
        QDir().mkpath(patchesRoot);
        writeJson(patchPath, VanillaCompatibleProvider::prismRuntimePatch(baseVersion, QStringLiteral("net.minecraft"), m_descriptor.minecraftVersion.isEmpty() ? m_descriptor.versionId : m_descriptor.minecraftVersion, false));
    }

    QJsonArray components;
    const auto minecraftVersion = m_descriptor.minecraftVersion.isEmpty() ? m_descriptor.versionId : m_descriptor.minecraftVersion;
    components.append(QJsonObject{ { "uid", "net.minecraft" },
                                   { "version", minecraftVersion },
                                   { "cachedName", standardComponentName(QStringLiteral("net.minecraft")) },
                                   { "cachedVersion", minecraftVersion },
                                   { "important", true } });
    if (hasStandardLoader) {
        const auto loaderPath = QDir(patchesRoot).filePath(QStringLiteral("%1.json").arg(loaderUid));
        if (standardMetadataOnly)
            QFile::remove(loaderPath);
        else
            writeJson(loaderPath, VanillaCompatibleProvider::prismRuntimePatch(loaderSource, loaderUid, loaderVersion, true));
        components.append(QJsonObject{ { "uid", loaderUid },
                                       { "version", loaderVersion },
                                       { "cachedName", standardComponentName(loaderUid) },
                                       { "cachedVersion", loaderVersion },
                                       { "disabled", !hasLoader } });
    }
    // This is deliberately an additive, Prism-ignored field. It lets Luna
    // distinguish its vanilla-compatible MMC bridge from a native Prism/MMC
    // instance when the directory is inspected by another Luna process.
    writeJson(componentsPath,
              QJsonObject{ { "formatVersion", 1 },
                           { "components", components },
                           { "luna", QJsonObject{ { "generatedBy", "LunaLauncher" },
                                                  { "format", "vanilla-compatible-mmc" },
                                                  { "version", 1 } } } });
    m_runtimeReady = true;
}

VanillaInstance::VanillaInstance(SettingsObject* globalSettings,
                                 std::unique_ptr<SettingsObject> settings,
                                 const VanillaInstanceDescriptor& descriptor)
    : MinecraftInstance(globalSettings, std::move(settings), descriptor.root), m_descriptor(descriptor)
{
    m_settings->registerSetting("VanillaGameDirectoryMode", QStringLiteral("auto"));
    m_settings->set("name", descriptor.name);
    m_settings->set("InstanceType", QStringLiteral("Vanilla"));
    const auto invalidateDirectories = [this](const Setting& setting) {
        if (setting.id() != "VanillaGameDirectoryMode") return;
        m_loader_mod_list.reset(); m_core_mod_list.reset(); m_nil_mod_list.reset();
        m_resource_pack_list.reset(); m_shader_pack_list.reset(); m_texture_pack_list.reset();
        m_yes_steve_model_list.reset(); m_custom_player_model_list.reset();
        m_schematics_list.reset(); m_data_pack_list.reset(); m_world_list.reset();
    };
    connect(m_settings.get(), &SettingsObject::SettingChanged, this,
            [invalidateDirectories](const Setting& setting, QVariant) { invalidateDirectories(setting); });
    connect(m_settings.get(), &SettingsObject::settingReset, this, invalidateDirectories);
    setVersionBroken(false);
}

std::optional<MinecraftClientRoute> VanillaInstance::clientArtifactRoute() const
{
    const auto clientJar = QDir::cleanPath(m_descriptor.layout.clientJar);
    if (clientJar.isEmpty() || !QFileInfo::exists(clientJar))
        return std::nullopt;

    const auto minecraftVersion = m_descriptor.minecraftVersion.isEmpty() ? m_descriptor.versionId : m_descriptor.minecraftVersion;
    const auto loaderType = (m_descriptor.loaderTypeOverride.isEmpty() ? m_descriptor.loaderType : m_descriptor.loaderTypeOverride).trimmed().toLower();
    return MinecraftClientRoute::external(clientJar, minecraftVersion, loaderType);
}

bool VanillaInstance::validateClientArtifact(QString* error) const
{
    QFile artifact(QDir(instanceRoot()).filePath("luna-imported/client-artifact.json"));
    if (artifact.exists()) {
        if (!artifact.open(QIODevice::ReadOnly)) {
            if (error) *error = tr("Could not read the preserved client verification record.");
            return false;
        }
        const auto record = QJsonDocument::fromJson(artifact.readAll()).object();
        return ImportedClientProfile::verifyClient(m_descriptor.layout.clientJar,record,error);
    }
    QFile sidecar(QDir(instanceRoot()).filePath("luna.settings.json"));
    const auto record = sidecar.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(sidecar.readAll()).object().value("clientArtifact").toObject() : QJsonObject{};
    if (record.isEmpty()) return true;
    return ImportedClientProfile::verifyClient(m_descriptor.layout.clientJar,record,error);
}

VanillaInstance::~VanillaInstance()
{
    // Flush while virtual dispatch and private storage still belong to this
    // adapter. The base profile destructor must not save into instanceRoot().
    m_components->saveNow();
    m_components.reset();
}

QString VanillaInstance::componentRoot() const
{
    ensureRuntimeFiles();
    return m_runtimeDirectory.path();
}

bool VanillaInstance::canLaunch() const
{
    if (!QFileInfo::exists(m_descriptor.layout.versionJson))
        return false;

    // HMCL/PCL repair missing client and library files on the next online
    // launch.  MinecraftInstance::createLaunchTask() already schedules the
    // normal LibrariesTask, which validates the local cache and downloads
    // missing artifacts (including the Mojang client jar) from the profile.
    // Do not disable the launch action merely because that repair has not run
    // yet; only profiles without a client jar and without a usable download
    // URL are genuinely unlaunchable.
    if (QFileInfo::exists(m_descriptor.layout.clientJar))
        return true;

    const auto client = m_descriptor.version.value(QStringLiteral("downloads"))
                            .toObject()
                            .value(QStringLiteral("client"))
                            .toObject();
    return !client.value(QStringLiteral("url")).toString().trimmed().isEmpty();
}

bool VanillaInstance::materializeForeignMavenFiles(QString* error) const
{
    ensureRuntimeFiles();
    const auto components = getPackProfile();
    if (!components || !components->getProfile()) {
        if (error) *error = tr("The external loader profile is not available.");
        return false;
    }
    const auto profile = components->getProfile();

    const auto targetRoot = m_descriptor.layout.librariesRoot;
    if (targetRoot.trimmed().isEmpty()) {
        if (error) *error = tr("The external instance has no libraries directory.");
        return false;
    }
    QDir().mkpath(targetRoot);

    // LibrariesTask downloads into Luna's normal cache. Resolve the same
    // paths with no override so we can copy only the Forge/NeoForge installer
    // side of the profile into the portable HMCL/PCL library tree.
    QStringList jars, natives;
    for (const auto& library : profile->getMavenFiles()) {
        if (!library)
            continue;
        QStringList native32, native64;
        library->getApplicableFiles(runtimeContext(), jars, natives, native32, native64, {});
    }

    const auto globalRoot = QDir::current().absoluteFilePath(QStringLiteral("libraries"));
    const auto globalDir = QDir(globalRoot);
    for (const auto& sourcePath : jars) {
        const auto source = QFileInfo(sourcePath).absoluteFilePath();
        if (!QFileInfo(source).isFile())
            continue;
        auto relative = globalDir.relativeFilePath(source);
        if (relative.startsWith(QStringLiteral("..")))
            continue;
        const auto destination = QDir(targetRoot).filePath(QDir::fromNativeSeparators(relative));
        if (QFileInfo::exists(destination))
            continue;
        if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(source, destination)) {
            if (error) *error = tr("Could not copy the external loader dependency %1.").arg(destination);
            return false;
        }
    }
    return true;
}

bool VanillaInstance::setForeignLoaderEnabled(bool enabled, QString* error)
{
    const auto version = m_descriptor.loaderVersionOverride.isEmpty() ? m_descriptor.loaderVersion : m_descriptor.loaderVersionOverride;
    if (standardLoaderUid().isEmpty() || version.trimmed().isEmpty()) {
        if (error) *error = tr("This instance does not contain a supported standard mod loader.");
        return false;
    }
    if (m_descriptor.loaderTypeOverride.isEmpty())
        m_descriptor.loaderTypeOverride = m_descriptor.loaderType;
    if (m_descriptor.loaderVersionOverride.isEmpty())
        m_descriptor.loaderVersionOverride = version.trimmed();
    if (m_descriptor.loaderProfileOverride.isEmpty())
        m_descriptor.loaderProfileOverride = sourceLoaderProfile();
    if (!VanillaCompatibleProvider::writeLoaderManifest(m_descriptor.layout.versionJson,
                                                        m_descriptor.loaderType,
                                                        version,
                                                        m_descriptor.loaderProfileOverride,
                                                        sourceMinecraftVersion(),
                                                        enabled,
                                                        error))
        return false;
    m_descriptor.loaderEnabled = enabled;
    m_descriptor.loaderManifestFingerprint = foreignLoaderManifestFingerprint();
    m_descriptor.lunaSettingsManaged = true;
    m_descriptor.lunaLoaderManaged = true;
    if (!VanillaCompatibleProvider::saveSidecar(m_descriptor, error))
        return false;
    m_runtimeReady = false;
    return true;
}

bool VanillaInstance::setForeignLoaderProfile(const QString& version, const QJsonObject& profile, QString* error)
{
    if (standardLoaderUid().isEmpty() || version.trimmed().isEmpty() || profile.isEmpty()) {
        if (error) *error = tr("A supported loader version profile is required.");
        return false;
    }
    if (m_descriptor.loaderTypeOverride.isEmpty())
        m_descriptor.loaderTypeOverride = m_descriptor.loaderType;
    if (!VanillaCompatibleProvider::writeLoaderManifest(m_descriptor.layout.versionJson,
                                                        m_descriptor.loaderType,
                                                        version.trimmed(),
                                                        profile,
                                                        sourceMinecraftVersion(),
                                                        true,
                                                        error))
        return false;
    m_descriptor.loaderVersionOverride = version.trimmed();
    m_descriptor.loaderProfileOverride = profile;
    m_descriptor.loaderEnabled = true;
    m_descriptor.loaderManifestFingerprint = foreignLoaderManifestFingerprint();
    m_descriptor.lunaSettingsManaged = true;
    m_descriptor.lunaLoaderManaged = true;
    if (!VanillaCompatibleProvider::saveSidecar(m_descriptor, error))
        return false;
    m_runtimeReady = false;
    return true;
}

QString VanillaInstance::foreignLoaderManifestFingerprint() const
{
    QFile file(m_descriptor.layout.versionJson);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex());
}


QString VanillaInstance::gameRoot() const
{
    const auto mode = m_settings->get("VanillaGameDirectoryMode").toString();
    if (mode == "shared") return QDir::cleanPath(QDir(m_descriptor.layout.versionsRoot).absoluteFilePath(".."));
    if (mode == "isolated") return QFileInfo(m_descriptor.layout.versionJson).absolutePath();
    return detectVanillaInstanceLayout(m_descriptor.root, m_descriptor.versionId).gameRoot;
}
