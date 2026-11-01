// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <optional>

class UserInteraction {
   public:
    virtual ~UserInteraction() = default;

    // Alternate desktop frontends may allow the launcher to open an embedded
    // provider window while resolving downloads that have no direct URL.
    // Other interaction channels keep this disabled by default.
    virtual bool supportsEmbeddedDownloadPage() const { return false; }

    virtual void status(const QString& message) = 0;
    virtual void deviceCode(const QString& url, const QString& code, int expiresIn) = 0;
    virtual std::optional<QString> input(const QString& prompt, bool secret) = 0;
    // Optional structured context for desktop frontends; terminal clients keep
    // the same plain-text interaction and may supply a local path.
    virtual std::optional<QString> inputWithContext(const QString& prompt, const QJsonObject& context)
    {
        Q_UNUSED(context);
        return input(prompt, false);
    }
    virtual std::optional<int> select(const QString& prompt, const QJsonArray& choices) = 0;
};
