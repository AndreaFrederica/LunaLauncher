// SPDX-License-Identifier: GPL-3.0-only
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QStyle>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include "WindowChromeController.h"
#include "ThemeStyle.h"

// No Application, settings, accounts, network manager or data-directory lock.
int runThemePreview(int argc, char** argv, const QString& path)
{
    QApplication app(argc, argv);
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
    app.setApplicationName("Luna Theme Preview");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024)
        return 2;
    QJsonParseError error;
    const auto spec = QJsonDocument::fromJson(file.readAll(), &error).object();
    if (error.error != QJsonParseError::NoError || spec.isEmpty())
        return 2;
    auto paths = spec.value("paths").toObject();
    for (auto it = paths.begin(); it != paths.end(); ++it) {
        QStringList values;
        for (const auto& value : it.value().toArray())
            values.append(value.toString());
        QDir::setSearchPaths(it.key(), values);
    }
    if (auto style = ThemeStyle::create(spec.value("style").toString(), spec.value("widgetThemeFile").toString()))
        app.setStyle(style);
    auto palette = app.palette();
    const auto groups = spec.value("palette").toArray();
    if (groups.size() != 3)
        return 2;
    const QPalette::ColorGroup groupIds[] = { QPalette::Active, QPalette::Inactive, QPalette::Disabled };
    for (int g = 0; g < 3; ++g) {
        const auto row = groups[g].toArray();
        if (row.size() != QPalette::NColorRoles)
            return 2;
        for (int role = 0; role < row.size(); ++role) {
            QColor color(row[role].toString());
            if (!color.isValid())
                return 2;
            palette.setColor(groupIds[g], QPalette::ColorRole(role), color);
        }
    }
    app.setPalette(palette);
    QStringList iconPaths;
    for (const auto& value : spec.value("iconPaths").toArray())
        iconPaths.append(value.toString());
    QIcon::setThemeSearchPaths(iconPaths);
    QIcon::setFallbackThemeName("pe_colored");
    QIcon::setThemeName(spec.value("icons").toString());
    app.setStyleSheet(spec.value("qss").toString());
    WindowChromeController chrome;
    chrome.initialize(spec.value("decoration") == "luna", spec.value("decorationConfig").toObject());
    chrome.setEffects(spec.value("effects").toObject());
    QMainWindow window;
    window.setWindowTitle(QObject::tr("Theme preview — %1").arg(spec.value("name").toString()));
    window.resize(850, 620);
    auto central = new QWidget(&window);
    auto layout = new QVBoxLayout(central);
    layout->addWidget(new QLabel(QObject::tr("Independent preview · Your saved appearance is unchanged"), central));
    auto menu = window.menuBar()->addMenu(QObject::tr("Example menu"));
    menu->addAction(QObject::tr("Enabled action"));
    auto disabled = menu->addAction(QObject::tr("Disabled action"));
    disabled->setEnabled(false);
    auto split = new QSplitter(central);
    layout->addWidget(split, 1);
    auto tree = new QTreeWidget(split);
    tree->setHeaderLabels({ QObject::tr("Instance"), QObject::tr("Status") });
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    for (const auto& name : { "Survival", "Creative", "Modded" }) {
        auto item = new QTreeWidgetItem(tree, { name, QObject::tr("Ready") });
        item->setIcon(0, QIcon::fromTheme("minecraft"));
        item->setCheckState(0, Qt::Checked);
    }
    tree->setCurrentItem(tree->topLevelItem(0));
    auto panel = new QWidget(split);
    auto form = new QFormLayout(panel);
    form->addRow(QObject::tr("Name"), new QLineEdit("Luna Launcher", panel));
    auto combo = new QComboBox(panel);
    combo->addItems({ "Minecraft", "Fabric", "NeoForge" });
    form->addRow(QObject::tr("Loader"), combo);
    auto check = new QCheckBox(QObject::tr("Enabled option"), panel);
    check->setChecked(true);
    form->addRow(check);
    auto field = new QLineEdit(QObject::tr("Disabled input"), panel);
    field->setEnabled(false);
    form->addRow(field);
    auto progress = new QProgressBar(panel);
    progress->setValue(62);
    form->addRow(QObject::tr("Download"), progress);
    auto background = new QLabel(panel);
    background->setPixmap(QIcon(spec.value("background").toString()).pixmap(180, 80));
    form->addRow(QObject::tr("Background"), background);
    auto dialog = new QPushButton(QObject::tr("Open example dialog…"), panel);
    dialog->setIcon(QIcon::fromTheme("configure", app.style()->standardIcon(QStyle::SP_FileDialogDetailedView)));
    form->addRow(dialog);
    QObject::connect(dialog, &QPushButton::clicked, &window, [&window] {
        QDialog sample(&window);
        sample.setWindowTitle(QObject::tr("Dialog preview"));
        auto layout = new QVBoxLayout(&sample);
        layout->addWidget(new QLabel(QObject::tr("Check title bars, palette, keyboard focus and disabled controls."), &sample));
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &sample);
        layout->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &sample, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &sample, &QDialog::reject);
        sample.exec();
    });
    auto log = new QTextEdit(central);
    QTextCursor cursor(log->document());
    for (const auto& value : spec.value("logs").toArray()) {
        const auto sample = value.toObject();
        QTextCharFormat format;
        format.setForeground(QColor(sample.value("foreground").toString()));
        format.setBackground(QColor(sample.value("background").toString()));
        cursor.insertText(sample.value("text").toString() + "\n", format);
    }
    log->setReadOnly(true);
    log->setMaximumHeight(140);
    layout->addWidget(log);
    auto close = new QDialogButtonBox(QDialogButtonBox::Close, central);
    layout->addWidget(close);
    QObject::connect(close, &QDialogButtonBox::rejected, &window, &QWidget::close);
    window.setCentralWidget(central);
    window.show();
    const auto capture = qEnvironmentVariable("LUNA_THEME_PREVIEW_CAPTURE");
    if (!capture.isEmpty())
        QTimer::singleShot(600, &window, [&window, capture] {
            const bool ok = window.grab().save(capture);
            qApp->exit(ok ? 0 : 3);
        });
    return app.exec();
}
