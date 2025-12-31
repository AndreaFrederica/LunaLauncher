#include "ServerPreviewWidget.h"
#include "MotdRenderer.h"
#include "Application.h"
#include "ui/pages/instance/ServerPingTask.h"
#include "ui/pages/instance/McResolver.h"
#include "ui/pages/instance/McClient.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QDebug>
#include <QPainter>
#include <QStyleOption>
#include <FileSystem.h>
#include <tag_compound.h>
#include <tag_list.h>
#include <tag_string.h>
#include <io/stream_reader.h>
#include <sstream>
#include <QFileInfo>
#include <QEnterEvent>

ServerEntryWidget::ServerEntryWidget(InstancePtr instance, const QString &name, const MinecraftTarget &target, const QPixmap &initialIcon, QWidget *parent)
    : QWidget(parent), m_instance(instance), m_target(target), m_name(name), m_cachedIcon(initialIcon)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    m_statusDot = new QLabel(this);
    m_statusDot->setFixedSize(8, 8);
    m_statusDot->setStyleSheet("background-color: gray; border-radius: 4px;");

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(32, 32);
    m_iconLabel->setScaledContents(true);
    m_iconLabel->setPixmap(m_cachedIcon);

    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    m_nameLabel = new QLabel(this);
    QFont nameFont = m_nameLabel->font();
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setText(m_name);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: gray;");

    textLayout->addWidget(m_nameLabel);
    textLayout->addWidget(m_statusLabel);

    layout->addWidget(m_statusDot);
    layout->addWidget(m_iconLabel);
    layout->addLayout(textLayout);
    layout->addStretch();

    // Default state
    m_statusLabel->setText(tr("-"));

    // Set a fixed height to look good in the toolbar
    setFixedHeight(48);
    // Set a reasonable width
    setMinimumWidth(200);

    refreshStatus();
}

void ServerEntryWidget::refreshStatus()
{
    updateLoadingUI();

    // Re-implementing ping logic here to get full JSON
    McResolver* resolver = new McResolver(nullptr, m_target.address, m_target.port);
    connect(resolver, &McResolver::succeeded, this, [this, resolver](QString ip, int port) {
        McClient* client = new McClient(nullptr, m_target.address, ip, port);
        connect(client, &McClient::succeeded, this, [this](QJsonObject data) {
            bool isOnline = true;
            int players = 0;
            QString motdHtml;
            QPixmap icon = m_cachedIcon;

            // Parse players
            if (data.contains("players")) {
                QJsonObject playersObj = data["players"].toObject();
                players = playersObj["online"].toInt();
            }

            // Parse MOTD
            if (data.contains("description")) {
                motdHtml = MotdRenderer::render(data["description"]);
            }

            // Parse Favicon
            if (data.contains("favicon")) {
                QString faviconStr = data["favicon"].toString();
                // Standard format: data:image/png;base64,<data>
                // But we should be robust and look for the comma
                int commaIndex = faviconStr.indexOf(',');
                if (commaIndex != -1) {
                    faviconStr = faviconStr.mid(commaIndex + 1);
                    QByteArray iconData = QByteArray::fromBase64(faviconStr.toLatin1());
                    if (icon.loadFromData(iconData)) {
                        m_cachedIcon = icon;
                    }
                }
            }

            updateUI(isOnline, players, motdHtml, icon);
        });

        connect(client, &McClient::failed, this, [this](QString error) {
            updateOfflineUI(error);
        });

        connect(client, &McClient::finished, client, &QObject::deleteLater);
        client->getStatusData();
    });

    connect(resolver, &McResolver::failed, this, [this](QString error) {
        updateOfflineUI(error);
    });

    connect(resolver, &McResolver::finished, resolver, &QObject::deleteLater);
    resolver->ping();
}

void ServerEntryWidget::updateLoadingUI()
{
    m_statusDot->setStyleSheet("background-color: orange; border-radius: 4px;");
    m_statusLabel->setText(tr("Pinging..."));
}

void ServerEntryWidget::updateUI(bool isOnline, int players, const QString &motdHtml, const QPixmap &icon)
{
    m_statusDot->setStyleSheet("background-color: #4CAF50; border-radius: 4px;"); // Green
    m_statusLabel->setText(tr("%1 players").arg(players));
    m_iconLabel->setPixmap(icon);

    m_fullMotdHtml = QString("<html><body><b>%1</b><br>%2<br><br>Players: %3</body></html>")
        .arg(m_target.address)
        .arg(motdHtml)
        .arg(players);

    setToolTip(m_fullMotdHtml);
}

void ServerEntryWidget::updateOfflineUI(const QString &error)
{
    m_statusDot->setStyleSheet("background-color: #F44336; border-radius: 4px;"); // Red
    m_statusLabel->setText(tr("Offline"));
    m_iconLabel->setPixmap(m_cachedIcon);
    setToolTip(tr("Offline: %1").arg(error));
}

void ServerEntryWidget::updateStyle()
{
    QPalette pal = palette();
    if (m_isSelected) {
        pal.setColor(QPalette::Window, pal.color(QPalette::Highlight));
        // Keep original text color or ensure it's readable on highlight
        // Often HighlightedText is white, which is good for dark highlight
        // But user requested not to change text color.
        // However, if highlight is dark blue and text is black, it might be unreadable.
        // If "don't change text color" means "keep it black/white as is", we just don't set WindowText/HighlightedText

        // Let's try to respect the user's wish strictly: only change background
        // But we need to make sure the label styles don't force a color that conflicts

        // Reset specific stylesheets to allow palette to work, but we won't set palette text colors to HighlightedText
        m_nameLabel->setStyleSheet("");
        m_statusLabel->setStyleSheet("color: gray;");
    } else if (m_isHovered) {
        QColor highlight = pal.color(QPalette::Highlight);
        highlight.setAlpha(60); // Transparent highlight
        pal.setColor(QPalette::Window, highlight);
        m_nameLabel->setStyleSheet("");
        m_statusLabel->setStyleSheet("color: gray;");
    } else {
        pal.setColor(QPalette::Window, QApplication::palette().color(QPalette::Window));
        m_nameLabel->setStyleSheet("");
        m_statusLabel->setStyleSheet("color: gray;");
    }
    setPalette(pal);
    setAutoFillBackground(m_isSelected || m_isHovered);
    update();
}

void ServerEntryWidget::setSelected(bool selected)
{
    if (m_isSelected == selected) return;
    m_isSelected = selected;
    updateStyle();
}

void ServerEntryWidget::enterEvent(QEnterEvent *event)
{
    m_isHovered = true;
    updateStyle();
    QWidget::enterEvent(event);
}

void ServerEntryWidget::leaveEvent(QEvent *event)
{
    m_isHovered = false;
    updateStyle();
    QWidget::leaveEvent(event);
}

void ServerEntryWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_instance) {
        APPLICATION->launch(m_instance, true, false, std::make_shared<MinecraftTarget>(m_target));
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ServerEntryWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit selected(this);
    }
    QWidget::mousePressEvent(event);
}

void ServerEntryWidget::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ServerEntryWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction(tr("Refresh"), this, &ServerEntryWidget::refreshStatus);
    menu.exec(event->globalPos());
}

// ServerPreviewWidget implementation

ServerPreviewWidget::ServerPreviewWidget(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_container = new QWidget(scrollArea);
    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_layout->addStretch();

    scrollArea->setWidget(m_container);
    mainLayout->addWidget(scrollArea);
}

void ServerPreviewWidget::setInstance(InstancePtr instance)
{
    m_instance = instance;
    loadServerInfo();
}

void ServerPreviewWidget::clearLayout()
{
    m_entries.clear();
    QLayoutItem *item;
    while ((item = m_layout->takeAt(0))) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void ServerPreviewWidget::onEntrySelected(ServerEntryWidget* selectedEntry)
{
    for (auto entry : m_entries) {
        entry->setSelected(entry == selectedEntry);
    }
}

void ServerPreviewWidget::loadServerInfo()
{
    clearLayout();

    if (!m_instance) {
        QLabel *label = new QLabel(tr("No Instance"), m_container);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        m_layout->addStretch();
        return;
    }

    // Read servers.dat
    QString serversDatPath = FS::PathCombine(m_instance->gameRoot(), "servers.dat");
    bool hasServers = false;

    try {
        if (QFileInfo::exists(serversDatPath)) {
            QByteArray input = FS::read(serversDatPath);
            std::istringstream foo(std::string(input.constData(), input.size()));
            auto pair = nbt::io::read_compound(foo);

            if (pair.second) {
                auto& serversList = pair.second->at("servers").as<nbt::tag_list>();
                for (size_t i = 0; i < serversList.size(); ++i) {
                    auto& serverTag = serversList[i].as<nbt::tag_compound>();
                    std::string nameStr(serverTag["name"]);
                    std::string addressStr(serverTag["ip"]);

                    QPixmap icon = QIcon::fromTheme("unknown_server").pixmap(32, 32);
                    if (serverTag.has_key("icon", nbt::tag_type::String)) {
                        std::string base64str(serverTag["icon"]);
                        QByteArray iconData = QByteArray::fromBase64(base64str.c_str());
                        QPixmap px;
                        if (px.loadFromData(iconData)) {
                            icon = px;
                        }
                    }

                    QString name = QString::fromUtf8(nameStr.c_str());
                    MinecraftTarget target = MinecraftTarget::parse(QString::fromUtf8(addressStr.c_str()), false);

                    ServerEntryWidget *entry = new ServerEntryWidget(m_instance, name, target, icon, m_container);
                    connect(entry, &ServerEntryWidget::selected, this, &ServerPreviewWidget::onEntrySelected);
                    m_layout->addWidget(entry);
                    m_entries.append(entry);
                    hasServers = true;
                }
            }
        }
    } catch (...) {
        qWarning() << "Failed to parse servers.dat for instance" << m_instance->id();
    }

    if (!hasServers) {
        QLabel *label = new QLabel(tr("No Server Configured"), m_container);
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        m_layout->addWidget(label);
    }

    m_layout->addStretch();
}
