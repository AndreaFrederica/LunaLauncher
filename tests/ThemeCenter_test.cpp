// SPDX-License-Identifier: GPL-3.0-only
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QUrlQuery>
#include <QVBoxLayout>
#include "Application.h"
#include "cli/McpServer.h"
#include "ui/dialogs/ThemeDownloadDialog.h"
#include "ui/themes/AppearanceService.h"
#include "ui/themes/ThemeManager.h"
#include "ui/themes/WindowChromeController.h"

class ThemeCenterTest : public QObject {
    Q_OBJECT
   private slots:
    void navigationAndTransactions()
    {
        // Seed the provider's normal cache, so native UI assertions are deterministic
        // and do not depend on an external service's response time or contents.
        auto cache = QDir(APPLICATION->themeManager()->appearance()->cacheDirectory());
        QVERIFY(QDir().mkpath(cache.absolutePath()));
        const QJsonObject item{ { "id", "2379809" },
                                { "name", "Luna test palette" },
                                { "provider", "kde" },
                                { "category", "palette" },
                                { "author", "Luna" },
                                { "version", "1.0" },
                                { "license", "MIT" },
                                { "description", "Local test fixture for the native theme center." },
                                { "compatibility", "KDE .colors" },
                                { "installable", true } };
        auto seed = [&](const QUrl& url, const QString& mode, const QString& id, const QJsonObject& data) {
            QFile file(cache.filePath(
                QString::fromLatin1(
                    QCryptographicHash::hash(url.toEncoded() + mode.toUtf8() + id.toUtf8(), QCryptographicHash::Sha256).toHex()) +
                ".json"));
            auto result = data;
            result.insert("cachedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(result).toJson()) > 0;
        };
        QUrl query("https://api.kde-look.org/ocs/v1/content/data");
        QUrlQuery parameters;
        parameters.addQueryItem("categories", "112");
        parameters.addQueryItem("sortmode", "high");
        parameters.addQueryItem("page", "0");
        parameters.addQueryItem("pagesize", "24");
        query.setQuery(parameters);
        QVERIFY(seed(query, "query", {}, { { "items", QJsonArray{ item } }, { "page", 0 }, { "pageSize", 24 }, { "hasNext", false } }));
        QVERIFY(seed(QUrl("https://api.kde-look.org/ocs/v1/content/data/2379809"), "files", "2379809",
                     { { "items", QJsonArray{ item } },
                       { "files", QJsonArray{ QJsonObject{
                                      { "id", "1" }, { "name", "LunaDark.colors" }, { "installable", true }, { "sizeKiB", 4 } } } } }));
        ThemeDownloadDialog center;
        auto category = center.findChild<QComboBox*>("themeCategory");
        QVERIFY(category);
        QCOMPARE(category->currentData().toString(), QString("widgets"));
        category->setCurrentIndex(category->findData("palette"));
        center.show();
        QVERIFY(QTest::qWaitForWindowExposed(&center));
        auto navigation = center.findChild<QListWidget*>("themeNavigation");
        auto pages = center.findChild<QStackedWidget*>();
        auto search = center.findChild<QLineEdit*>("themeSearch");
        QVERIFY(navigation);
        QVERIFY(pages);
        QCOMPARE(pages->count(), 4);
        QVERIFY(search);
        auto online = center.findChild<QTreeWidget*>("themeOnline");
        auto files = center.findChild<QComboBox*>("themeFiles");
        QVERIFY(online);
        QVERIFY(files);
        QTRY_COMPARE(online->topLevelItemCount(), 1);
        search->setText("Retained search");
        for (int page : { 1, 2, 3, 0 }) {
            navigation->setCurrentRow(page);
            QCOMPARE(pages->currentIndex(), page);
        }
        QCOMPARE(search->text(), QString("Retained search"));
        navigation->setCurrentRow(2);
        auto mode = center.findChild<QComboBox*>("appearance_mode");
        auto theme = center.findChild<QComboBox*>("appearance_theme");
        auto apply = center.findChild<QPushButton*>("themeApply");
        auto undo = center.findChild<QPushButton*>("themeUndo");
        QVERIFY(mode);
        QVERIFY(theme);
        QVERIFY(apply);
        QVERIFY(undo);
        auto service = APPLICATION->themeManager()->appearance();
        const auto initial = service->state();
        mode->setCurrentIndex(mode->findData("independent"));
        theme->setCurrentIndex(theme->findData("Fusion"));
        QTest::mouseClick(apply, Qt::LeftButton);
        QVERIFY(service->state().value("requiresRestart").toBool());
        QVERIFY(undo->isEnabled());
        QCOMPARE(service->state().value("effective"), initial.value("effective"));
        QTest::mouseClick(undo, Qt::LeftButton);
        QVERIFY(!service->state().value("requiresRestart").toBool());
        const auto output = qEnvironmentVariable("LUNA_THEME_TEST_OUTPUT");
        if (!output.isEmpty()) {
            QDir().mkpath(output);
            QVERIFY(center.grab().save(QDir(output).filePath("theme-center-combination.png")));
            navigation->setCurrentRow(0);
            search->clear();
            online->setCurrentItem(online->topLevelItem(0));
            QTRY_COMPARE(files->count(), 1);
            QCOMPARE(files->currentIndex(), 0);
            QVERIFY(center.grab().save(QDir(output).filePath("theme-center-online.png")));
            center.resize(800, 620);
            QTest::qWait(100);
            QVERIFY(center.grab().save(QDir(output).filePath("theme-center-narrow.png")));
        }
        const auto retainedPage = navigation->currentRow();
        center.hide();
        center.show();
        QCOMPARE(navigation->currentRow(), retainedPage);
    }
    void modalSettingsEntry()
    {
        QDialog settings;
        settings.setModal(true);
        settings.resize(400, 300);
        settings.show();
        QVERIFY(QTest::qWaitForWindowExposed(&settings));
        ThemeDownloadDialog::openCenter();
        auto center = settings.findChild<ThemeDownloadDialog*>();
        QVERIFY(center);
        QVERIFY(QTest::qWaitForWindowExposed(center));
        auto navigation = center->findChild<QListWidget*>("themeNavigation");
        const auto row = navigation->visualItemRect(navigation->item(2));
        QTest::mouseClick(navigation->viewport(), Qt::LeftButton, Qt::NoModifier, row.center());
        QCOMPARE(navigation->currentRow(), 2);
        settings.accept();
        QVERIFY(!center->parentWidget());
        QCOMPARE(navigation->currentRow(), 2);
        center->hide();
    }
    void chromeAndDialogs()
    {
        if (!WindowChromeController::supported())
            QSKIP("Uses native macOS decoration");
        WindowChromeController chrome;
        chrome.initialize(true, { { "height", 36 } });
        QMainWindow window;
        window.setWindowTitle("Luna native decoration");
        window.setCentralWidget(new QLabel("Native Qt content", &window));
        window.resize(600, 400);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto caption = window.findChild<QWidget*>("lunaWindowCaption");
        QVERIFY(caption);
        QCOMPARE(caption->height(), 36);
        QVERIFY(window.windowFlags().testFlag(Qt::FramelessWindowHint));
        window.showMaximized();
        QTRY_VERIFY(window.isMaximized());
        window.showNormal();
        QTRY_VERIFY(!window.isMaximized());
        window.showFullScreen();
        QTRY_VERIFY(window.isFullScreen());
        QVERIFY(!caption->isVisible());
        window.showNormal();
        QTRY_VERIFY(caption->isVisible());
        QDialog dialog(&window);
        dialog.setWindowTitle("Dialog decoration");
        auto layout = new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel("Dialog body", &dialog));
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QTest::qWait(100);
        QVERIFY2(
            dialog.width() >= 220,
            qPrintable(
                QString("Width %1, minimum %2, maximum %3").arg(dialog.width()).arg(dialog.minimumWidth()).arg(dialog.maximumWidth())));
        QVERIFY(dialog.findChild<QWidget*>("lunaWindowCaption"));
        const auto output = qEnvironmentVariable("LUNA_THEME_TEST_OUTPUT");
        if (!output.isEmpty()) {
            QVERIFY(window.grab().save(QDir(output).filePath("native-decoration.png")));
            QVERIFY(dialog.grab().save(QDir(output).filePath("native-dialog.png")));
        }
    }
};
int main(int argc, char** argv)
{
    const auto original = QDir::currentPath();
    QTemporaryDir profile;
    int result = 2;
    {
        QByteArrayList arguments{ QByteArray(argv[0]), "--dir", profile.path().toUtf8(), "--mcp" };
        QList<char*> pointers;
        for (auto& value : arguments)
            pointers.append(value.data());
        pointers.append(nullptr);
        int count = arguments.size();
        Application app(count, pointers.data());
        if (app.status() != Application::Initialized)
            return 2;
        // Do not start a transport in this native widget test harness.
        for (auto server : app.findChildren<McpServer*>())
            delete server;
        Q_INIT_RESOURCE(pe_colored);
        Q_INIT_RESOURCE(backgrounds);
        Q_INIT_RESOURCE(multimc);
        app.themeManager()->appearance()->initialize();
        ThemeCenterTest test;
        result = QTest::qExec(&test, argc, argv);
    }
    QDir::setCurrent(original);
    return result;
}
#include "ThemeCenter_test.moc"
