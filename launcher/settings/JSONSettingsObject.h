/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include "settings/SettingsObject.h"
#include <QVariantMap>

/**
 * SettingsObject backed by a human-readable JSON document.
 *
 * The document has the following shape:
 *   { "formatVersion": 1, "settings": { "name": "...", ... } }
 *
 * The file belongs to the instance directory itself and is named
 * `luna.settings.json`, matching the convention used by HMCL/PCL-style
 * instance directories.
 */
class JSONSettingsObject : public SettingsObject {
    Q_OBJECT
   public:
    explicit JSONSettingsObject(QString path, QObject* parent = nullptr);

    QString filePath() const { return m_filePath; }

    bool reload() override;
    void suspendSave() override;
    void resumeSave() override;

   protected slots:
    void changeSetting(const Setting& setting, QVariant value) override;
    void resetSetting(const Setting& setting) override;

   protected:
    QVariant retrieveValue(const Setting& setting) override;

   private:
    bool loadJson();
    void doSave();
    QVariant coerceValue(const QVariant& value, const Setting& setting) const;

    QString m_filePath;
    QVariantMap m_values;
    bool m_loaded = false;
};
