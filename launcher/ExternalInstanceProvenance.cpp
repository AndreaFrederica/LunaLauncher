// SPDX-License-Identifier: GPL-3.0-only
#include "ExternalInstanceProvenance.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>

#include <algorithm>

namespace {

ExternalLauncherEvidence evidence(const ExternalLauncherRecognizer& recognizer)
{
    ExternalLauncherEvidence result;
    result.providerId = recognizer.id;
    result.displayName = recognizer.displayName;
    result.iconId = recognizer.iconId;
    return result;
}

void findMarkers(const QString& root, const QStringList& markers, QStringList& matches)
{
    auto current = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    for (int depth = 0; depth <= 4 && !current.isEmpty(); ++depth) {
        const QDir directory(current);
        for (const auto& marker : markers) {
            const auto path = directory.filePath(marker);
            if (QFileInfo::exists(path))
                matches.append(QDir(root).relativeFilePath(path));
        }
        const auto parent = QDir::cleanPath(directory.filePath(QStringLiteral("..")));
        if (parent == current) break;
        current = parent;
    }
}

bool hasLunaMmcMarker(const QString& root, QStringList* matchedFields = nullptr)
{
    const auto path = QDir(root).filePath(QStringLiteral("mmc-pack.json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return false;
    const auto marker = document.object().value(QStringLiteral("luna")).toObject();
    const bool generatedByLuna = marker.value(QStringLiteral("generatedBy")).toString().compare(QStringLiteral("LunaLauncher"), Qt::CaseInsensitive) == 0;
    const bool vanillaCompatibility = marker.value(QStringLiteral("format")).toString() == QStringLiteral("vanilla-compatible-mmc");
    if (generatedByLuna && vanillaCompatibility && matchedFields) {
        matchedFields->append(QStringLiteral("luna.generatedBy"));
        matchedFields->append(QStringLiteral("luna.format"));
    }
    if (generatedByLuna && vanillaCompatibility)
        return true;

    // Compatibility with Luna packs created before the explicit marker was
    // introduced. A native MMC/Prism instance has instance.cfg; a vanilla
    // profile under versions/ (or a shared vanilla root with versions/) does
    // not. Luna is currently the only MMC implementation that creates this
    // component graph for that layout.
    const auto instanceCfg = QDir(root).filePath(QStringLiteral("instance.cfg"));
    const auto versions = QDir(root).filePath(QStringLiteral("versions"));
    const auto parentIsVersions = QFileInfo(root).dir().dirName().compare(QStringLiteral("versions"), Qt::CaseInsensitive) == 0;
    const bool hasVersionDirectory = QFileInfo(versions).isDir() || parentIsVersions;
    if (!QFileInfo::exists(instanceCfg) && hasVersionDirectory) {
        if (matchedFields)
            matchedFields->append(QStringLiteral("legacy-luna-mmc-layout"));
        return true;
    }
    return false;
}

ExternalLauncherRecognizer makeHmcl()
{
    ExternalLauncherRecognizer result{ QStringLiteral("hmcl"), QStringLiteral("HMCL"), QStringLiteral("hmcl"), 0, true };
    result.detect = [result](const ExternalLauncherProbeContext& context) {
        auto found = evidence(result);
        findMarkers(context.root, { QStringLiteral("hmclversion.json"), QStringLiteral("hmclversion.cfg"), QStringLiteral(".hmcl"), QStringLiteral("hmcl") }, found.matchedPaths);
        if (context.version.contains(QStringLiteral("hmcl"))) found.matchedFields.append(QStringLiteral("hmcl"));
        found.confidence = found.matchedPaths.isEmpty() ? 0 : 75;
        if (!found.matchedFields.isEmpty()) found.confidence += 20;
        found.canReadInstanceSettings = found.confidence > 0;
        found.canEditCanonicalMetadata = found.confidence >= 75;
        return found;
    };
    return result;
}

ExternalLauncherRecognizer makePcl()
{
    ExternalLauncherRecognizer result{ QStringLiteral("pcl"), QStringLiteral("PCL"), QStringLiteral("pcl"), 0, true };
    result.detect = [result](const ExternalLauncherProbeContext& context) {
        auto found = evidence(result);
        findMarkers(context.root, { QStringLiteral("PCL"), QStringLiteral("PCL.ini"), QStringLiteral("PCL.ini.json") }, found.matchedPaths);
        if (context.version.contains(QStringLiteral("PCL"))) found.matchedFields.append(QStringLiteral("PCL"));
        found.confidence = found.matchedPaths.isEmpty() ? 0 : 75;
        if (!found.matchedFields.isEmpty()) found.confidence += 20;
        found.canReadInstanceSettings = found.confidence > 0;
        found.canEditCanonicalMetadata = found.confidence >= 75;
        return found;
    };
    return result;
}

ExternalLauncherRecognizer makeMmc()
{
    ExternalLauncherRecognizer result{ QStringLiteral("mmc"), QStringLiteral("MultiMC / Prism"), QStringLiteral("mmc"), 0, true };
    result.detect = [result](const ExternalLauncherProbeContext& context) {
        auto found = evidence(result);
        // Luna's compatibility profile is a distinct producer. Do not also
        // report it as an ambiguous generic MMC/Prism profile.
        if (hasLunaMmcMarker(context.root))
            return found;
        findMarkers(context.root, { QStringLiteral("instance.cfg"), QStringLiteral("mmc-pack.json"), QStringLiteral("patches") }, found.matchedPaths);
        found.confidence = found.matchedPaths.isEmpty() ? 0 : 85;
        found.canReadInstanceSettings = found.confidence > 0;
        found.canEditCanonicalMetadata = QFileInfo(QDir(context.root).filePath(QStringLiteral("instance.cfg"))).exists();
        return found;
    };
    return result;
}

ExternalLauncherRecognizer makeLunaMmc()
{
    ExternalLauncherRecognizer result{ QStringLiteral("luna"), QStringLiteral("Luna Launcher (MMC compatibility)"), QStringLiteral("luna"), 100, true };
    result.detect = [result](const ExternalLauncherProbeContext& context) {
        auto found = evidence(result);
        if (!hasLunaMmcMarker(context.root, &found.matchedFields))
            return found;
        found.matchedPaths.append(QStringLiteral("mmc-pack.json"));
        found.version = QStringLiteral("vanilla-compatible");
        found.confidence = 100;
        found.canReadInstanceSettings = true;
        found.canEditCanonicalMetadata = true;
        return found;
    };
    return result;
}

ExternalLauncherRecognizer makeMojang()
{
    ExternalLauncherRecognizer result{ QStringLiteral("mojang"), QStringLiteral("官方原版"), QStringLiteral("vanilla"), -100, true };
    result.detect = [result](const ExternalLauncherProbeContext&) {
        auto found = evidence(result);
        found.confidence = 50;
        found.matchedFields.append(QStringLiteral("standard-version-json"));
        return found;
    };
    return result;
}

} // namespace

QJsonObject ExternalLauncherEvidence::toJson() const
{
    return { { "providerId", providerId }, { "displayName", displayName }, { "version", version },
             { "iconId", iconId }, { "matchedPaths", QJsonArray::fromStringList(matchedPaths) },
             { "matchedFields", QJsonArray::fromStringList(matchedFields) }, { "confidence", confidence },
             { "canReadInstanceSettings", canReadInstanceSettings },
             { "canEditCanonicalMetadata", canEditCanonicalMetadata } };
}

QJsonObject ExternalInstanceProvenance::toJson() const
{
    QJsonArray providersJson;
    for (const auto& provider : providers)
        providersJson.append(provider.toJson());
    return { { "providers", providersJson }, { "primaryProvider", primaryProvider }, { "mixed", mixed }, { "badgeId", badgeId } };
}

bool ExternalInstanceProvenance::hasProvider(const QString& providerId) const
{
    return std::any_of(providers.cbegin(), providers.cend(), [&providerId](const auto& provider) {
        return provider.providerId.compare(providerId, Qt::CaseInsensitive) == 0;
    });
}

ExternalLauncherRecognizerRegistry& ExternalLauncherRecognizerRegistry::instance()
{
    static ExternalLauncherRecognizerRegistry registry;
    return registry;
}

ExternalLauncherRecognizerRegistry::ExternalLauncherRecognizerRegistry()
{
    registerRecognizer(makeHmcl());
    registerRecognizer(makePcl());
    registerRecognizer(makeLunaMmc());
    registerRecognizer(makeMmc());
    registerRecognizer(makeMojang());
}

void ExternalLauncherRecognizerRegistry::registerRecognizer(ExternalLauncherRecognizer recognizer)
{
    if (recognizer.id.trimmed().isEmpty() || !recognizer.detect)
        return;
    for (auto& existing : m_recognizers) {
        if (existing.id.compare(recognizer.id, Qt::CaseInsensitive) == 0) {
            // Packaged recognizers are authoritative for their own ID. An
            // installed script may add a new provider, but cannot silently
            // replace HMCL/PCL/MMC detection rules.
            if (existing.builtIn && !recognizer.builtIn)
                return;
            existing = std::move(recognizer);
            return;
        }
    }
    m_recognizers.append(std::move(recognizer));
}

QList<ExternalLauncherRecognizer> ExternalLauncherRecognizerRegistry::recognizers() const
{
    return m_recognizers;
}

ExternalInstanceProvenance ExternalLauncherRecognizerRegistry::detect(const ExternalLauncherProbeContext& context) const
{
    ExternalInstanceProvenance result;
    for (const auto& recognizer : m_recognizers) {
        const auto found = recognizer.detect(context);
        if (found.confidence > 0)
            result.providers.append(found);
    }
    const auto hasNonBaseline = std::any_of(result.providers.cbegin(), result.providers.cend(), [](const auto& item) {
        return item.providerId != QStringLiteral("mojang");
    });
    if (hasNonBaseline) {
        result.providers.erase(std::remove_if(result.providers.begin(), result.providers.end(), [](const auto& item) {
            return item.providerId == QStringLiteral("mojang");
        }), result.providers.end());
    }
    std::stable_sort(result.providers.begin(), result.providers.end(), [](const auto& left, const auto& right) {
        return left.confidence > right.confidence;
    });
    if (result.providers.isEmpty()) {
        auto fallback = makeMojang().detect(context);
        result.providers.append(fallback);
    }
    result.primaryProvider = result.providers.first().providerId;
    result.mixed = result.providers.size() > 1;
    result.badgeId = result.mixed ? QStringLiteral("mixed") : result.primaryProvider;
    return result;
}

