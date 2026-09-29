#include "AuthSession.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>
#include <QUuid>
#include "MinecraftUuid.h"

QString AuthSession::serializeUserProperties()
{
    QJsonObject userAttrs;
    /*
    for (auto key : u.properties.keys())
    {
        auto array = QJsonArray::fromStringList(u.properties.values(key));
        userAttrs.insert(key, array);
    }
    */
    QJsonDocument value(userAttrs);
    return value.toJson(QJsonDocument::Compact);
}

bool AuthSession::MakeOffline(QString offline_playername)
{
    session = "-";
    access_token = "0";
    player_name = offline_playername;
    return true;
}

bool AuthSession::useLocalYggdrasil(const QJsonObject& response, const QString& expectedUuid, const QString& apiUrl)
{
    const auto profile = response.value("selectedProfile").toObject();
    const auto expected = parseMinecraftUuid(expectedUuid);
    const QUrl endpoint(apiUrl);
    if (expected.isNull() || parseMinecraftUuid(profile.value("id").toString()) != expected || profile.value("name").toString().isEmpty() ||
        response.value("accessToken").toString().isEmpty() || response.value("metadata").toObject().isEmpty() ||
        endpoint.scheme() != "http" || endpoint.host() != "127.0.0.1")
        return false;
    uuid = expected.toString(QUuid::Id128);
    player_name = profile.value("name").toString();
    access_token = response.value("accessToken").toString();
    session = "token:" + access_token + ":" + uuid;
    user_type = "mojang";
    yggdrasil = true;
    unifiedPass = false;
    yggdrasilApiUrl = apiUrl;
    yggdrasilPrefetched =
        QString::fromLatin1(QJsonDocument(response.value("metadata").toObject()).toJson(QJsonDocument::Compact).toBase64());
    localYggdrasilFallback = true;
    status = PlayableOnline;
    auth_server_online = true;
    wants_online = true;
    demo = false;
    launchMode = LaunchMode::Normal;
    return true;
}

void AuthSession::MakeDemo(QString name, QString u)
{
    uuid = u;
    session = "-";
    access_token = "0";
    player_name = name;
    launchMode = LaunchMode::Demo;
};
