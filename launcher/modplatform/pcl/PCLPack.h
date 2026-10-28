// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QMap>
#include <QStringList>

class MinecraftInstance;

namespace PCL {

struct Setup {
    QMap<QString, QString> values;
    QByteArray sourceSha256;
};

struct InstanceConfig {
    bool valid = false;
    bool migratedJava = false;
    QString javaFolder;
    int javaMajor = -1;
    int javaMinor = -1;
    int javaBuild = -1;
    int javaRevision = -1;
    int javaMajorRevision = -1;
    int javaMinorRevision = -1;
    QByteArray sourceSha256;
    QString error;
};

struct ConversionResult {
    bool found = false;
    bool generated = false;
    QString error;
    QJsonArray entries;
};

Setup parseSetup(const QByteArray& data);
InstanceConfig parseInstanceConfig(const QByteArray& data);
int customRamMegabytes(int sliderValue);
QStringList findNestedPackCandidates(const QStringList& archiveFiles);
QStringList javaAgentPaths(const QString& arguments);
QStringList unavailableJavaAgentFiles(const QString& arguments, const QString& gameRoot);
QString pclBuiltinIconCandidate(const QString& logo);
ConversionResult convertInstanceConfig(MinecraftInstance& instance);

}  // namespace PCL
