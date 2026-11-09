// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2025 TheKodeToad <TheKodeToad@proton.me>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "MirrorDownload.h"

#include "Application.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <algorithm>

#include "modplatform/jsapi/JSAPIManager.h"
#include "settings/SettingsObject.h"

namespace {

QString normalizeBase(QString base)
{
    base = base.trimmed();
    if (base.isEmpty())
        return {};
    if (!base.endsWith('/'))
        base += '/';
    return base;
}

QString joinBase(const QString& base, const QString& path)
{
    if (base.isEmpty())
        return {};
    return base + path;
}

struct ProbeScore {
    qint64 milliseconds = 0;
    qint64 measuredAt = 0;
};

QHash<QString, ProbeScore> probeScores;
QSet<QString> probesInFlight;

QString probeKey(const QString& url)
{
    const QUrl parsed(url);
    if (!parsed.isValid() || parsed.host().isEmpty())
        return {};
    return parsed.scheme().toLower() + QStringLiteral("://") + parsed.host().toLower() +
           (parsed.port() > 0 ? QStringLiteral(":%1").arg(parsed.port()) : QString());
}

void scheduleProbe(const MirrorUrlCandidate& candidate)
{
    auto* application = APPLICATION_DYN;
    if (!application)
        return;

    const bool hasExplicitProbe = !candidate.probeUrl.isEmpty();
    const auto sourceUrl = hasExplicitProbe ? candidate.probeUrl : candidate.url;
    const auto key = probeKey(sourceUrl);
    if (key.isEmpty())
        return;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (probeScores.contains(key) && now - probeScores.value(key).measuredAt < 10 * 60 * 1000)
        return;
    if (probesInFlight.contains(key))
        return;

    QUrl url(sourceUrl);
    if (!hasExplicitProbe)
        url.setPath(QStringLiteral("/"));
    url.setQuery({});
    url.setFragment({});
    if (!url.isValid())
        return;

    auto network = application->network();
    if (!network)
        return;

    probesInFlight.insert(key);
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();
    QNetworkRequest request(url);
    request.setTransferTimeout(3000);
    auto reply = network->head(request);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, key, timer]() {
        const auto elapsed = timer->elapsed();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool reachable = reply->error() == QNetworkReply::NoError || status > 0;
        probeScores.insert(key, { reachable ? elapsed : 60000, QDateTime::currentMSecsSinceEpoch() });
        probesInFlight.remove(key);
        reply->deleteLater();
    });
}

}  // namespace

const std::array<MirrorDownload::MirrorTypeInfo, 5> MirrorDownload::MirrorTypes = {
    {
        // Official - Empty strings mean use BuildConfig defaults
        {
            QT_TRANSLATE_NOOP("APIPage", "Official"),
            "",  // MetaURLOverride - use BuildConfig.META_URL
            "",  // ResourceURL - use BuildConfig.DEFAULT_RESOURCE_BASE
            "",  // LibrariesURL - use BuildConfig.LIBRARY_BASE
            ""   // FMLLibsURL - use BuildConfig.FMLLIBS_BASE_URL
        },
        // BMCLAPI - https://bmclapi2.bangbang93.com
        {
            QT_TRANSLATE_NOOP("APIPage", "BMCLAPI"),
            "https://bmclapi2.bangbang93.com/",
            "https://bmclapi2.bangbang93.com/assets/",
            "https://bmclapi2.bangbang93.com/maven/",
            "https://bmclapi2.bangbang93.com/maven/"  // FML Libraries
        },
        // Custom - User-provided URLs
        {
            QT_TRANSLATE_NOOP("APIPage", "Custom"),
            "",  // User-provided
            "",  // User-provided
            "",  // User-provided
            ""   // User-provided
        },
        // FastMinecraftMirror - https://fastmcmirror.org/CN.html
        {
            QT_TRANSLATE_NOOP("APIPage", "FastMinecraftMirror"),
            "https://launchermeta.fastmcmirror.org/",
            "https://resources.fastmcmirror.org/",
            "https://libraries.fastmcmirror.org/",
            "https://forge.fastmcmirror.org/"
        },
        // HKMC Mirror - https://bsmirror.hkmc.online (BMCLAPI-compatible)
        {
            QT_TRANSLATE_NOOP("APIPage", "HKMC Mirror (Main)"),
            "https://bsmirror.hkmc.online/",
            "https://bsmirror.hkmc.online/assets/",
            "https://bsmirror.hkmc.online/maven/",
            "https://bsmirror.hkmc.online/maven/"
        }
    }
};

MirrorDownload::ResolvedMirror MirrorDownload::resolve(SettingsObject* settings)
{
    ResolvedMirror mirror;
    if (!settings)
        return mirror;

    // Third-party mod file CDNs have their own override and must also work when
    // the Mojang downloads are served officially.
    mirror.modrinthCdnBase = normalizeBase(settings->get("ModrinthDownloadMirrorURL").toString());

    const int type = settings->get("DownloadMirrorType").toInt();
    if (type < First || type > Last)
        return mirror;

    mirror.type = static_cast<MirrorType>(type);

    if (mirror.type == Official)
        return mirror;

    if (mirror.type == BMCLAPI) {
        const QString base = normalizeBase(QStringLiteral("https://bmclapi2.bangbang93.com"));
        mirror.mojangBase = base;
        mirror.librariesBase = joinBase(base, QStringLiteral("maven/"));
        mirror.assetsBase = joinBase(base, QStringLiteral("assets/"));
        mirror.fabricMetaBase = joinBase(base, QStringLiteral("fabric-meta/"));
        mirror.quiltMetaBase = joinBase(base, QStringLiteral("quilt-meta/"));
        mirror.fmlLibsBase = mirror.librariesBase;
        mirror.authlibInjectorBase = joinBase(base, QStringLiteral("mirrors/authlib-injector/"));
        return mirror;
    }

    if (mirror.type == FastMCMirror) {
        // FastMinecraftMirror uses separate hosts for each upstream service.
        // The JS mirror provider performs the host-specific rewrite; these
        // bases also cover callers that consume assets/libraries directly.
        mirror.mojangBase = QStringLiteral("https://launcher.fastmcmirror.org/");
        mirror.librariesBase = QStringLiteral("https://libraries.fastmcmirror.org/");
        mirror.assetsBase = QStringLiteral("https://resources.fastmcmirror.org/");
        mirror.fabricMetaBase = QStringLiteral("https://fabricmeta.fastmcmirror.org/");
        mirror.fmlLibsBase = QStringLiteral("https://forge.fastmcmirror.org/");
        return mirror;
    }

    if (mirror.type == HKMC) {
        const QString base = normalizeBase(QStringLiteral("https://bsmirror.hkmc.online"));
        mirror.mojangBase = base;
        mirror.librariesBase = joinBase(base, QStringLiteral("maven/"));
        mirror.assetsBase = joinBase(base, QStringLiteral("assets/"));
        mirror.fabricMetaBase = joinBase(base, QStringLiteral("fabric-meta/"));
        mirror.quiltMetaBase = joinBase(base, QStringLiteral("quilt-meta/"));
        mirror.fmlLibsBase = mirror.librariesBase;
        mirror.authlibInjectorBase = joinBase(base, QStringLiteral("mirrors/authlib-injector/"));
        return mirror;
    }

    // Custom: every endpoint is optional. If only the Mojang downloads base is
    // configured, assume a BMCLAPI-compatible layout for the well-known subtrees
    // so a single custom API URL is enough for the common case.
    mirror.mojangBase = normalizeBase(settings->get("MojangDownloadsMirrorURL").toString());
    mirror.librariesBase = normalizeBase(settings->get("LibrariesURL").toString());
    mirror.assetsBase = normalizeBase(settings->get("ResourceURLOverride").toString());
    mirror.fmlLibsBase = normalizeBase(settings->get("LegacyFMLLibsURLOverride").toString());

    if (!mirror.mojangBase.isEmpty()) {
        if (mirror.librariesBase.isEmpty())
            mirror.librariesBase = joinBase(mirror.mojangBase, QStringLiteral("maven/"));
        if (mirror.assetsBase.isEmpty())
            mirror.assetsBase = joinBase(mirror.mojangBase, QStringLiteral("assets/"));
        mirror.fabricMetaBase = joinBase(mirror.mojangBase, QStringLiteral("fabric-meta/"));
        mirror.quiltMetaBase = joinBase(mirror.mojangBase, QStringLiteral("quilt-meta/"));
        mirror.authlibInjectorBase = joinBase(mirror.mojangBase, QStringLiteral("mirrors/authlib-injector/"));
    }

    if (mirror.fmlLibsBase.isEmpty())
        mirror.fmlLibsBase = mirror.librariesBase;

    return mirror;
}

QString MirrorDownload::rewriteUrl(const QString& url, const ResolvedMirror& mirror)
{
    const auto candidates = rewriteUrls(url, mirror);
    return candidates.isEmpty() ? url : candidates.constFirst();
}

QList<QString> MirrorDownload::rewriteUrls(const QString& url, const ResolvedMirror& mirror)
{
    QList<QString> empty;
    if (url.isEmpty())
        return empty;

    QJsonObject context{
        { QStringLiteral("mirrorType"), mirror.isOfficial() ? QStringLiteral("official") :
          (mirror.type == BMCLAPI ? QStringLiteral("bmclapi") :
           (mirror.type == FastMCMirror ? QStringLiteral("fastmcmirror") :
            (mirror.type == HKMC ? QStringLiteral("hkmc") : QStringLiteral("custom")))) },
        { QStringLiteral("mirrorTypeId"), static_cast<int>(mirror.type) },
        { QStringLiteral("mojangBase"), mirror.mojangBase },
        { QStringLiteral("librariesBase"), mirror.librariesBase },
        { QStringLiteral("assetsBase"), mirror.assetsBase },
        { QStringLiteral("fabricMetaBase"), mirror.fabricMetaBase },
        { QStringLiteral("quiltMetaBase"), mirror.quiltMetaBase },
        { QStringLiteral("fmlLibsBase"), mirror.fmlLibsBase },
        { QStringLiteral("authlibInjectorBase"), mirror.authlibInjectorBase },
        { QStringLiteral("modrinthCdnBase"), mirror.modrinthCdnBase },
    };
    auto& jsApis = JSAPIManager::instance();
    // Tests and small command-line tools can use MirrorDownload without going
    // through Application::initialize(). Load the packaged/user JS hooks on
    // first use in that case.
    if (jsApis.getAllAPIs().isEmpty())
        jsApis.initialize();
    const auto candidates = jsApis.rewriteUrls(url, context);
    if (candidates.isEmpty())
        return empty;

    struct RankedCandidate {
        MirrorUrlCandidate candidate;
        qsizetype index = 0;
        bool measured = false;
        qint64 score = 0;
    };
    QList<RankedCandidate> ranked;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    for (qsizetype i = 0; i < candidates.size(); ++i) {
        const auto& candidate = candidates.at(i);
        scheduleProbe(candidate);
        const auto key = probeKey(candidate.probeUrl.isEmpty() ? candidate.url : candidate.probeUrl);
        const auto score = probeScores.value(key);
        const bool measured = !key.isEmpty() && score.measuredAt > 0 && now - score.measuredAt < 10 * 60 * 1000;
        ranked.append({ candidate, i, measured, measured ? score.milliseconds : 0 });
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (left.measured != right.measured)
            return left.measured;
        if (left.measured && left.score != right.score)
            return left.score < right.score;
        if (left.candidate.priority != right.candidate.priority)
            return left.candidate.priority > right.candidate.priority;
        return left.index < right.index;
    });

    QList<QString> result;
    for (const auto& item : ranked)
        result.append(item.candidate.url);
    return result;
}

QString MirrorDownload::rewriteUrl(const QString& url, SettingsObject* settings)
{
    return rewriteUrl(url, resolve(settings));
}
