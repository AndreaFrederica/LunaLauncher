// SPDX-License-Identifier: GPL-3.0-only

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <cstdio>

#include <modplatform/flame/CurseForgeExternalTool.h>
#include <modplatform/flame/CurseForgeDownloadPageService.h>

class CurseForgeDownloadPageServiceTest : public QObject {
    Q_OBJECT

   private slots:
    void protocolVersion()
    {
        QCOMPARE(CurseForgeExternalTool::ProtocolVersion, 1);
        QCOMPARE(CurseForgeDownloadPageService::ProtocolVersion, CurseForgeExternalTool::ProtocolVersion);
    }

    void resolvesExecutable()
    {
        QCOMPARE(CurseForgeExternalTool::resolveExecutable(QCoreApplication::applicationFilePath()),
                 QCoreApplication::applicationFilePath());

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile ordinaryFile(directory.filePath("not-a-tool"));
        QVERIFY(ordinaryFile.open(QIODevice::WriteOnly));
        ordinaryFile.write("not executable");
        ordinaryFile.close();
        QVERIFY(CurseForgeExternalTool::resolveExecutable(ordinaryFile.fileName()).isEmpty());
    }

    void probesProtocol()
    {
        QString error;
        bool supportsHeadless = false;
        QVERIFY2(CurseForgeExternalTool::probe(QCoreApplication::applicationFilePath(), &error, &supportsHeadless), qPrintable(error));
        QVERIFY(supportsHeadless);
    }
};

int main(int argc, char** argv)
{
    if (argc == 2 && QByteArray(argv[1]) == "--probe") {
        fputs("{\"protocolVersion\":1,\"capabilities\":[\"curseforgeRestrictedDownload\"],\"headless\":true}\n", stdout);
        return 0;
    }

    QCoreApplication application(argc, argv);
    CurseForgeDownloadPageServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "CurseForgeDownloadPageService_test.moc"
