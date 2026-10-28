// SPDX-License-Identifier: GPL-3.0-only
#include "EnhancedOfflineDialog.h"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QUuid>
#include <QVBoxLayout>
#include "minecraft/auth/LocalYggdrasil.h"
#include "minecraft/auth/MinecraftUuid.h"

MinecraftAccountPtr createEnhancedOfflineAccount(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Add Enhanced Offline Account"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* hint = new QLabel(
        QObject::tr("Create a local identity with a persistent UUID and local skin/cape. The service runs while the launcher is open."));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* form = new QFormLayout;
    layout->addLayout(form);
    auto* name = new QLineEdit;
    form->addRow(QObject::tr("Player name:"), name);
    auto* password = new QLineEdit;
    password->setEchoMode(QLineEdit::Password);
    form->addRow(QObject::tr("Local password:"), password);
    auto* uuid = new QLineEdit;
    uuid->setPlaceholderText(QObject::tr("Leave empty to use the standard offline UUID"));
    form->addRow(QObject::tr("Fixed UUID:"), uuid);
    auto* skin = new QLineEdit;
    auto* cape = new QLineEdit;
    auto* slim = new QCheckBox(QObject::tr("Slim skin model"));
    for (auto entry : { qMakePair(QObject::tr("Skin PNG:"), skin), qMakePair(QObject::tr("Cape PNG:"), cape) }) {
        auto* row = new QHBoxLayout;
        row->addWidget(entry.second);
        auto* browse = new QPushButton(QObject::tr("Browse…"));
        row->addWidget(browse);
        form->addRow(entry.first, row);
        QObject::connect(browse, &QPushButton::clicked, &dialog, [&, edit = entry.second] {
            const auto path = QFileDialog::getOpenFileName(&dialog, QObject::tr("Select texture"), {}, QObject::tr("PNG images (*.png)"));
            if (!path.isEmpty())
                edit->setText(path);
        });
    }
    form->addRow(slim);
    auto* status = new QLabel;
    status->setWordWrap(true);
    layout->addWidget(status);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    MinecraftAccountPtr account;
    shared_qobject_ptr<AuthFlow> flow;
    QString createdProfile;
    QString createdUser;
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const auto username = name->text().trimmed();
        if (!QRegularExpression("^[A-Za-z0-9_]{1,16}$").match(username).hasMatch() || password->text().toUtf8().size() < 8 ||
            (!uuid->text().trimmed().isEmpty() && parseMinecraftUuid(uuid->text().trimmed()).isNull())) {
            status->setText(
                QObject::tr("Use a 1–16 character player name, a password of at least 8 bytes, and a valid UUID if specified."));
            return;
        }
        buttons->setEnabled(false);
        status->setText(QObject::tr("Starting local authentication service…"));
        auto& service = LocalYggdrasil::instance();
        QString error;
        QJsonObject profile;
        auto fail = [&](const QString& message) {
            status->setText(message);
            buttons->setEnabled(true);
        };
        if (createdProfile.isEmpty()) {
            if (!service.createProfile(username, password->text(), uuid->text().trimmed(), profile, error)) {
                fail(error);
                return;
            }
            createdProfile = profile.value("id").toString();
            createdUser = profile.value("user_id").toString();
            name->setEnabled(false);
            password->setEnabled(false);
            uuid->setEnabled(false);
        }
        for (auto entry : { qMakePair(QString("skin"), skin), qMakePair(QString("cape"), cape) }) {
            if (entry.second->text().isEmpty())
                continue;
            QFile file(entry.second->text());
            if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) {
                fail(QObject::tr("Cannot read texture, or file exceeds 2 MiB."));
                return;
            }
            const auto model = entry.first == "skin" && slim->isChecked() ? "?model=slim" : "";
            if (!service.request("PUT", "profiles/" + createdProfile + "/" + entry.first + model, file.readAll(), profile, error,
                                 "image/png")) {
                fail(error);
                return;
            }
        }
        account = MinecraftAccount::createYggdrasil(username, password->text(), service.apiUrl(), service.apiUrl(),
                                                    QObject::tr("Enhanced Offline"), "/authserver/refresh", "/authserver/validate");
        account->accountData()->yggdrasilConfig.managedLocal = true;
        flow = account->login();
        QObject::connect(flow.get(), &Task::succeeded, &dialog, &QDialog::accept);
        QObject::connect(flow.get(), &Task::failed, &dialog, [&](const QString& reason) {
            status->setText(reason);
            buttons->setEnabled(true);
        });
        flow->start();
    });
    if (dialog.exec() == QDialog::Accepted)
        return account;
    if (flow && flow->isRunning())
        flow->abort();
    if (!createdUser.isEmpty()) {
        QJsonObject ignored;
        QString error;
        LocalYggdrasil::instance().request("DELETE", "users/" + createdUser, {}, ignored, error);
    }
    return nullptr;
}
