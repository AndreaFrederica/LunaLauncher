/*
 *  Luna Launcher - Minecraft Launcher 
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#include "YesSteveModel.h"
#include <QFile>
#include <QRegularExpression>

YesSteveModel::YesSteveModel(QFileInfo file) : Resource(file)
{
    readMetadata();
}

void YesSteveModel::readMetadata()
{
    if (m_type != ResourceType::SINGLEFILE && m_type != ResourceType::ZIPFILE) {
        return;
    }
    QFile file(m_file_info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QString header = QString::fromUtf8(file.read(4096));
    
    QRegularExpression nameRx(R"(<name>\s*(.*))");
    auto nameMatch = nameRx.match(header);
    if (nameMatch.hasMatch()) {
        m_name = nameMatch.captured(1).trimmed();
    }

    QRegularExpression authorBlockRx(R"(<author>\s*[\r\n]+\s*<name>\s*(.*))");
    auto authorMatch = authorBlockRx.match(header);
    if (authorMatch.hasMatch()) {
        m_author = authorMatch.captured(1).trimmed();
    } else {
        QRegularExpression authorSimpleRx(R"(<author>\s*(?!<)(.*))");
        auto authorSimpleMatch = authorSimpleRx.match(header);
        if (authorSimpleMatch.hasMatch()) {
            QString val = authorSimpleMatch.captured(1).trimmed();
            if (!val.isEmpty()) {
                m_author = val;
            }
        }
    }

    QRegularExpression homepageRx(R"(<homepage>\s*(.*))");
    auto homepageMatch = homepageRx.match(header);
    if (homepageMatch.hasMatch()) {
        m_homepage = homepageMatch.captured(1).trimmed();
    } else {
        QRegularExpression urlRx(R"((https?://\S+))");
        auto urlMatch = urlRx.match(header);
        if (urlMatch.hasMatch()) {
            m_homepage = urlMatch.captured(1).trimmed();
        }
    }
}
