// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/

#pragma once

#include <QDateTime>
#include <QFileInfo>
#include <QObject>
#include <QString>

/**
 * 服务器插件类 - 独立的插件数据结构
 * 不依赖 Resource/Mod 系统
 */
class Plugin : public QObject {
    Q_OBJECT
public:
    using Ptr = std::shared_ptr<Plugin>;

    explicit Plugin(const QFileInfo& file);
    ~Plugin() override = default;

    // 基本信息
    [[nodiscard]] auto name() const -> QString { return m_name; }
    [[nodiscard]] auto version() const -> QString { return m_version; }
    [[nodiscard]] auto description() const -> QString { return m_description; }
    [[nodiscard]] auto authors() const -> QStringList { return m_authors; }

    // 文件信息
    [[nodiscard]] auto fileinfo() const -> QFileInfo { return m_file_info; }
    [[nodiscard]] auto internal_id() const -> QString { return m_internal_id; }
    [[nodiscard]] auto enabled() const -> bool { return m_enabled; }
    [[nodiscard]] auto valid() const -> bool { return m_valid; }
    [[nodiscard]] auto dateTimeChanged() const -> QDateTime { return m_changed_date_time; }
    [[nodiscard]] auto sizeStr() const -> QString;
    [[nodiscard]] auto sizeInfo() const -> qint64 { return m_size_info; }

    // 启用/禁用
    void setEnabled(bool enabled);
    bool enable();
    bool disable();

private:
    // 文件信息
    QFileInfo m_file_info;
    QString m_internal_id;
    QString m_name;
    QDateTime m_changed_date_time;
    qint64 m_size_info = 0;

    // 插件元数据（从 plugin.yml 读取）
    QString m_version;
    QString m_description;
    QStringList m_authors;

    // 状态
    bool m_enabled = true;
    bool m_valid = false;

    void parsePluginInfo();
};
