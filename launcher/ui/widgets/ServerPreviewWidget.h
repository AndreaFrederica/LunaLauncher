#pragma once

#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <memory>
#include <QToolButton>
#include <QEnterEvent>
#include <QScrollArea>
#include "minecraft/launch/MinecraftTarget.h"
#include "BaseInstance.h"

class ServerEntryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ServerEntryWidget(InstancePtr instance, int index, const QString &name, const MinecraftTarget &target, const QPixmap &initialIcon, QWidget *parent = nullptr);
    void setSelected(bool selected);
    bool isSelected() const { return m_isSelected; }
    int calculateIdealWidth() const;

signals:
    void selected(ServerEntryWidget* entry);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void refreshStatus();
    void updateUI(bool isOnline, int players, const QString &motdHtml, const QPixmap &icon);
    void updateLoadingUI();
    void updateOfflineUI(const QString &error);

private:
    void updateStyle();
    void saveIconToDisk(const QByteArray &iconData);

    InstancePtr m_instance;
    int m_index;
    MinecraftTarget m_target;
    QString m_name;
    bool m_isSelected = false;
    bool m_isHovered = false;

    QLabel *m_iconLabel;
    QLabel *m_nameLabel;
    QLabel *m_statusLabel;
    QLabel *m_statusDot;

    QString m_fullMotdHtml;
    QPixmap m_cachedIcon;
};

class ServerPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ServerPreviewWidget(QWidget *parent = nullptr);
    void setInstance(InstancePtr instance);

private slots:
    void onEntrySelected(ServerEntryWidget* entry);

private:
    void loadServerInfo();
    void clearLayout();

    InstancePtr m_instance;
    QVBoxLayout *m_layout; // Layout of the container widget
    QWidget *m_container; // Widget inside ScrollArea
    QList<ServerEntryWidget*> m_entries;
};
