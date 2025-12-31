#pragma once

#include <QString>
#include <QJsonValue>

class MotdRenderer
{
public:
    static QString render(const QJsonValue &motd);

private:
    static QString parseComponent(const QJsonValue &component, const QString &parentStyle = "");
    static QString mapColorToHex(const QString &colorName);
};
