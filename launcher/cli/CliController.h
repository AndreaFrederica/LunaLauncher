// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>

#include "cli/CliOptions.h"

class CliController final : public QObject {
    Q_OBJECT

   public:
    explicit CliController(CliOptions options, QObject* parent = nullptr);

   public slots:
    void run();

   private:
    void printHuman(const QString& operation, const QJsonObject& result);

    CliOptions m_options;
};
