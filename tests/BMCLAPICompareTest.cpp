#include <QTest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSignalSpy>
#include <QDebug>

class BMCLAPICompareTest : public QObject {
    Q_OBJECT

private:
    QNetworkAccessManager *manager;

    QByteArray fetch(const QUrl &url) {
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, "PrismLauncher/Test");

        qDebug() << "Fetching:" << url.toString();
        QNetworkReply *reply = manager->get(request);

        QSignalSpy spy(reply, &QNetworkReply::finished);
        spy.wait(30000); // 30s timeout

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Network error for" << url.toString() << ":" << reply->errorString();
            delete reply;
            return QByteArray();
        }

        QByteArray data = reply->readAll();
        delete reply;
        return data;
    }

private slots:
    void initTestCase() {
        manager = new QNetworkAccessManager(this);
    }

    void compareVersionManifest() {
        QUrl mojangUrl("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");
        QUrl bmclapiUrl("https://bmclapi2.bangbang93.com/mc/game/version_manifest_v2.json");

        QByteArray mojangData = fetch(mojangUrl);
        QByteArray bmclapiData = fetch(bmclapiUrl);

        if (mojangData.isEmpty() || bmclapiData.isEmpty()) {
            QSKIP("Network request failed, skipping comparison");
        }

        QJsonDocument mojangDoc = QJsonDocument::fromJson(mojangData);
        QJsonDocument bmclapiDoc = QJsonDocument::fromJson(bmclapiData);

        QVERIFY(!mojangDoc.isNull());
        QVERIFY(!bmclapiDoc.isNull());

        QJsonObject mojangObj = mojangDoc.object();
        QJsonObject bmclapiObj = bmclapiDoc.object();

        // Compare latest versions
        QJsonObject mLatest = mojangObj["latest"].toObject();
        QJsonObject bLatest = bmclapiObj["latest"].toObject();

        qDebug() << "Mojang Latest:" << mLatest;
        qDebug() << "BMCLAPI Latest:" << bLatest;

        // Release version should definitely match
        QCOMPARE(mLatest["release"].toString(), bLatest["release"].toString());

        QJsonArray mVersions = mojangObj["versions"].toArray();
        QJsonArray bVersions = bmclapiObj["versions"].toArray();

        QVERIFY(mVersions.size() > 0);
        QVERIFY(bVersions.size() > 0);

        // Find the latest release in both lists and compare
        auto findVersion = [](const QJsonArray &arr, const QString &id) -> QJsonObject {
            for(const auto &val : arr) {
                QJsonObject obj = val.toObject();
                if(obj["id"].toString() == id) return obj;
            }
            return QJsonObject();
        };

        QString latestReleaseId = mLatest["release"].toString();
        QJsonObject mVer = findVersion(mVersions, latestReleaseId);
        QJsonObject bVer = findVersion(bVersions, latestReleaseId);

        QVERIFY(!mVer.isEmpty());
        QVERIFY(!bVer.isEmpty());

        QUrl mUrl(mVer["url"].toString());
        QUrl bUrl(bVer["url"].toString());

        qDebug() << "Mojang URL for" << latestReleaseId << ":" << mUrl.toString();
        qDebug() << "BMCLAPI URL for" << latestReleaseId << ":" << bUrl.toString();

        // Verify BMCLAPI mirrors the content exactly, preserving original URLs.
        // This means the client MUST rewrite URLs.
        QCOMPARE(bUrl.toString(), mUrl.toString());

        // Verify path is same (structure preserved)
        QCOMPARE(mUrl.path(), bUrl.path());
    }
};

QTEST_GUILESS_MAIN(BMCLAPICompareTest)
#include "BMCLAPICompareTest.moc"
