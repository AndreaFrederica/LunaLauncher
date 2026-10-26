// SPDX-License-Identifier: GPL-3.0-only
#include <QComboBox>
#include <QCheckBox>
#include <QBuffer>
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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QProxyStyle>
#include <QSlider>
#include <QTabWidget>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSettings>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>
#include <QUrlQuery>
#include <QVBoxLayout>
#include "Application.h"
#include "cli/McpServer.h"
#include "ui/dialogs/ThemeDownloadDialog.h"
#include "ui/themes/AppearanceService.h"
#include "ui/themes/ThemeManager.h"
#include "ui/themes/ThemeThumbnailLoader.h"
#include "ui/themes/WindowChromeController.h"

class ThumbnailReply final : public QNetworkReply {
    QByteArray m_bytes;
    qint64 m_position = 0;
   public:
    ThumbnailReply(const QNetworkRequest& request, const QByteArray& bytes, QObject* parent) : QNetworkReply(parent), m_bytes(bytes)
    {
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(40, this, [this] {
            if (isFinished())
                return;
            emit readyRead();
            setFinished(true);
            emit finished();
        });
    }
    void abort() override
    {
        if (isFinished())
            return;
        setError(OperationCanceledError, "Cancelled");
        setFinished(true);
        emit finished();
    }
    qint64 bytesAvailable() const override { return m_bytes.size() - m_position + QNetworkReply::bytesAvailable(); }
   protected:
    qint64 readData(char* data, qint64 maximum) override
    {
        const auto length = qMin(maximum, m_bytes.size() - m_position);
        if (!length)
            return -1;
        memcpy(data, m_bytes.constData() + m_position, length);
        m_position += length;
        return length;
    }
};
class ThumbnailNetwork final : public QNetworkAccessManager {
   public:
    int active = 0, maximum = 0, requests = 0;
   protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override
    {
        QImage image(100, 60, QImage::Format_RGB32);
        image.fill(Qt::green);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        ++requests;
        maximum = qMax(maximum, ++active);
        auto reply = new ThumbnailReply(request, bytes, this);
        connect(reply, &QNetworkReply::finished, this, [this] { --active; });
        return reply;
    }
};
class ThemeCenterTest : public QObject {
    Q_OBJECT
   private slots:
    void thumbnailConcurrencyAndCancellation()
    {
        QTemporaryDir cache;
        ThumbnailNetwork network;
        ThemeThumbnailLoader loader(&network, cache.path(), nullptr);
        int completed = 0;
        auto callback = [&](const QImage& image) {
            QVERIFY(!image.isNull());
            QCOMPARE(image.pixelColor(10, 10), QColor(Qt::green));
            ++completed;
        };
        for (int i = 0; i < 9; ++i)
            loader.load("https://themes.example/" + QString::number(i) + ".png", callback);
        loader.load("https://themes.example/0.png", callback, true);
        QTRY_COMPARE(completed, 10);
        QCOMPARE(network.requests, 9);
        QVERIFY(network.maximum <= 4);
        loader.load("https://themes.example/0.png", callback);
        QCOMPARE(completed, 11);
        QCOMPARE(network.requests, 9);
        int stale = 0;
        loader.load("https://themes.example/obsolete.png", [&](const QImage&) { ++stale; });
        loader.reset();
        loader.load("https://themes.example/current.png", callback);
        QTRY_COMPARE(completed, 12);
        QCOMPARE(stale, 0);
        QCOMPARE(network.active, 0);
    }
    void nativeControls()
    {
        if (!qEnvironmentVariableIsEmpty("LUNA_THEME_TEST_PROFILE")) {
            QVERIFY2(APPLICATION->themeManager()->appearance()->state().value("error").toString().isEmpty(),
                qPrintable(APPLICATION->themeManager()->appearance()->state().value("error").toString()));
            auto proxy = qobject_cast<QProxyStyle*>(qApp->style());
            QVERIFY(proxy);
            QCOMPARE(QString(proxy->baseStyle()->metaObject()->className()), QString("Kvantum::Style"));
            auto appearance = APPLICATION->themeManager()->appearance();
            const auto theme = appearance->state().value("effective").toObject().value("theme").toString();
            QSettings settings(appearance->component(theme).value("path").toString(), QSettings::IniFormat);
            const QColor expected(settings.value("GeneralColors/window.color").toString().left(7));
            QVERIFY(expected.isValid());
            QCOMPARE(qApp->palette().color(QPalette::Window).rgb(), expected.rgb());
        }
        QWidget window;
        window.resize(700, 480);
        auto layout = new QVBoxLayout(&window);
        auto input = new QLineEdit(&window);
        input->setPlaceholderText("Keyboard input and focus");
        auto combo = new QComboBox(&window);
        combo->addItems({ "Minecraft", "Fabric", "NeoForge" });
        auto check = new QCheckBox("Enabled option", &window);
        auto disabled = new QPushButton("Disabled button", &window);
        disabled->setEnabled(false);
        auto action = new QPushButton(QIcon::fromTheme("configure"), "Button with icon", &window);
        auto slider = new QSlider(Qt::Horizontal, &window);
        slider->setValue(35);
        auto tabs = new QTabWidget(&window);
        tabs->addTab(new QLabel("First native page", tabs), "Instances");
        tabs->addTab(new QLabel("Second native page", tabs), "Settings");
        for (auto widget : QList<QWidget*>{ input, combo, check, disabled, action, slider, tabs })
            layout->addWidget(widget);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        qInfo() << "Native widget scale:" << window.devicePixelRatioF();
        QTest::mouseClick(input, Qt::LeftButton);
        QTest::keyClicks(input, "Luna native theme");
        QCOMPARE(input->text(), QString("Luna native theme"));
        QTest::mouseClick(check, Qt::LeftButton, Qt::NoModifier, QPoint(8, check->height() / 2));
        QVERIFY(check->isChecked());
        combo->setFocus();
        QTest::keyClick(combo, Qt::Key_Down);
        QCOMPARE(combo->currentIndex(), 1);
        tabs->setCurrentIndex(1);
        QCOMPARE(tabs->currentIndex(), 1);
        QTest::qWait(250);
        const auto normal = action->grab().toImage();
        const auto output = qEnvironmentVariable("LUNA_THEME_TEST_OUTPUT");
        if (!output.isEmpty()) {
            QDir().mkpath(output);
            QVERIFY(window.grab().save(QDir(output).filePath("native-controls.png")));
        }
        const auto actionCenter = action->rect().center();
        QSignalSpy pressed(action, &QAbstractButton::pressed);
        QSignalSpy released(action, &QAbstractButton::released);
        QTest::mouseClick(action, Qt::LeftButton, Qt::NoModifier, actionCenter);
        QVERIFY(pressed.count() >= 1);
        QVERIFY(released.count() >= 1);
        // Keep a deterministic pressed-state sample for the visual artifact;
        // some native styles release the button before a synthetic mouse
        // event can be inspected by the test process.
        action->setDown(true);
        QCoreApplication::processEvents();
        QVERIFY(action->grab().toImage() != normal);
        if (!output.isEmpty())
            QVERIFY(window.grab().save(QDir(output).filePath("native-controls-pressed.png")));
        action->setDown(false);
    }
    void cardGallery()
    {
        const auto cache = QDir(APPLICATION->themeManager()->appearance()->cacheDirectory());
        QVERIFY(QDir().mkpath(cache.filePath("images")));
        const auto previews = qEnvironmentVariable("LUNA_THEME_TEST_PREVIEWS");
        QJsonArray items;
        const QStringList names{ "KvArcDark", "KvMojaveLight", "KvAdapta", "KvOxygen", "KvCurves3d", "KvYaru" };
        auto seed = [&](const QUrl& url, const QString& mode, const QString& id, QJsonObject data) {
            QFile file(cache.filePath(QString::fromLatin1(QCryptographicHash::hash(
                url.toEncoded() + mode.toUtf8() + id.toUtf8(), QCryptographicHash::Sha256).toHex()) + ".json"));
            data.insert("cachedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(data).toJson()) > 0;
        };
        for (int i = 0; i < 18; ++i) {
            const auto name = names.at(i % names.size());
            const auto id = QString::number(3000000 + i);
            const QUrl url("https://themes.example/" + name + ".png");
            const auto path = cache.filePath("images/" + QString::fromLatin1(QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha256).toHex()) + ".img");
            QImage image;
            if (!previews.isEmpty())
                QVERIFY(image.load(QDir(previews).filePath(name + ".png")));
            else {
                image = QImage(640, 360, QImage::Format_RGB32);
                image.fill(QColor::fromHsv((i % 6) * 50, 80, 170));
            }
            QVERIFY(image.save(path, "PNG"));
            const QJsonObject item{ { "id", id }, { "name", name + (i >= 6 ? " · Alternative desktop controls" : "") },
                { "author", "Tsu Jan" }, { "provider", "kde" }, { "category", "widgets" },
                { "thumbnail", url.toString() }, { "installable", true }, { "compatibility", "Kvantum" } };
            items.append(item);
            QVERIFY(seed(QUrl("https://api.opendesktop.org/ocs/v1/content/data/" + id), "files", id,
                { { "items", QJsonArray{ item } }, { "files", QJsonArray{ QJsonObject{ { "id", "1" }, { "name", name + ".tar.xz" }, { "installable", true } } } } }));
        }
        QUrl url("https://api.opendesktop.org/ocs/v1/content/data?categories=123&sortmode=high&page=0&pagesize=24");
        QVERIFY(seed(url, "query", {}, { { "items", items }, { "page", 0 }, { "total", 18 }, { "hasNext", false } }));
        ThemeDownloadDialog center;
        center.resize(1400, 850);
        center.show();
        QVERIFY(QTest::qWaitForWindowExposed(&center));
        auto cards = center.findChild<QListWidget*>("themeOnline");
        auto search = center.findChild<QLineEdit*>("themeSearch");
        QVERIFY(cards);
        QVERIFY(search);
        QTRY_COMPARE(cards->count(), 18);
        QTRY_VERIFY(!cards->item(5)->data(Qt::DecorationRole).value<QPixmap>().isNull());
        QCOMPARE(cards->item(0)->text(), names.first());
        QTRY_VERIFY(cards->visualItemRect(cards->item(1)).left() > cards->visualItemRect(cards->item(0)).left());
        if (cards->viewport()->width() >= 660)
            QVERIFY2(cards->visualItemRect(cards->item(2)).top() == cards->visualItemRect(cards->item(0)).top(),
                qPrintable(QString("Viewport %1, grid %2, spacing %3, third card %4")
                    .arg(cards->viewport()->width()).arg(cards->gridSize().width()).arg(cards->spacing()).arg(cards->visualItemRect(cards->item(2)).top())));
        const auto firstRect = cards->visualItemRect(cards->item(0));
        QTest::mouseClick(cards->viewport(), Qt::LeftButton, Qt::NoModifier, firstRect.center());
        QCOMPARE(cards->currentRow(), 0);
        auto files = center.findChild<QComboBox*>("themeFiles");
        QTRY_COMPARE(files->count(), 1);
        search->setText("KvOxygen");
        QVERIFY(cards->item(0)->isHidden());
        QVERIFY(!cards->item(3)->isHidden());
        search->clear();
        cards->scrollToBottom();
        QVERIFY(cards->verticalScrollBar()->value() > 0);
        cards->scrollToTop();
        const auto output = qEnvironmentVariable("LUNA_THEME_TEST_OUTPUT");
        if (!output.isEmpty())
            QVERIFY(center.grab().save(QDir(output).filePath("theme-card-gallery.png")));
        center.resize(800, 620);
        QTest::qWait(150);
        QVERIFY(cards->visualItemRect(cards->item(0)).right() <= cards->viewport()->width());
        if (!output.isEmpty())
            QVERIFY(center.grab().save(QDir(output).filePath("theme-card-gallery-narrow.png")));
    }
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
        auto online = center.findChild<QListWidget*>("themeOnline");
        auto files = center.findChild<QComboBox*>("themeFiles");
        QVERIFY(online);
        QVERIFY(files);
        if (!qEnvironmentVariableIsEmpty("LUNA_THEME_TEST_PROFILE")) {
            auto installed = center.findChild<QTreeWidget*>("themeInstalled");
            QVERIFY(installed);
            bool hasIncompleteGlobal = false;
            bool hasReferenceRow = false;
            for (int i = 0; i < installed->topLevelItemCount(); ++i) {
                const auto package = installed->topLevelItem(i);
                for (int j = 0; j < package->childCount(); ++j) {
                    const auto component = package->child(j);
                    hasIncompleteGlobal |= component->text(1).contains(QStringLiteral("kde-global")) &&
                                           component->text(2).contains(QStringLiteral("需要补齐"));
                    hasReferenceRow |= component->text(1) == QStringLiteral("KDE 组件引用");
                    for (int k = 0; k < component->childCount(); ++k)
                        hasReferenceRow |= component->child(k)->text(1) == QStringLiteral("KDE 组件引用");
                }
            }
            if (hasIncompleteGlobal)
                QVERIFY2(hasReferenceRow, "Global KDE package references should expose an actionable missing-component row.");
        }
        QTRY_COMPARE(online->count(), 1);
        QCOMPARE(online->viewMode(), QListView::IconMode);
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
            online->setCurrentItem(online->item(0));
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
        const auto requestedProfile = qEnvironmentVariable("LUNA_THEME_TEST_PROFILE");
        QByteArrayList arguments{ QByteArray(argv[0]), "--dir", (requestedProfile.isEmpty() ? profile.path() : requestedProfile).toUtf8(), "--mcp" };
        QList<char*> pointers;
        for (auto& value : arguments)
            pointers.append(value.data());
        pointers.append(nullptr);
        int count = arguments.size();
        Q_INIT_RESOURCE(pe_colored);
        Q_INIT_RESOURCE(pe_dark);
        Q_INIT_RESOURCE(pe_light);
        Q_INIT_RESOURCE(pe_blue);
        Q_INIT_RESOURCE(breeze_dark);
        Q_INIT_RESOURCE(breeze_light);
        Q_INIT_RESOURCE(OSX);
        Q_INIT_RESOURCE(iOS);
        Q_INIT_RESOURCE(flat);
        Q_INIT_RESOURCE(flat_white);
        Q_INIT_RESOURCE(backgrounds);
        Q_INIT_RESOURCE(multimc);
        Application app(count, pointers.data());
        if (app.status() != Application::Initialized)
            return 2;
        // Do not start a transport in this native widget test harness.
        for (auto server : app.findChildren<McpServer*>())
            delete server;
        app.themeManager()->appearance()->initialize();
        ThemeCenterTest test;
        result = QTest::qExec(&test, argc, argv);
    }
    QDir::setCurrent(original);
    return result;
}
#include "ThemeCenter_test.moc"
