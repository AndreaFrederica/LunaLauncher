// SPDX-License-Identifier: GPL-3.0-only
#include "VanillaInstance.h"

#include "Application.h"
#include "minecraft/OneSixVersionFormat.h"
#include "minecraft/VersionFile.h"
#include "Exception.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

QStringList modernArguments(const QJsonObject& version, const QString& key)
{
    QStringList result;
    const auto array = version.value(QStringLiteral("arguments")).toObject().value(key).toArray();
    for (const auto& item : array) {
        if (item.isString()) {
            result.append(item.toString());
            continue;
        }
        // Conditional arguments are represented as {rules, value}. Keep
        // unconditional values and the common loader values; the launcher
        // itself applies the platform/account substitutions later.
        const auto object = item.toObject();
        if (!object.value(QStringLiteral("rules")).isArray()) {
            const auto value = object.value(QStringLiteral("value"));
            if (value.isString()) result.append(value.toString());
            else if (value.isArray()) {
                for (const auto& nested : value.toArray())
                    if (nested.isString()) result.append(nested.toString());
            }
        }
    }
    return result;
}

void writeJson(const QString& path, const QJsonObject& object)
{
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        const auto data = QJsonDocument(object).toJson(QJsonDocument::Indented);
        file.write(data);
        file.commit();
    }
}

QJsonObject prismPatch(QJsonObject source, const QString& uid, const QString& componentVersion, bool loader)
{
    source.insert(QStringLiteral("uid"), uid);
    source.insert(QStringLiteral("version"), componentVersion);
    source.insert(QStringLiteral("formatVersion"), 1);
    if (source.contains(QStringLiteral("arguments"))) {
        const auto game = modernArguments(source, QStringLiteral("game"));
        const auto jvm = modernArguments(source, QStringLiteral("jvm"));
        source.remove(QStringLiteral("arguments"));
        if (!game.isEmpty()) source.insert(QStringLiteral("minecraftArguments"), game.join(QLatin1Char(' ')));
        if (!jvm.isEmpty()) {
            QJsonArray values;
            for (const auto& value : jvm) values.append(value);
            source.insert(QStringLiteral("+jvmArgs"), values);
        }
    }
    if (loader) {
        // `id` belongs to the Minecraft component. Keeping it in the loader
        // patch is harmless, but removing it avoids the loader replacing the
        // base game's version metadata in older parser paths.
        source.remove(QStringLiteral("id"));
        source.remove(QStringLiteral("jar"));
        source.remove(QStringLiteral("assetIndex"));
        source.remove(QStringLiteral("assets"));
        source.remove(QStringLiteral("downloads"));
        source.remove(QStringLiteral("releaseTime"));
        source.remove(QStringLiteral("time"));
        source.remove(QStringLiteral("minimumLauncherVersion"));
    }
    return source;
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

    auto object = prismPatch(source, uid, version.trimmed(), loader);
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
    if (m_descriptor.layout.mode == VanillaGameLayout::IsolatedVersionRoot
        && QFileInfo(lookupRoot).dir().dirName() == QStringLiteral("versions"))
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
    // Keep the Prism component graph beside the foreign instance. Copying the
    // instance directory therefore carries Luna's editable settings and
    // component metadata to another Luna installation as one unit.
    const auto root = m_descriptor.root;
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
    const auto loaderSource = m_descriptor.loaderProfileOverride.isEmpty() ? sourceLoaderProfile() : m_descriptor.loaderProfileOverride;
    const bool standardMetadataOnly = hasStandardLoader
        && VanillaCompatibleProvider::isStandardLoaderProfile(rawProfile, m_descriptor.loaderType, loaderSource);
    QJsonObject baseVersion = hasStandardLoader ? sourceMinecraftVersion() : m_descriptor.version;
    // Modern PCL/HMCL profiles contain a fully merged Forge JSON. Store the
    // Minecraft metadata and loader metadata as separate Prism components so
    // PackProfile can build the same LaunchProfile as HMCL.
    if (hasStandardLoader) {
        baseVersion.remove(QStringLiteral("mainClass"));
        baseVersion.remove(QStringLiteral("arguments"));
        baseVersion.remove(QStringLiteral("minecraftArguments"));
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
        writeJson(patchPath, prismPatch(baseVersion, QStringLiteral("net.minecraft"), m_descriptor.minecraftVersion.isEmpty() ? m_descriptor.versionId : m_descriptor.minecraftVersion, false));
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
            writeJson(loaderPath, prismPatch(loaderSource, loaderUid, loaderVersion, true));
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
    m_settings->set("name", descriptor.name);
    m_settings->set("InstanceType", QStringLiteral("Vanilla"));
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

QString VanillaInstance::componentRoot() const
{
    ensureRuntimeFiles();
    return m_descriptor.root;
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

