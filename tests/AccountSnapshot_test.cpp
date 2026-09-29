// SPDX-License-Identifier: GPL-3.0-only
#include <QJsonDocument>
#include <QTest>
#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AuthSession.h"

class AccountSnapshotTest : public QObject {
    Q_OBJECT
   private slots:
    void snapshotsNeverContainCredentials()
    {
        AccountData data;
        data.minecraftProfile.id = "123456789abc4def8123456789abcdef";
        data.minecraftProfile.name = "Alice";
        data.minecraftProfile.skin.url = "https://textures.minecraft.net/texture/one";
        data.minecraftProfile.skin.data = "saved-skin";
        data.minecraftProfile.validity = Validity::Certain;
        data.minecraftEntitlement = { true, true, Validity::Certain };
        data.officialVerifiedAt = QDateTime::currentDateTimeUtc();
        data.msaToken.token = "secret-ms-token";
        data.msaToken.refresh_token = "secret-refresh-token";
        data.yggdrasilToken.token = "secret-mc-token";
        const auto snapshot = data.officialSnapshot();
        QVERIFY(!snapshot.isEmpty());
        QVERIFY(!QJsonDocument(snapshot).toJson().contains("secret-"));
        AccountData reloaded;
        QVERIFY(reloaded.resumeStateFromV3(data.saveState()));
        QCOMPARE(reloaded.officialVerifiedAt.toSecsSinceEpoch(), data.officialVerifiedAt.toSecsSinceEpoch());
        QVERIFY(reloaded.officialSnapshot().isEmpty());  // Disk data is assumed, not newly verified.
        reloaded.minecraftProfile.skin.data.clear();
        reloaded.accountState = AccountState::Offline;
        reloaded.restoreOfficialSnapshot(snapshot);
        QCOMPARE(reloaded.minecraftProfile.skin.data, QByteArray("saved-skin"));
        QCOMPARE(reloaded.accountState, AccountState::Offline);
        QCOMPARE(reloaded.minecraftEntitlement.validity, Validity::Assumed);
        QCOMPARE(reloaded.yggdrasilToken.token, QString("secret-mc-token"));
        reloaded.minecraftProfile.skin.data.clear();
        reloaded.minecraftProfile.skin.url += "-changed";
        reloaded.restoreOfficialSnapshot(snapshot);
        QVERIFY(reloaded.minecraftProfile.skin.data.isEmpty());
        reloaded.minecraftProfile.skin.url = data.minecraftProfile.skin.url;
        reloaded.minecraftProfile.id = "aaaaaaaaaaaa4aaa8aaaaaaaaaaaaaaa";
        reloaded.restoreOfficialSnapshot(snapshot);
        QVERIFY(reloaded.minecraftProfile.skin.data.isEmpty());
    }

    void localSessionKeepsOfficialIdentityAndOnlineGameMode()
    {
        const QString uuid = "123456789abc4def8123456789abcdef";
        QJsonObject response{ { "selectedProfile", QJsonObject{ { "id", uuid }, { "name", "Alice" } } },
                              { "accessToken", "local-token" },
                              { "metadata", QJsonObject{ { "signaturePublickey", "public" } } } };
        AuthSession session;
        session.uuid = uuid;
        session.access_token = "official-token";
        QVERIFY(!session.useLocalYggdrasil(response, "aaaaaaaaaaaa4aaa8aaaaaaaaaaaaaaa", "http://127.0.0.1:12345"));
        QCOMPARE(session.access_token, QString("official-token"));
        QVERIFY(session.useLocalYggdrasil(response, uuid, "http://127.0.0.1:12345"));
        QCOMPARE(session.uuid, uuid);
        QCOMPARE(session.player_name, QString("Alice"));
        QCOMPARE(session.access_token, QString("local-token"));
        QCOMPARE(session.launchMode, LaunchMode::Normal);
        QCOMPARE(session.status, AuthSession::PlayableOnline);
        QVERIFY(session.yggdrasil && session.localYggdrasilFallback && session.wants_online);
        QVERIFY(!session.yggdrasilPrefetched.isEmpty());
        QVERIFY(session.useLocalYggdrasil(response, "12345678-9abc-4def-8123-456789abcdef", "http://127.0.0.1:12345"));
        QCOMPARE(session.uuid, uuid);
    }

    void thirdPartySnapshotsKeepProfileTexturesWithoutCredentials()
    {
        AccountData data;
        data.type = AccountType::Yggdrasil;
        data.minecraftProfile.id = "123456789abc4def8123456789abcdef";
        data.minecraftProfile.name = "ThirdParty";
        data.minecraftProfile.validity = Validity::Certain;
        data.minecraftProfile.skin.url = "https://textures.example/skin";
        data.minecraftProfile.skin.variant = "slim";
        data.minecraftProfile.skin.data = "skin-bytes";
        Cape cape;
        cape.id = "cape";
        cape.alias = "cape";
        cape.url = "https://textures.example/cape";
        cape.data = "cape-bytes";
        data.minecraftProfile.capes.insert(cape.id, cape);
        data.minecraftProfile.currentCape = cape.id;
        data.yggdrasilToken.extra["credentials"] = "provider-password";
        data.yggdrasilToken.token = "provider-token";

        const auto snapshot = data.cachedSnapshot();
        QVERIFY(!snapshot.isEmpty());
        const auto encoded = QJsonDocument(snapshot).toJson();
        QVERIFY(encoded.contains("cape-bytes"));
        QVERIFY(!encoded.contains("provider-password"));
        QVERIFY(!encoded.contains("provider-token"));

        AccountData restored;
        restored.type = AccountType::Yggdrasil;
        restored.minecraftProfile.id = data.minecraftProfile.id;
        restored.minecraftProfile.name = data.minecraftProfile.name;
        restored.minecraftProfile.validity = Validity::Certain;
        restored.minecraftProfile.skin.url = data.minecraftProfile.skin.url;
        restored.minecraftProfile.capes.insert(cape.id, Cape{ cape.id, cape.url, cape.alias, {} });
        restored.restoreCachedSnapshot(snapshot);
        QCOMPARE(restored.minecraftProfile.skin.data, QByteArray("skin-bytes"));
        QCOMPARE(restored.minecraftProfile.capes.value(cape.id).data, QByteArray("cape-bytes"));
        QCOMPARE(restored.cacheState, CacheState::Cached);
    }
};

QTEST_GUILESS_MAIN(AccountSnapshotTest)
#include "AccountSnapshot_test.moc"
