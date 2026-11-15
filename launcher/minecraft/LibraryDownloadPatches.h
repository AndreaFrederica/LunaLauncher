// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QVector>

class LibraryDownloadPatches {
public:
    struct Rule {
        QString id, library, classifier, originalUrl, mode;
        QStringList urls;
        bool enabled = true;
    };
    static QString basePath();
    static QString overlayPath();
    static LibraryDownloadPatches load(const QString& base, const QString& overlay);
    static LibraryDownloadPatches current();
    static QJsonObject status();
    static bool openFile(bool overlay, QString& error);
    QStringList apply(const QString& library, const QString& classifier, const QString& originalUrl,
                      const QStringList& candidates, QStringList* matched = nullptr) const;
    QVector<Rule> rules;
    QStringList errors;
};
