#include "UntrustedModsDialog.h"
#include "ui_UntrustedModsDialog.h"

#include "Application.h"
#include "cli/ScopedUserInteraction.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QJsonObject>
#include <QPushButton>
#include <QTimer>

std::optional<bool> UntrustedModsDialog::confirm(const QStringList& paths, QWidget* parent)
{
    if (APPLICATION->isHeadless()) {
        if (!activeUserInteraction)
            return std::nullopt;
        const auto choice = activeUserInteraction->select(
            tr("This modpack includes files from outside Modrinth or CurseForge:\n%1\n\nOnly continue if you trust this modpack.")
                .arg(paths.join('\n')),
            QJsonArray{ QJsonObject{ { "id", "cancel" }, { "name", tr("Cancel") } },
                        QJsonObject{ { "id", "trust" }, { "name", tr("I trust this modpack and wish to proceed") } } });
        if (!choice || (*choice != 0 && *choice != 1))
            return std::nullopt;
        return *choice == 1;
    }

    UntrustedModsDialog dialog{ paths, parent };
    return dialog.exec() == QDialog::Accepted;
}

UntrustedModsDialog::UntrustedModsDialog(const QStringList& paths, QWidget* parent) : QDialog{ parent }, m_ui{ new Ui::UntrustedModsDialog }
{
    m_ui->setupUi(this);
    m_ui->modList->addItems(paths);

    auto* ok = m_ui->buttonBox->button(QDialogButtonBox::Ok);
    ok->setEnabled(false);

    connect(m_ui->confirmCheckbox, &QAbstractButton::clicked, ok, &QWidget::setEnabled);

    m_ui->confirmCheckbox->setEnabled(false);
    QTimer::singleShot(3000, this, [this] { m_ui->confirmCheckbox->setEnabled(true); });
}

UntrustedModsDialog::~UntrustedModsDialog()
{
    delete m_ui;
}
