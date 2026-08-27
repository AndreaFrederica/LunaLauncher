// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonArray>
#include <QString>

#include <optional>

class UserInteraction {
   public:
    virtual ~UserInteraction() = default;

    virtual void status(const QString& message) = 0;
    virtual void deviceCode(const QString& url, const QString& code, int expiresIn) = 0;
    virtual std::optional<QString> input(const QString& prompt, bool secret) = 0;
    virtual std::optional<int> select(const QString& prompt, const QJsonArray& choices) = 0;
};
