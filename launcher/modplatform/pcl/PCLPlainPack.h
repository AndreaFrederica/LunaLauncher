// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>

class MinecraftInstance;

namespace PCL {

struct PlainPackCandidate {
    QString root;
    QString version;
};

struct PlainPackConversionResult {
    bool succeeded = false;
    QString error;
    QJsonArray entries;
};

QList<PlainPackCandidate> findPlainPackCandidates(const QStringList& archiveFiles);
int classFileJavaMajor(const QByteArray& data);
bool isFishModLoaderLibrary(const QString& coordinate);
PlainPackConversionResult convertPlainPack(MinecraftInstance& instance, const QString& sourceRoot, const QString& version);

}  // namespace PCL
