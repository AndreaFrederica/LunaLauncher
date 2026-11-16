// SPDX-License-Identifier: GPL-3.0-only
/*
 *  LunaLauncher - Minecraft Launcher
 *
 *  Tests for the download integrity plumbing and the cdn.modrinth.com
 *  mirror override that the blocked-mod Modrinth fallback relies on.
 */

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "minecraft/MirrorDownload.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"

class DownloadIntegrityTest : public QObject {
    Q_OBJECT

   private slots:
    void artifactCacheIsOptInAndTemporaryFilesAreNotIndexed()
    {
        QTemporaryDir data;
        QVERIFY(data.isValid());
        QString temporary;
        {
            HttpMetaCache cache(data.filePath("index.json"));
            cache.addBase("ATLauncherPacks", data.filePath("downloads"));
            auto entry = cache.resolveEntry("ATLauncherPacks", "fixture.jar");
            temporary = entry->getFullPath();
            QVERIFY(!temporary.startsWith(data.path()));
            QVERIFY(QDir().mkpath(QFileInfo(temporary).absolutePath()));
            QFile file(temporary);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("installed payload");
            file.close();
            entry->setStale(false);
            QVERIFY(cache.updateEntry(entry));
            QCOMPARE(cache.entryCount(), 0);
            cache.SaveNow();
            QVERIFY(!cache.getEntry("ATLauncherPacks", "fixture.jar"));
            QVERIFY(QFileInfo::exists(temporary)); // Still available for install copy/extract.
        }
        QVERIFY(!QFileInfo::exists(temporary));
        QVERIFY(!QFileInfo::exists(data.filePath("downloads/fixture.jar")));
    }

    void enabledArtifactCacheReusesFilesAndKeepsRuntimeSeparate()
    {
        QTemporaryDir data;
        QVERIFY(data.isValid());
        HttpMetaCache cache(data.filePath("index.json"));
        cache.addBase("ATLauncherPacks", data.filePath("downloads"));
        cache.addBase("libraries", data.filePath("libraries"));
        cache.addBase("meta", data.filePath("meta"));
        cache.setCacheDownloadArtifacts(true);
        for (const auto& base : { QString("ATLauncherPacks"), QString("libraries"), QString("meta") }) {
            const auto name = base == "meta" ? "index.json" : "fixture.jar";
            auto entry = cache.resolveEntry(base, name);
            const auto path = entry->getFullPath();
            QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("payload"); file.close();
            entry->setMD5Sum(QCryptographicHash::hash("payload", QCryptographicHash::Md5).toHex());
            entry->setLocalChangedTimestamp(QFileInfo(path).lastModified().toMSecsSinceEpoch());
            entry->makeEternal(true);
            entry->setStale(false);
            QVERIFY(cache.updateEntry(entry));
        }
        QCOMPARE(cache.entryCount("metadata"), 1);
        QCOMPARE(cache.entryCount("artifacts"), 1);
        QCOMPARE(cache.entryCount("runtime"), 1);
        QVERIFY(!cache.resolveEntry("ATLauncherPacks", "fixture.jar")->isStale());
        cache.setCacheDownloadArtifacts(false);
        QVERIFY(cache.resolveEntry("ATLauncherPacks", "fixture.jar")->getFullPath() != data.filePath("downloads/fixture.jar"));
        QVERIFY(cache.resolveEntry("libraries", "fixture.jar")->getFullPath() == data.filePath("libraries/fixture.jar"));
        QVERIFY(cache.evictKind("metadata"));
        QVERIFY(QFileInfo::exists(data.filePath("downloads/fixture.jar")));
        QVERIFY(QFileInfo::exists(data.filePath("libraries/fixture.jar")));
        QVERIFY(cache.evictKind("artifacts"));
        QVERIFY(!QFileInfo::exists(data.filePath("downloads/fixture.jar")));
        QVERIFY(QFileInfo::exists(data.filePath("libraries/fixture.jar")));
    }

    void rewritesModrinthCdn()
    {
        MirrorDownload::ResolvedMirror mirror;
        mirror.modrinthCdnBase = QStringLiteral("https://mirror.example.com/");

        const QString url = QStringLiteral("https://cdn.modrinth.com/data/LUwpuw0l/versions/ZXbjpM6f/YARCF-0.14%281.12.2%29.jar");
        QCOMPARE(MirrorDownload::rewriteUrl(url, mirror),
                 QStringLiteral("https://mirror.example.com/data/LUwpuw0l/versions/ZXbjpM6f/YARCF-0.14%281.12.2%29.jar"));
    }

    void leavesCurseForgeCdnAlone()
    {
        MirrorDownload::ResolvedMirror mirror;
        mirror.modrinthCdnBase = QStringLiteral("https://mirror.example.com/");

        const QString url = QStringLiteral("https://mediafilez.forgecdn.net/files/3310/627/YARCF-0.14.jar");
        QCOMPARE(MirrorDownload::rewriteUrl(url, mirror), url);
    }

    void doesNotMatchLookalikeHost()
    {
        MirrorDownload::ResolvedMirror mirror;
        mirror.modrinthCdnBase = QStringLiteral("https://mirror.example.com/");

        const QString url = QStringLiteral("https://cdn.modrinth.com.evil.example/data/x.jar");
        QCOMPARE(MirrorDownload::rewriteUrl(url, mirror), url);
    }

    void keepsUrlWhenNoOverrideIsConfigured()
    {
        MirrorDownload::ResolvedMirror mirror;
        const QString url = QStringLiteral("https://cdn.modrinth.com/data/x.jar");
        QCOMPARE(MirrorDownload::rewriteUrl(url, mirror), url);
    }

    void acceptsMatchingChecksum()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeFile(dir, "payload.bin", "hello world\n");

        Net::Download dl;
        dl.setExpectedIntegrity(QStringLiteral("sha1"), sha1("hello world\n"), 12);

        QString error;
        QVERIFY2(dl.verifyFileIntegrity(path, &error), qPrintable(error));
    }

    void rejectsWrongChecksum()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeFile(dir, "payload.bin", "hello world\n");

        Net::Download dl;
        dl.setExpectedIntegrity(QStringLiteral("sha1"), QStringLiteral("0000000000000000000000000000000000000000"), 12);

        QString error;
        QVERIFY(!dl.verifyFileIntegrity(path, &error));
        QVERIFY(error.contains(QStringLiteral("mismatch")));
    }

    void rejectsWrongSize()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeFile(dir, "payload.bin", "hello world\n");

        Net::Download dl;
        dl.setExpectedIntegrity(QStringLiteral("sha1"), sha1("hello world\n"), 999);

        QString error;
        QVERIFY(!dl.verifyFileIntegrity(path, &error));
        QVERIFY(error.contains(QStringLiteral("size mismatch")));
    }

    void rejectsMissingFile()
    {
        Net::Download dl;
        dl.setExpectedIntegrity(QStringLiteral("sha1"), sha1("anything"), 8);

        QString error;
        QVERIFY(!dl.verifyFileIntegrity(QStringLiteral("does-not-exist.bin"), &error));
        QVERIFY(!error.isEmpty());
    }

    void ignoresUnknownHashType()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeFile(dir, "payload.bin", "hello world\n");

        Net::Download dl;
        dl.setExpectedIntegrity(QStringLiteral("crc32"), QStringLiteral("deadbeef"), -1);

        QString error;
        QVERIFY2(dl.verifyFileIntegrity(path, &error), qPrintable(error));
    }

   private:
    static QString writeFile(const QTemporaryDir& dir, const QString& name, const QByteArray& contents)
    {
        const QString path = QDir(dir.path()).absoluteFilePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(contents);
        file.close();
        return path;
    }

    static QString sha1(const QByteArray& data)
    {
        return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
    }
};

QTEST_GUILESS_MAIN(DownloadIntegrityTest)

#include "DownloadIntegrity_test.moc"
