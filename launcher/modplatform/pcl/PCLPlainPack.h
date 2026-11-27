// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QList>
#include <QString>
#include <QStringList>

class MinecraftInstance;
class QJsonObject;

namespace PCL {

struct PlainPackCandidate {
    QString root;
    QString version;
    QString profilePath;
    QString error;
    QString details;
};

struct PlainPackConversionResult {
    bool succeeded = false;
    QString error;
    QJsonArray entries;
};

bool isVersionProfilePath(const QString& path);
QList<PlainPackCandidate> findPlainPackCandidates(const QStringList& archiveFiles,
                                                const QHash<QString, QJsonObject>& profiles = {});
void validatePlainPackCandidate(PlainPackCandidate& candidate, const QHash<QString, QJsonObject>& profiles,
                                const QSet<QString>& files, bool legacy = false);
int classFileJavaMajor(const QByteArray& data);
bool isFishModLoaderLibrary(const QString& coordinate);
PlainPackConversionResult convertPlainPack(MinecraftInstance& instance, const QString& sourceRoot, const QString& version);

}  // namespace PCL
