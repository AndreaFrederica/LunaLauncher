// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/

#include "Plugin.h"
#include "FileSystem.h"

#include <QFile>
#include <QLocale>

Plugin::Plugin(const QFileInfo& file) : QObject()
{
    m_file_info = file;
    m_internal_id = file.absoluteFilePath();
    m_changed_date_time = file.lastModified();
    m_size_info = file.size();

    QString suffix = file.suffix().toLower();

    // 检查文件类型
    if (suffix == "jar") {
        m_valid = true;
        m_enabled = true;
        m_name = file.completeBaseName();
    } else if (suffix == "disabled") {
        // 检查禁用前的扩展名（例如 plugin.jar.disabled）
        QString baseName = file.completeBaseName();
        if (baseName.endsWith(".jar", Qt::CaseInsensitive)) {
            m_valid = true;
            m_enabled = false;
            // 移除 .jar 部分作为名称
            m_name = baseName.left(baseName.length() - 4);
        } else {
            m_valid = false;
            m_enabled = false;
            m_name = file.fileName();
        }
    } else {
        // 非插件文件（配置文件等）
        m_valid = false;
        m_enabled = false;
        m_name = file.fileName();
    }

    // 尝试解析 plugin.yml（未来实现）
    if (m_valid) {
        parsePluginInfo();
    }
}

QString Plugin::sizeStr() const
{
    return QLocale().formattedDataSize(m_size_info);
}

void Plugin::setEnabled(bool enabled)
{
    if (!m_valid || m_enabled == enabled)
        return;

    if (enabled)
        enable();
    else
        disable();
}

bool Plugin::enable()
{
    if (!m_valid || m_enabled)
        return false;

    QFileInfo file = fileinfo();
    if (!file.exists())
        return false;

    QString newPath = file.absolutePath() + "/" + file.completeBaseName();
    if (!FS::move(file.absoluteFilePath(), newPath))
        return false;

    m_file_info = QFileInfo(newPath);
    m_internal_id = m_file_info.absoluteFilePath();
    m_enabled = true;

    return true;
}

bool Plugin::disable()
{
    if (!m_valid || !m_enabled)
        return false;

    QFileInfo file = fileinfo();
    if (!file.exists())
        return false;

    QString newPath = file.absoluteFilePath() + ".disabled";
    if (!FS::move(file.absoluteFilePath(), newPath))
        return false;

    m_file_info = QFileInfo(newPath);
    m_internal_id = m_file_info.absoluteFilePath();
    m_enabled = false;

    return true;
}

void Plugin::parsePluginInfo()
{
    // TODO: 解析 plugin.yml 获取版本、描述、作者等信息
    // 暂时使用文件名作为名称
    m_version = "Unknown";
    m_description = "";
    m_authors.clear();
}
