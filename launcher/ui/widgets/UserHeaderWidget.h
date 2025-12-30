#pragma once

#include <QWidget>
#include <QIcon>

class QLabel;
class QToolButton;
class QMenu;
class QAction;

class UserHeaderWidget : public QWidget {
    Q_OBJECT
public:
    explicit UserHeaderWidget(QWidget* parent = nullptr);
    void setAvatar(const QIcon& icon);
    void setName(const QString& name);
    void setMenu(QMenu* menu);
    void setSettingsAction(QAction* action);
    void setTerracottaAction(QAction* action);
    void setYukariAction(QAction* action);

private:
    QLabel* m_avatar;
    QToolButton* m_nameButton;
    QToolButton* m_settingsBtn;
    QToolButton* m_terracottaBtn;
    QToolButton* m_yukariBtn;
};
