// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

#include "VanillaInstanceLayout.h"

struct ExternalLauncherEvidence {
    QString providerId;
    QString displayName;
    QString version;
    QString iconId;
    QStringList matchedPaths;
    QStringList matchedFields;
    int confidence = 0;
    bool canReadInstanceSettings = false;
    bool canEditCanonicalMetadata = false;

    QJsonObject toJson() const;
};

struct ExternalInstanceProvenance {
    QList<ExternalLauncherEvidence> providers;
    // Compatibility hint for existing consumers. It never removes providers
    // and is not used for conflict resolution.
    QString primaryProvider;
    bool mixed = false;
    QString badgeId;

    QJsonObject toJson() const;
    bool hasProvider(const QString& providerId) const;
};

struct ExternalLauncherProbeContext {
    QString root;
    VanillaInstanceLayout layout;
    QJsonObject version;
};

struct ExternalLauncherRecognizer {
    QString id;
    QString displayName;
    QString iconId;
    int priority = 0;
    bool builtIn = true;
    std::function<ExternalLauncherEvidence(const ExternalLauncherProbeContext&)> detect;
};

/** Registry shared by Classic, Neo UI and JSRT-backed recognizers. */
class ExternalLauncherRecognizerRegistry {
   public:
    static ExternalLauncherRecognizerRegistry& instance();

    void registerRecognizer(ExternalLauncherRecognizer recognizer);
    QList<ExternalLauncherRecognizer> recognizers() const;
    ExternalInstanceProvenance detect(const ExternalLauncherProbeContext& context) const;

   private:
    ExternalLauncherRecognizerRegistry();
    QList<ExternalLauncherRecognizer> m_recognizers;
};

