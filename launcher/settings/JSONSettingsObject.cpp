/* SPDX-License-Identifier: GPL-3.0-only */
#include "JSONSettingsObject.h"

#include "settings/Setting.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {
QJsonValue variantToJson(const QVariant& value)
{
    if (!value.isValid() || value.isNull())
        return QJsonValue(QJsonValue::Null);
    if (value.userType() == QMetaType::QStringList) {
        QJsonArray array;
        for (const auto& item : value.toStringList())
            array.append(item);
        return array;
    }
    if (value.userType() == QMetaType::QVariantList) {
        QJsonArray array;
        for (const auto& item : value.toList())
            array.append(variantToJson(item));
        return array;
    }
    if (value.userType() == QMetaType::QVariantMap) {
        QJsonObject object;
        const auto map = value.toMap();
        for (auto iter = map.constBegin(); iter != map.constEnd(); ++iter)
            object.insert(iter.key(), variantToJson(iter.value()));
        return object;
    }
    if (value.userType() == QMetaType::QByteArray) {
        return QJsonObject{ { QStringLiteral("$type"), QStringLiteral("bytearray") },
                            { QStringLiteral("value"), QString::fromLatin1(value.toByteArray().toBase64()) } };
    }
    const auto converted = QJsonValue::fromVariant(value);
    return converted.isUndefined() ? QJsonValue(value.toString()) : converted;
}

QVariant jsonToVariant(const QJsonValue& value)
{
    if (value.isObject()) {
        const auto object = value.toObject();
        if (object.value(QStringLiteral("$type")).toString() == QStringLiteral("bytearray"))
            return QByteArray::fromBase64(object.value(QStringLiteral("value")).toString().toLatin1());
        return object.toVariantMap();
    }
    if (value.isArray()) {
        QVariantList list;
        for (const auto& item : value.toArray())
            list.append(jsonToVariant(item));
        return list;
    }
    return value.toVariant();
}
}

JSONSettingsObject::JSONSettingsObject(QString path, QObject* parent)
    : SettingsObject(parent), m_filePath(std::move(path))
{
    m_loaded = loadJson();
}

bool JSONSettingsObject::loadJson()
{
    QFile file(m_filePath);
    if (!QFile::exists(m_filePath))
        return false;
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open JSON settings file" << m_filePath << file.errorString();
        return false;
    }

    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "Could not parse JSON settings file" << m_filePath << error.errorString();
        return false;
    }

    const auto root = document.object();
    const auto settings = root.value(QStringLiteral("settings"));
    if (!settings.isObject()) {
        qWarning() << "JSON settings file has no object-valued settings member" << m_filePath;
        return false;
    }
    const auto settingsObject = settings.toObject();
    for (auto iter = settingsObject.constBegin(); iter != settingsObject.constEnd(); ++iter)
        m_values.insert(iter.key(), jsonToVariant(iter.value()));
    return true;
}

bool JSONSettingsObject::reload()
{
    m_values.clear();
    m_loaded = loadJson();
    if (!m_loaded)
        return false;
    return SettingsObject::reload();
}

void JSONSettingsObject::suspendSave()
{
    m_suspendSave = true;
}

void JSONSettingsObject::resumeSave()
{
    m_suspendSave = false;
    if (m_doSave) {
        m_doSave = false;
        doSave();
    }
}

void JSONSettingsObject::changeSetting(const Setting& setting, QVariant value)
{
    if (!contains(setting.id()))
        return;

    if (value.isValid()) {
        m_values.insert(setting.id(), value);
        for (const auto& synonym : setting.configKeys()) {
            if (synonym != setting.id())
                m_values.remove(synonym);
        }
    } else {
        for (const auto& key : setting.configKeys())
            m_values.remove(key);
    }
    doSave();
}

void JSONSettingsObject::resetSetting(const Setting& setting)
{
    if (!contains(setting.id()))
        return;
    for (const auto& key : setting.configKeys())
        m_values.remove(key);
    doSave();
}

void JSONSettingsObject::doSave()
{
    if (m_suspendSave) {
        m_doSave = true;
        return;
    }

    // The instance directory already exists. QFile reports an actionable
    // error if it is unavailable or read-only.
    QJsonObject settings;
    for (auto iter = m_values.constBegin(); iter != m_values.constEnd(); ++iter)
        settings.insert(iter.key(), variantToJson(iter.value()));
    const QJsonObject root{ { QStringLiteral("formatVersion"), 1 }, { QStringLiteral("settings"), settings } };
    const auto data = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QString atomicError;
    {
        QSaveFile file(m_filePath);
        file.setDirectWriteFallback(true);
        if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit()) {
            m_loaded = true;
            return;
        }
        atomicError = file.errorString();
    }

    // External HMCL/PCL directories can reject the temporary-file rename
    // used by QSaveFile on Windows even though the existing settings file is
    // writable. Retry after the QSaveFile object has been destroyed so its
    // temporary handle is closed, preserving the file's ACL with a direct
    // truncate/write operation.
    QFile direct(m_filePath);
    QString directError;
    if (direct.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const auto written = direct.write(data);
        const bool flushed = written == data.size() && direct.flush();
        direct.close();
        if (flushed) {
            m_loaded = true;
            return;
        }
        directError = direct.errorString();
    } else {
        directError = direct.errorString();
    }
    qWarning() << "Could not write JSON settings file" << m_filePath
               << "atomic:" << atomicError << "direct:" << directError;
}

QVariant JSONSettingsObject::coerceValue(const QVariant& value, const Setting& setting) const
{
    const auto expected = setting.defValue().userType();
    if (expected == QMetaType::QStringList && value.userType() == QMetaType::QVariantList) {
        QStringList result;
        for (const auto& item : value.toList())
            result.append(item.toString());
        return result;
    }
    if (expected == QMetaType::QByteArray && value.userType() == QMetaType::QString)
        return value.toString().toUtf8();
    return value;
}

QVariant JSONSettingsObject::retrieveValue(const Setting& setting)
{
    if (!contains(setting.id()))
        return QVariant();
    for (const auto& key : setting.configKeys()) {
        const auto iter = m_values.constFind(key);
        if (iter != m_values.constEnd())
            return coerceValue(iter.value(), setting);
    }
    return QVariant();
}
