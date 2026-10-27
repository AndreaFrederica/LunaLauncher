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

class DownloadIntegrityTest : public QObject {
    Q_OBJECT

   private slots:
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
