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
#include <QStringList>

#include <memory>

#include "Resource.h"

/**
 * 服务器插件类 - 兼容 Resource 接口
 */
class Plugin : public Resource {
    Q_OBJECT
public:
    using Ptr = shared_qobject_ptr<Plugin>;

    explicit Plugin(const QFileInfo& file);
    ~Plugin() override = default;

    // 基本信息
    [[nodiscard]] auto version() const -> QString { return m_version; }
    [[nodiscard]] auto description() const -> QString { return m_description; }
    [[nodiscard]] auto website() const -> QString { return m_website; }
    [[nodiscard]] auto authors() const -> QStringList { return m_authors; }

    // 状态
    // valid(), enabled(), sizeStr() 等由 Resource 基类提供

private:
    // 插件元数据（从 plugin.yml 读取）
    QString m_version;
    QString m_description;
    QString m_website;
    QStringList m_authors;

    void parsePluginInfo();
};
