// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "cli/UserInteraction.h"

class CliInteraction final : public UserInteraction {
   public:
    CliInteraction(bool nonInteractive, bool passwordStdin);

    void status(const QString& message) override;
    void deviceCode(const QString& url, const QString& code, int expiresIn) override;
    std::optional<QString> input(const QString& prompt, bool secret) override;
    std::optional<int> select(const QString& prompt, const QJsonArray& choices) override;

   private:
    QString readLine(bool secret);

    bool m_nonInteractive;
    bool m_passwordStdin;
    bool m_passwordConsumed = false;
};
