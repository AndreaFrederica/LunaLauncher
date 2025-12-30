#include "UserHeaderWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMenu>
#include <QAction>

UserHeaderWidget::UserHeaderWidget(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    m_avatar = new QLabel(this);
    m_avatar->setFixedSize(48, 48);
    m_avatar->setScaledContents(true);
    layout->addWidget(m_avatar, 0, Qt::AlignHCenter);

    m_nameButton = new QToolButton(this);
    m_nameButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_nameButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(m_nameButton);

    m_settingsBtn = new QToolButton(this);
    m_settingsBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_settingsBtn->setVisible(false);
    layout->addWidget(m_settingsBtn);

    m_terracottaBtn = new QToolButton(this);
    m_terracottaBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_terracottaBtn->setVisible(false);
    layout->addWidget(m_terracottaBtn);

    m_yukariBtn = new QToolButton(this);
    m_yukariBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_yukariBtn->setVisible(false);
    layout->addWidget(m_yukariBtn);
}

void UserHeaderWidget::setAvatar(const QIcon& icon)
{
    m_avatar->setPixmap(icon.pixmap(48, 48));
}

void UserHeaderWidget::setName(const QString& name)
{
    m_nameButton->setText(name);
}

void UserHeaderWidget::setMenu(QMenu* menu)
{
    m_nameButton->setMenu(menu);
    m_nameButton->setPopupMode(QToolButton::InstantPopup);
}

void UserHeaderWidget::setSettingsAction(QAction* action)
{
    m_settingsBtn->setDefaultAction(action);
    m_settingsBtn->setVisible(action != nullptr);
}
void UserHeaderWidget::setTerracottaAction(QAction* action)
{
    m_terracottaBtn->setDefaultAction(action);
    m_terracottaBtn->setVisible(action != nullptr);
}
void UserHeaderWidget::setYukariAction(QAction* action)
{
    m_yukariBtn->setDefaultAction(action);
    m_yukariBtn->setVisible(action != nullptr);
}
