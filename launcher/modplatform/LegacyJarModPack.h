// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonObject>
#include <QList>
#include <QStringList>
#include "VanillaConversion.h"

namespace LegacyJarModPack {
struct Candidate {
    QString root;
    QString version;
};

QList<Candidate> findCandidates(const QStringList& files);
QString baseVersion(const QJsonObject& profile, const QString& version, QString& error);
bool prepare(const QString& sourceRoot, const QString& version, const QJsonObject& baseProfile,
             const QString& baseJar, const VanillaConversionFeedback& feedback, QString& error);
} // namespace LegacyJarModPack
