// SPDX-License-Identifier: GPL-3.0-only
#include "ThemeDownloadDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include "Application.h"
#include "settings/SettingsObject.h"
#include "ui/themes/AppearanceService.h"
#include "ui/themes/ThemeInstallTask.h"
#include "ui/themes/ThemeManager.h"
#include "ui/themes/ThemeStore.h"
#include "ui/themes/ThemeThumbnailLoader.h"

namespace {
constexpr int thumbnailStatusRole = Qt::UserRole + 1;
class ThemeCardDelegate final : public QStyledItemDelegate {
   public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        const auto view = qobject_cast<const QListView*>(parent());
        return view && view->gridSize().isValid() ? view->gridSize() - QSize(2, 2) : QSize(218, 208);
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem panel(option);
        initStyleOption(&panel, index);
        panel.text.clear();
        panel.icon = {};
        auto style = panel.widget ? panel.widget->style() : QApplication::style();
        painter->save();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &panel, painter, panel.widget);
        const auto card = option.rect.adjusted(5, 5, -5, -5);
        const auto preview = QRect(card.left() + 7, card.top() + 7, card.width() - 14, card.height() - 76);
        painter->setPen(option.palette.color((option.state & QStyle::State_Selected) ? QPalette::Highlight : QPalette::Mid));
        painter->drawRoundedRect(card.adjusted(0, 0, -1, -1), 5, 5);
        painter->fillRect(preview, option.palette.brush(QPalette::AlternateBase));
        const auto pixmap = index.data(Qt::DecorationRole).value<QPixmap>();
        if (!pixmap.isNull()) {
            const auto size = pixmap.size().scaled(preview.size(), Qt::KeepAspectRatio);
            const QRect target(preview.center() - QPoint(size.width() / 2, size.height() / 2), size);
            painter->setRenderHint(QPainter::SmoothPixmapTransform);
            painter->drawPixmap(target, pixmap);
        } else {
            painter->setPen(option.palette.color(QPalette::Text));
            painter->drawText(preview, Qt::AlignCenter | Qt::TextWordWrap, index.data(thumbnailStatusRole).toString());
        }
        painter->setClipRect(card);
        painter->setPen(option.palette.color((option.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text));
        auto font = option.font;
        font.setBold(true);
        painter->setFont(font);
        const QRect title(card.left() + 7, preview.bottom() + 9, card.width() - 14, 36);
        painter->drawText(title, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, index.data(Qt::DisplayRole).toString());
        font.setBold(false);
        painter->setFont(font);
        const auto metadata = index.data(Qt::UserRole).toJsonObject();
        painter->drawText(QRect(title.left(), title.bottom() + 2, title.width(), 18), Qt::AlignCenter,
                         QFontMetrics(font).elidedText(metadata.value("author").toString(), Qt::ElideRight, title.width()));
        painter->restore();
    }
};
class ThemeCardView final : public QListWidget {
   public:
    explicit ThemeCardView(QWidget* parent) : QListWidget(parent)
    {
        setViewMode(QListView::IconMode);
        setResizeMode(QListView::Adjust);
        setMovement(QListView::Static);
        setSpacing(0);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        setUniformItemSizes(true);
        setMouseTracking(true);
        setWordWrap(true);
        setItemDelegate(new ThemeCardDelegate(this));
        setMinimumWidth(190);
    }
   protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QListWidget::resizeEvent(event);
        const auto width = qMax(160, viewport()->width() - 16);
        const auto columns = qMax(1, width / 220);
        const auto cellWidth = width / columns;
        setGridSize(QSize(cellWidth, (cellWidth - 24) * 9 / 16 + 86));
    }
};
QIcon iconFor(const QString& name)
{
    QStyle::StandardPixmap fallback = QStyle::SP_FileIcon;
    if (name == "download")
        fallback = QStyle::SP_ArrowDown;
    else if (name == "configure" || name == "view-list-details")
        fallback = QStyle::SP_FileDialogDetailedView;
    else if (name == "folder" || name == "folder-open")
        fallback = QStyle::SP_DirIcon;
    else if (name == "view-refresh")
        fallback = QStyle::SP_BrowserReload;
    else if (name == "go-previous" || name == "edit-undo")
        fallback = QStyle::SP_ArrowBack;
    else if (name == "go-next")
        fallback = QStyle::SP_ArrowForward;
    else if (name == "edit-delete")
        fallback = QStyle::SP_TrashIcon;
    else if (name == "edit-clear")
        fallback = QStyle::SP_DialogResetButton;
    else if (name == "process-stop")
        fallback = QStyle::SP_BrowserStop;
    else if (name == "dialog-ok-apply")
        fallback = QStyle::SP_DialogApplyButton;
    else if (name == "document-open")
        fallback = QStyle::SP_DialogOpenButton;
    else if (name == "document-preview")
        fallback = QStyle::SP_FileDialogContentsView;
    else if (name == "internet-web-browser")
        fallback = QStyle::SP_DesktopIcon;
    return QIcon::fromTheme(name, QApplication::style()->standardIcon(fallback));
}
QPushButton* button(const QString& text, const QString& icon, QWidget* parent)
{
    auto result = new QPushButton(iconFor(icon), text, parent);
    return result;
}
QTreeWidget* tree(QWidget* parent, const QStringList& headers)
{
    auto result = new QTreeWidget(parent);
    result->setHeaderLabels(headers);
    result->setRootIsDecorated(false);
    result->setAlternatingRowColors(true);
    result->setUniformRowHeights(true);
    result->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i < headers.size(); ++i)
        result->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    return result;
}
QString kindName(const QString& kind)
{
    if (kind == "widgets")
        return QObject::tr("控件");
    if (kind == "bundle")
        return QObject::tr("成套组合");
    if (kind == "palette")
        return QObject::tr("配色");
    if (kind == "icons")
        return QObject::tr("图标");
    if (kind == "background")
        return QObject::tr("背景");
    if (kind == "decoration")
        return QObject::tr("窗口装饰");
    return kind;
}
QJsonObject selection(QTreeWidget* view)
{
    return view->currentItem() ? view->currentItem()->data(0, Qt::UserRole).toJsonObject() : QJsonObject();
}
QJsonObject selection(QListWidget* view)
{
    return view->currentItem() ? view->currentItem()->data(Qt::UserRole).toJsonObject() : QJsonObject();
}
QString previewUrl(const QJsonObject& item)
{
    const auto preview = item.value("preview").toString();
    return preview.isEmpty() ? item.value("thumbnail").toString() : preview;
}
void openWeb(const QString& value)
{
    const QUrl url(value);
    if (url.scheme() == "https" && !url.host().isEmpty())
        QDesktopServices::openUrl(url);
}
}  // namespace

void ThemeDownloadDialog::openCenter()
{
    static QPointer<ThemeDownloadDialog> center;
    if (!center)
        center = new ThemeDownloadDialog();
    // Settings uses QDialog::exec(). A parentless window would be blocked by
    // that application-modal dialog. Temporarily join its allowed child chain.
    auto modal = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (modal && modal != center && center->parentWidget() != modal) {
        center->setParent(modal, center->windowFlags());
        connect(
            modal, &QDialog::finished, center,
            [window = center] {
                if (!window)
                    return;
                const bool visible = window->isVisible();
                const auto geometry = window->saveGeometry();
                window->setParent(nullptr, window->windowFlags());
                window->restoreGeometry(geometry);
                if (visible)
                    window->show();
            },
            Qt::SingleShotConnection);
    }
    center->show();
    center->raise();
    center->activateWindow();
}
ThemeDownloadDialog::ThemeDownloadDialog(QWidget* parent) : QDialog(parent), m_service(APPLICATION->themeManager()->appearance())
{
    m_service->initialize();
    m_thumbnails = new ThemeThumbnailLoader(APPLICATION->network().get(), m_service->cacheDirectory(), this);
    setWindowTitle(tr("主题中心 · Classic UI"));
    setObjectName("classicThemeCenter");
    setAttribute(Qt::WA_QuitOnClose, false);
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    resize(1120, 760);
    setMinimumSize(760, 560);
    auto outer = new QVBoxLayout(this);
    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::PlainText);
    outer->addWidget(m_summary);
    auto body = new QHBoxLayout;
    outer->addLayout(body, 1);
    auto side = new QVBoxLayout;
    body->addLayout(side);
    m_navigation = new QListWidget(this);
    m_navigation->setObjectName("themeNavigation");
    m_navigation->setMaximumWidth(150);
    m_navigation->setMinimumWidth(110);
    const QList<QPair<QString, QString>> pages{ { tr("在线主题"), "download" },
                                                { tr("已安装"), "folder" },
                                                { tr("外观组合"), "configure" },
                                                { tr("下载任务"), "view-list-details" } };
    for (const auto& page : pages)
        m_navigation->addItem(new QListWidgetItem(iconFor(page.second), page.first));
    side->addWidget(m_navigation, 1);
    auto sources = button(tr("来源设置…"), "configure", this);
    side->addWidget(sources);
    auto clearCache = button(tr("清理目录缓存"), "edit-clear", this);
    side->addWidget(clearCache);
    connect(clearCache, &QPushButton::clicked, this, [this] {
        m_thumbnails->reset(true);
        QDir cache(m_service->cacheDirectory());
        if (!cache.exists() || cache.removeRecursively())
            m_status->setText(tr("目录缓存已清理，已安装主题保留。"));
        else
            m_status->setText(tr("目录缓存清理失败。"));
    });
    connect(sources, &QPushButton::clicked, this, &ThemeDownloadDialog::manageSources);
    m_pages = new QStackedWidget(this);
    body->addWidget(m_pages, 1);
    connect(m_navigation, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    auto online = new QWidget(this);
    auto layout = new QVBoxLayout(online);
    layout->setContentsMargins(0, 0, 0, 0);
    m_pages->addWidget(online);
    auto selectors = new QHBoxLayout;
    layout->addLayout(selectors);
    m_source = new QComboBox(online);
    m_source->setObjectName("themeSource");
    selectors->addWidget(m_source, 1);
    m_category = new QComboBox(online);
    m_category->setObjectName("themeCategory");
    for (const auto& p : QList<QPair<QString, QString>>{ { tr("配色"), "palette" },
                                                         { tr("图标"), "icons" },
                                                         { tr("成套主题（Plasma 5 / 6）"), "bundle" },
                                                         { tr("控件 / Kvantum"), "widgets" },
                                                         { tr("KWin 装饰"), "decoration" },
                                                         { tr("Plasma 桌面"), "plasma" } })
        m_category->addItem(p.first, p.second);
    m_category->setCurrentIndex(m_category->findData("widgets"));
    selectors->addWidget(m_category);
    m_sort = new QComboBox(online);
    m_sort->addItem(tr("评分"), "high");
    m_sort->addItem(tr("最新"), "new");
    m_sort->addItem(tr("名称"), "alpha");
    m_sort->addItem(tr("下载量"), "down");
    selectors->addWidget(m_sort);
    auto refresh = button(tr("刷新"), "view-refresh", online);
    selectors->addWidget(refresh);
    auto searchRow = new QHBoxLayout;
    layout->addLayout(searchRow);
    m_search = new QLineEdit(online);
    m_search->setObjectName("themeSearch");
    m_search->setPlaceholderText(tr("筛选已载入项目"));
    m_search->setClearButtonEnabled(true);
    searchRow->addWidget(m_search, 1);
    m_compatible = new QCheckBox(tr("仅显示可导入类别"), online);
    m_compatible->setChecked(true);
    searchRow->addWidget(m_compatible);
    m_status = new QLabel(online);
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    auto split = new QSplitter(online);
    layout->addWidget(split, 1);
    m_online = new ThemeCardView(split);
    m_online->setObjectName("themeOnline");
    auto details = new QWidget(split);
    details->setMinimumWidth(260);
    auto dl = new QVBoxLayout(details);
    m_title = new QLabel(tr("选择主题查看文件和兼容信息"), details);
    m_title->setWordWrap(true);
    m_title->setTextFormat(Qt::PlainText);
    dl->addWidget(m_title);
    m_thumbnail = new QLabel(details);
    m_thumbnail->setAlignment(Qt::AlignCenter);
    m_thumbnail->setMinimumHeight(100);
    m_thumbnail->setMaximumHeight(170);
    dl->addWidget(m_thumbnail);
    m_description = new QTextBrowser(details);
    m_description->setOpenExternalLinks(false);
    dl->addWidget(m_description, 1);
    m_files = new QComboBox(details);
    m_files->setObjectName("themeFiles");
    m_files->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_files->setMinimumContentsLength(10);
    dl->addWidget(m_files);
    auto allowRow = new QGridLayout;
    dl->addLayout(allowRow);
    int allowIndex = 0;
    for (const auto& kind : { "palette", "icons", "widgets", "decoration", "background" }) {
        auto c = new QCheckBox(kindName(kind), details);
        c->setChecked(true);
        m_allowed.insert(kind, c);
        allowRow->addWidget(c, allowIndex / 3, allowIndex % 3);
        ++allowIndex;
    }
    m_install = button(tr("安装所选文件"), "download", details);
    m_install->setEnabled(false);
    dl->addWidget(m_install);
    auto storePage = button(tr("在来源网站中打开"), "internet-web-browser", details);
    dl->addWidget(storePage);
    connect(storePage, &QPushButton::clicked, this, [this] { openWeb(m_selected.value("pageUrl").toString()); });
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    split->setSizes({ 520, 340 });
    auto pagination = new QHBoxLayout;
    layout->addLayout(pagination);
    m_previous = button(tr("上一页"), "go-previous", online);
    m_next = button(tr("下一页"), "go-next", online);
    pagination->addWidget(m_previous);
    pagination->addStretch();
    pagination->addWidget(m_next);
    m_previous->setEnabled(false);
    m_next->setEnabled(false);
    connect(refresh, &QPushButton::clicked, this, [this] { query(true, m_page); });
    connect(m_previous, &QPushButton::clicked, this, [this] { query(false, qMax(0, m_page - 1)); });
    connect(m_next, &QPushButton::clicked, this, [this] { query(false, m_page + 1); });
    connect(m_source, &QComboBox::currentIndexChanged, this, [this] {
        if (m_filling)
            return;
        const bool kde = m_source->currentData() == "kde";
        m_category->setEnabled(kde);
        m_sort->setEnabled(kde);
        if (!kde)
            APPLICATION->settings()->set("ThemeDownloadSource", m_source->currentData());
        query();
    });
    connect(m_category, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_filling)
            query();
    });
    connect(m_sort, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_filling)
            query();
    });
    connect(m_search, &QLineEdit::textChanged, this, &ThemeDownloadDialog::filter);
    connect(m_compatible, &QCheckBox::toggled, this, &ThemeDownloadDialog::filter);
    connect(m_online, &QListWidget::currentItemChanged, this, &ThemeDownloadDialog::showDetails);
    connect(m_install, &QPushButton::clicked, this, &ThemeDownloadDialog::installSelected);
    connect(m_files, &QComboBox::currentIndexChanged, this,
            [this] { m_install->setEnabled(m_files->currentData().toJsonObject().value("installable").toBool()); });

    auto installed = new QWidget(this);
    auto il = new QVBoxLayout(installed);
    il->setContentsMargins(0, 0, 0, 0);
    m_pages->addWidget(installed);
    auto imports = new QHBoxLayout;
    il->addLayout(imports);
    auto importFile = button(tr("导入文件…"), "document-open", installed);
    auto importFolder = button(tr("导入文件夹…"), "folder-open", installed);
    auto rescan = button(tr("重新扫描"), "view-refresh", installed);
    imports->addWidget(importFile);
    imports->addWidget(importFolder);
    imports->addStretch();
    imports->addWidget(rescan);
    m_installed = tree(installed, { tr("主题包 / 组件"), tr("版本 / 格式"), tr("状态") });
    m_installed->setObjectName("themeInstalled");
    m_installed->setRootIsDecorated(true);
    il->addWidget(m_installed, 1);
    auto actions = new QHBoxLayout;
    il->addLayout(actions);
    auto configure = button(tr("使用所选主题包…"), "configure", installed);
    m_update = button(tr("检查来源更新"), "view-refresh", installed);
    auto folder = button(tr("打开目录"), "folder-open", installed);
    m_remove = button(tr("卸载"), "edit-delete", installed);
    actions->addWidget(configure);
    actions->addWidget(m_update);
    actions->addWidget(folder);
    actions->addStretch();
    actions->addWidget(m_remove);
    connect(importFile, &QPushButton::clicked, this, [this] { importLocal(false); });
    connect(importFolder, &QPushButton::clicked, this, [this] { importLocal(true); });
    connect(rescan, &QPushButton::clicked, this, [] { APPLICATION->themeManager()->refresh(); });
    connect(configure, &QPushButton::clicked, this, &ThemeDownloadDialog::configureSelected);
    connect(m_remove, &QPushButton::clicked, this, &ThemeDownloadDialog::removeSelected);
    connect(m_update, &QPushButton::clicked, this, &ThemeDownloadDialog::updateSelected);
    connect(folder, &QPushButton::clicked, this, [this] {
        auto path = selection(m_installed).value("directory").toString();
        if (!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    connect(m_installed, &QTreeWidget::currentItemChanged, this, [this] {
        const auto s = selection(m_installed);
        m_remove->setEnabled(!s.value("packageId").toString().isEmpty() || s.value("removable").toBool());
        m_update->setEnabled(!s.value("packageId").toString().isEmpty() || s.value("dependency").toBool());
        m_update->setText(s.value("dependency").toBool() ? tr("选择并下载此依赖…") : tr("检查来源更新"));
    });

    auto combination = new QWidget(this);
    auto cl = new QVBoxLayout(combination);
    cl->setContentsMargins(0, 0, 0, 0);
    m_pages->addWidget(combination);
    m_stateLabel = new QLabel(combination);
    m_stateLabel->setWordWrap(true);
    m_stateLabel->setTextFormat(Qt::PlainText);
    cl->addWidget(m_stateLabel);
    auto scroll = new QScrollArea(combination);
    scroll->setWidgetResizable(true);
    cl->addWidget(scroll, 1);
    auto fields = new QWidget(scroll);
    auto form = new QFormLayout(fields);
    scroll->setWidget(fields);
    for (const auto& option : QList<QPair<QString, QString>>{ { "mode", tr("外观模式") },
                                                              { "theme", tr("控件风格 / 皮肤") },
                                                              { "colorMode", tr("配色模式") },
                                                              { "palette", tr("固定配色") },
                                                              { "lightPalette", tr("系统浅色时") },
                                                              { "darkPalette", tr("系统深色时") },
                                                              { "icons", tr("图标") },
                                                              { "background", tr("背景") },
                                                              { "decoration", tr("窗口装饰") },
                                                              { "decorationTheme", tr("装饰主题") } }) {
        auto combo = new QComboBox(fields);
        combo->setObjectName("appearance_" + option.first);
        combo->setMinimumContentsLength(20);
        m_options.insert(option.first, combo);
        form->addRow(option.second, combo);
        connect(combo, &QComboBox::currentIndexChanged, this, [this] {
            if (m_filling)
                return;
            m_dirty = true;
            updateState();
        });
    }
    m_titlebarPalette = new QCheckBox(tr("向支持的系统标题栏传递应用配色（KDE）"), fields);
    form->addRow(m_titlebarPalette);
    connect(m_titlebarPalette, &QCheckBox::toggled, this, [this] {
        if (!m_filling)
            m_dirty = true;
    });
    auto help = new QLabel(tr("安装不会自动应用。Kvantum 可改变真实 Qt 控件的形状、尺寸和状态绘制，并使用主题自带配色。"
                              "配色和图标可即时切换；控件皮肤、外观模式和窗口装饰的变化会在下次启动生效。"
                              "KWin / Aurorae 窗口装饰和 Plasma 桌面组件需要各自的运行环境。"),
                           fields);
    help->setWordWrap(true);
    form->addRow(help);
    m_draftStatus = new QLabel(combination);
    m_draftStatus->setWordWrap(true);
    m_draftStatus->setTextFormat(Qt::PlainText);
    cl->addWidget(m_draftStatus);
    auto ca = new QHBoxLayout;
    cl->addLayout(ca);
    auto preview = button(tr("独立预览"), "document-preview", combination);
    auto apply = button(tr("应用组合"), "dialog-ok-apply", combination);
    apply->setObjectName("themeApply");
    auto reset = button(tr("重载已保存组合"), "view-refresh", combination);
    m_undo = button(tr("撤销待重启更改"), "edit-undo", combination);
    m_undo->setObjectName("themeUndo");
    ca->addWidget(preview);
    ca->addWidget(reset);
    ca->addWidget(m_undo);
    ca->addStretch();
    ca->addWidget(apply);
    connect(preview, &QPushButton::clicked, this, [this] {
        QString error;
        m_service->preview(draft(), this, error);
        if (!error.isEmpty())
            m_draftStatus->setText(error);
    });
    connect(apply, &QPushButton::clicked, this, [this] {
        QString error;
        if (m_service->apply(draft(), m_baseRevision, error).isEmpty())
            m_draftStatus->setText(error);
        else {
            m_dirty = false;
            refreshChoices(true);
            m_draftStatus->setText(tr("组合已保存。"));
        }
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        m_dirty = false;
        refreshChoices(true);
        m_draftStatus->clear();
    });
    connect(m_undo, &QPushButton::clicked, this, [this] {
        QString error;
        if (!m_service->undoPending(m_service->state().value("revision").toInteger(), error))
            m_draftStatus->setText(error);
        else {
            m_dirty = false;
            refreshChoices(true);
        }
    });

    auto tasksPage = new QWidget(this);
    auto tl = new QVBoxLayout(tasksPage);
    tl->setContentsMargins(0, 0, 0, 0);
    m_pages->addWidget(tasksPage);
    auto note = new QLabel(tr("关闭主题中心后，安装任务仍会继续。安装结果不会改变当前外观。"), tasksPage);
    note->setWordWrap(true);
    tl->addWidget(note);
    m_tasks = tree(tasksPage, { tr("任务"), tr("进度"), tr("状态") });
    tl->addWidget(m_tasks, 1);
    auto ta = new QHBoxLayout;
    tl->addLayout(ta);
    m_cancel = button(tr("取消任务"), "process-stop", tasksPage);
    m_retry = button(tr("重试"), "view-refresh", tasksPage);
    ta->addWidget(m_cancel);
    ta->addWidget(m_retry);
    ta->addStretch();
    m_cancel->setEnabled(false);
    m_retry->setEnabled(false);
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        if (auto task = selectedTask())
            task->abort();
    });
    connect(m_retry, &QPushButton::clicked, this, [this] {
        if (auto task = dynamic_cast<ThemeInstallTask*>(selectedTask()); task && task->isFinished() && !task->wasSuccessful())
            startInstall(task->request());
    });
    connect(m_tasks, &QTreeWidget::currentItemChanged, this, [this] {
        auto task = selectedTask();
        m_cancel->setEnabled(task && task->isRunning() && task->canAbort());
        m_retry->setEnabled(task && task->isFinished() && !task->wasSuccessful());
    });
    connect(m_service, &AppearanceService::taskAdded, this, &ThemeDownloadDialog::addTask);
    connect(m_service, &AppearanceService::previewFailed, this, [this](const QString& error) {
        m_status->setText(error);
        m_draftStatus->setText(error);
    });
    for (const auto& task : m_service->tasks())
        addTask(task.get());
    connect(m_service, &AppearanceService::catalogChanged, this, [this] {
        refreshInstalled();
        refreshChoices(false);
        emit themesInstalled();
    });
    connect(m_service, &AppearanceService::stateChanged, this, [this] {
        refreshInstalled();
        if (!m_dirty)
            refreshChoices(true);
        updateState();
    });
    auto close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    close->button(QDialogButtonBox::Close)->setText(tr("关闭"));
    outer->addWidget(close);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::hide);
    m_navigation->setCurrentRow(0);
    reloadSources();
    refreshInstalled();
    refreshChoices(true);
    updateState();
    QTimer::singleShot(0, this, [this] { query(); });
}

void ThemeDownloadDialog::reloadSources()
{
    m_filling = true;
    const auto previous = m_source->currentData().toString();
    m_source->clear();
    m_source->addItem("KDE Store", "kde");
    auto sources = APPLICATION->settings()->get("ThemeDownloadSources").toStringList();
    if (sources.isEmpty())
        sources.append("https://github.com/PrismLauncher/Themes");
    for (const auto& source : sources) {
        const auto repo = ThemeStoreTask::githubRepository(source);
        if (!repo.isEmpty())
            m_source->addItem("GitHub · " + repo, "https://github.com/" + repo);
    }
    const int index = m_source->findData(previous);
    m_source->setCurrentIndex(index >= 0 ? index : 0);
    m_filling = false;
}
void ThemeDownloadDialog::manageSources()
{
    auto sources = APPLICATION->settings()->get("ThemeDownloadSources").toStringList();
    if (sources.isEmpty())
        sources.append("https://github.com/PrismLauncher/Themes");
    bool ok = false;
    const auto text = QInputDialog::getMultiLineText(
        this, tr("主题下载来源"), tr("每行一个 GitHub 仓库 HTTPS 地址。KDE Store 是内置来源。"), sources.join('\n'), &ok);
    if (!ok)
        return;
    QStringList normalized;
    for (const auto& line : text.split('\n', Qt::SkipEmptyParts)) {
        const auto repo = ThemeStoreTask::githubRepository(line.trimmed());
        if (repo.isEmpty()) {
            QMessageBox::warning(this, tr("来源无效"), line);
            return;
        }
        const auto source = "https://github.com/" + repo;
        if (!normalized.contains(source, Qt::CaseInsensitive))
            normalized.append(source);
    }
    APPLICATION->settings()->set("ThemeDownloadSources", normalized);
    reloadSources();
    query();
}
QJsonObject ThemeDownloadDialog::queryParameters() const
{
    const auto source = m_source->currentData().toString();
    return { { "provider", source == "kde" ? "kde" : "github" },
             { "source", source },
             { "category", m_category->currentData().toString() },
             { "sort", m_sort->currentData().toString() } };
}
void ThemeDownloadDialog::query(bool refresh, int page, const QString& contentId)
{
    if (m_query && m_query->isRunning())
        m_query->abort();
    const auto generation = ++m_queryGeneration;
    auto params = queryParameters();
    params.insert("page", page);
    params.insert("refresh", refresh);
    if (!contentId.isEmpty()) {
        params.insert("mode", "files");
        params.insert("contentId", contentId);
    }
    m_status->setText(tr("正在刷新目录…"));
    m_query = makeShared<ThemeStoreTask>(APPLICATION->network().get(), params, m_service->cacheDirectory());
    connect(m_query.get(), &Task::finished, this, [this, generation, params, raw = m_query.get()] {
        if (generation != m_queryGeneration)
            return;
        if (!raw->wasSuccessful()) {
            m_status->setText(tr("刷新失败，保留已有列表：%1").arg(raw->failReason()));
            return;
        }
        const auto result = raw->result();
        m_loadedQuery = params;
        m_page = result.value("page").toInt();
        const auto selected = m_selected.value("id").toString();
        m_thumbnails->reset();
        m_online->blockSignals(true);
        m_online->clear();
        for (const auto& value : result.value("items").toArray()) {
            const auto item = value.toObject();
            auto row = new QListWidgetItem(item.value("name").toString(), m_online);
            row->setData(Qt::UserRole, item);
            row->setData(thumbnailStatusRole, tr("正在加载预览…"));
            row->setToolTip(item.value("name").toString() + '\n' + item.value("compatibility").toString());
            const QPersistentModelIndex index(m_online->model()->index(m_online->row(row), 0));
            m_thumbnails->load(previewUrl(item), [this, index](const QImage& image) {
                if (!index.isValid())
                    return;
                m_online->model()->setData(index, QPixmap::fromImage(image), Qt::DecorationRole);
                m_online->model()->setData(index, image.isNull() ? tr("暂无预览图") : QString(), thumbnailStatusRole);
            });
            if (item.value("id") == selected)
                m_online->setCurrentItem(row);
        }
        m_online->blockSignals(false);
        m_status->setText(tr("第 %1 页 · 本页 %2 项 / 共 %3 项 · %4%5")
                              .arg(m_page + 1)
                              .arg(result.value("items").toArray().size())
                              .arg(result.value("total").toInt(result.value("items").toArray().size()))
                              .arg(result.value("offline").toBool() ? tr("离线缓存 · ") : QString())
                              .arg(result.value("cachedAt").toString()));
        m_previous->setEnabled(m_page > 0);
        m_next->setEnabled(result.value("hasNext").toBool());
        filter();
        m_online->scrollToTop();
        showDetails();
    });
    m_query->start();
}
void ThemeDownloadDialog::filter()
{
    const auto text = m_search->text().trimmed();
    for (int i = 0; i < m_online->count(); ++i) {
        auto row = m_online->item(i);
        const auto data = row->data(Qt::UserRole).toJsonObject();
        row->setHidden(
            (m_compatible->isChecked() && !data.value("installable").toBool()) ||
            (!text.isEmpty() && !row->text().contains(text, Qt::CaseInsensitive) && !data.value("author").toString().contains(text, Qt::CaseInsensitive)));
    }
}
void ThemeDownloadDialog::showDetails()
{
    ++m_detailGeneration;
    if (m_details && m_details->isRunning())
        m_details->abort();
    m_selected = selection(m_online);
    m_files->clear();
    m_install->setEnabled(false);
    m_thumbnail->clear();
    if (m_selected.isEmpty()) {
        m_title->setText(tr("选择主题查看文件和兼容信息"));
        m_description->clear();
        return;
    }
    m_title->setText(m_selected.value("name").toString());
    m_description->setPlainText(
        tr("%1\n作者：%2\n版本：%3\n许可：%4\n\n%5")
            .arg(m_selected.value("compatibility").toString(), m_selected.value("author").toString(),
                 m_selected.value("version").toString(),
                 (m_selected.value("license").toString().isEmpty() ? tr("来源未提供许可") : m_selected.value("license").toString()),
                 m_selected.value("description").toString()));
    if (m_selected.value("provider") == "github") {
        const auto file = m_selected.value("file").toObject();
        m_files->addItem(file.value("name").toString(), file);
        m_files->setCurrentIndex(0);
    } else
        loadFiles(m_selected);
    const auto generation = m_detailGeneration;
    m_thumbnail->setText(tr("正在加载预览…"));
    m_thumbnails->load(previewUrl(m_selected), [this, generation](const QImage& image) {
        if (generation != m_detailGeneration)
            return;
        if (image.isNull())
            m_thumbnail->setText(tr("暂无预览图"));
        else {
            m_thumbnail->setPixmap(QPixmap::fromImage(image.scaled(300, 170, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            m_thumbnail->setToolTip(tr("作者截图；本机效果请安装后使用独立预览。"));
        }
    }, true);
}
void ThemeDownloadDialog::loadFiles(const QJsonObject& identity)
{
    auto request = m_loadedQuery;
    request.insert("mode", "files");
    request.insert("contentId", identity.value("id"));
    request.insert("category", identity.value("category"));
    m_files->setPlaceholderText(tr("正在读取文件…"));
    const auto generation = m_detailGeneration;
    m_details = makeShared<ThemeStoreTask>(APPLICATION->network().get(), request, m_service->cacheDirectory());
    connect(m_details.get(), &Task::finished, this, [this, generation, raw = m_details.get()] {
        if (generation != m_detailGeneration)
            return;
        if (!raw->wasSuccessful()) {
            m_description->append(tr("文件列表读取失败：%1").arg(raw->failReason()).toHtmlEscaped());
            return;
        }
        m_files->clear();
        for (const auto& value : raw->result().value("files").toArray()) {
            const auto file = value.toObject();
            auto label = file.value("name").toString();
            if (file.contains("sizeKiB"))
                label += QString(" · %1 KiB").arg(file.value("sizeKiB").toInteger());
            m_files->addItem(label, file);
        }
        if (m_files->count())
            m_files->setCurrentIndex(0);
        if (!m_files->count())
            m_files->setPlaceholderText(tr("没有可下载的文件"));
    });
    m_details->start();
}
void ThemeDownloadDialog::installSelected()
{
    const auto file = m_files->currentData().toJsonObject();
    if (!file.value("installable").toBool())
        return;
    auto request = m_loadedQuery;
    request.remove("page");
    request.remove("refresh");
    request.remove("sort");
    request.insert("contentId", m_selected.value("id"));
    request.insert("fileId", file.value("id"));
    request.insert("name", file.value("name"));
    request.insert("title", m_selected.value("name"));
    request.insert("version", file.value("version"));
    QJsonArray allowed;
    for (auto it = m_allowed.begin(); it != m_allowed.end(); ++it)
        if (it.value()->isChecked())
            allowed.append(it.key());
    if (request.value("category") == "bundle")
        allowed.append("bundle");
    if (allowed.isEmpty()) {
        m_status->setText(tr("至少选择一种要导入的组件。"));
        return;
    }
    request.insert("components", allowed);
    startInstall(request);
}
void ThemeDownloadDialog::startInstall(QJsonObject request)
{
    auto task =
        makeShared<ThemeInstallTask>(APPLICATION->network().get(), request, m_service->packageDirectory(), m_service->cacheDirectory());
    m_service->retainTask(task);
    connect(task.get(), &Task::succeeded, m_service, [] { APPLICATION->themeManager()->refresh(); });
    task->start();
    m_navigation->setCurrentRow(3);
}
void ThemeDownloadDialog::importLocal(bool directory)
{
    const auto path =
        directory ? QFileDialog::getExistingDirectory(this, tr("导入主题文件夹"))
                  : QFileDialog::getOpenFileName(this, tr("导入主题文件"), {},
                                                 tr("主题文件 (*.zip *.tar *.gz *.xz *.bz2 *.zst *.colors *.qss *.css);;全部文件 (*)"));
    if (!path.isEmpty())
        startInstall({ { "path", path } });
}
void ThemeDownloadDialog::refreshInstalled()
{
    const auto previous = selection(m_installed).value("id").toString();
    m_installed->clear();
    QHash<QString, QTreeWidgetItem*> packages;
    for (const auto& value : m_service->packages()) {
        auto data = value.toObject();
        data.insert("packageId", data.value("id"));
        auto row = new QTreeWidgetItem(
            m_installed, { data.value("name").toString(), data.value("version").toString(), data.value("provider").toString() });
        row->setData(0, Qt::UserRole, data);
        packages.insert(data.value("id").toString(), row);
        if (data.value("id") == previous)
            m_installed->setCurrentItem(row);
    }
    auto legacy = new QTreeWidgetItem(m_installed, { tr("内置与原有 Prism 主题"), {}, {} });
    for (const auto& value : m_service->components()) {
        const auto data = value.toObject();
        auto row = new QTreeWidgetItem(
            packages.value(data.value("packageId").toString(), legacy),
            { data.value("name").toString(), kindName(data.value("kind").toString()) + " · " + data.value("format").toString(),
              m_service->isUsed(data.value("id").toString()) ? tr("已引用") : tr("可用") });
        row->setData(0, Qt::UserRole, data);
        if (data.value("id") == previous)
            m_installed->setCurrentItem(row);
        if (data.contains("unavailable"))
            row->setText(2, data.value("unavailable").toString());
        if (data.value("kind") == "bundle") {
            QString error;
            const auto resolved = m_service->packageConfiguration(data.value("id").toString(), error);
            row->setText(2, !error.isEmpty() ? error : resolved.value("complete").toBool() ? tr("组件齐全") : tr("需要补齐或选择组件"));
            for (const auto& value : data.value("dependencies").toArray()) {
                auto dependency = value.toObject();
                if (!dependency.value("supported").toBool())
                    continue;
                bool found = false;
                for (const auto& package : m_service->packages())
                    found |= package.toObject().value("provider") == dependency.value("provider") &&
                             package.toObject().value("contentId") == dependency.value("contentId");
                if (found)
                    continue;
                dependency.insert("dependency", true);
                dependency.insert("id", "dependency-" + dependency.value("contentId").toString());
                auto child = new QTreeWidgetItem(row, { kindName(dependency.value("category").toString()) + " · #" + dependency.value("contentId").toString(),
                                                       tr("商店依赖"), tr("尚未安装 · 选择后点击下载") });
                child->setData(0, Qt::UserRole, dependency);
            }
            row->setExpanded(true);
        }
    }
    for (auto row : packages)
        row->setExpanded(true);
    m_remove->setEnabled(!selection(m_installed).value("packageId").toString().isEmpty() ||
                         selection(m_installed).value("removable").toBool());
    const bool dependencySelected = selection(m_installed).value("dependency").toBool();
    m_update->setEnabled(m_remove->isEnabled() || dependencySelected);
    m_update->setText(dependencySelected ? tr("选择并下载此依赖…") : tr("检查来源更新"));
}
void ThemeDownloadDialog::configureSelected()
{
    const auto selected = selection(m_installed);
    if (selected.value("dependency").toBool()) {
        updateSelected();
        return;
    }
    QJsonObject configuration;
    QString error;
    if (selected.contains("packageId")) {
        const auto result = m_service->packageConfiguration(selected.value("id").toString(), error);
        if (!error.isEmpty()) {
            QMessageBox::warning(this, tr("无法组合主题"), error);
            return;
        }
        if (!result.value("missing").toArray().isEmpty()) {
            QStringList missing;
            for (const auto& value : result.value("missing").toArray()) {
                const auto component = value.toObject();
                missing.append(kindName(component.value("kind").toString()) + "：" + component.value("name").toString());
            }
            QMessageBox::information(this, tr("主题组件尚未齐全"),
                                     tr("此主题还缺少以下应用组件：\n%1\n\n请先下载包下列出的商店依赖，或从在线主题中安装对应组件。")
                                         .arg(missing.join('\n')));
            return;
        }
        configuration = result.value("configuration").toObject();
        const auto choices = result.value("choices").toObject();
        for (auto it = choices.begin(); it != choices.end(); ++it) {
            const auto entries = it.value().toArray();
            QStringList names;
            for (const auto& value : entries)
                names.append(value.toObject().value("name").toString() + " · " + value.toObject().value("id").toString());
            bool ok = false;
            const auto name = QInputDialog::getItem(this, tr("选择主题变体"), kindName(it.key()), names, 0, false, &ok);
            if (!ok)
                return;
            const auto entry = entries.at(names.indexOf(name)).toObject();
            configuration.insert(it.key() == "widgets" ? "theme" : it.key() == "decoration" ? "decorationTheme" : it.key(), entry.value("id"));
            if (it.key() == "widgets" || it.key() == "palette") {
                configuration.insert("mode", "independent");
                configuration.insert("colorMode", "fixed");
            }
            if (it.key() == "decoration")
                configuration.insert("decoration", "luna");
        }
        const auto widgets = m_service->component(configuration.value("theme").toString());
        if (widgets.value("fixedColors").toBool() ||
            (widgets.value("format") == "qss-v2" && widgets.value("paletteBinding") != "semantic"))
            configuration.insert("palette", "");
    } else {
        configuration = m_service->state().value("requested").toObject();
        const auto kind = selected.value("kind").toString();
        if (kind == "widgets") {
            configuration.insert("mode", "independent");
            configuration.insert("theme", selected.value("id"));
            configuration.insert("palette", "");
            configuration.insert("colorMode", "fixed");
        } else if (kind == "icons" || kind == "background")
            configuration.insert(kind, selected.value("id"));
    }
    m_filling = true;
    for (auto it = m_options.begin(); it != m_options.end(); ++it) {
        const auto index = it.value()->findData(configuration.value(it.key()).toString());
        if (index >= 0)
            it.value()->setCurrentIndex(index);
    }
    m_filling = false;
    m_baseRevision = m_service->state().value("revision").toInteger();
    m_titlebarPalette->setChecked(configuration.value("systemTitlebarPalette").toBool());
    m_dirty = true;
    m_navigation->setCurrentRow(2);
    updateState();
    m_draftStatus->setText(tr("已载入此包的组件组合，请预览后应用。"));
}
void ThemeDownloadDialog::removeSelected()
{
    const auto selected = selection(m_installed);
    const auto id = selected.value("packageId").toString();
    if (selected.value("id").toString().isEmpty())
        return;
    if (QMessageBox::question(this, tr("卸载主题包"), tr("卸载此包及其组件？正在使用或等待重启的组件受到保护。")) != QMessageBox::Yes)
        return;
    QString error;
    if (!(id.isEmpty() ? m_service->removeLegacyComponent(selected.value("kind").toString(), selected.value("id").toString(), error)
                       : m_service->removePackage(id, error)))
        QMessageBox::warning(this, tr("无法卸载"), error);
}
void ThemeDownloadDialog::updateSelected()
{
    auto s = selection(m_installed);
    if (!s.contains("provider"))
        for (const auto& value : m_service->packages())
            if (value.toObject().value("id") == s.value("packageId"))
                s = value.toObject();
    if (s.value("provider") != "kde" && s.value("provider") != "github") {
        m_status->setText(tr("本地主题请重新导入新版文件。"));
        return;
    }
    m_filling = true;
    const auto savedSource = s.value("provider") == "kde" ? QString("kde") : s.value("source").toString();
    int source = m_source->findData(savedSource);
    if (source < 0 && !ThemeStoreTask::githubRepository(savedSource).isEmpty()) {
        m_source->addItem("GitHub · " + ThemeStoreTask::githubRepository(savedSource), savedSource);
        source = m_source->count() - 1;
    }
    if (source >= 0)
        m_source->setCurrentIndex(source);
    const int category = m_category->findData(s.value("category"));
    if (category >= 0)
        m_category->setCurrentIndex(category);
    m_filling = false;
    m_category->setEnabled(s.value("provider") == "kde");
    m_sort->setEnabled(s.value("provider") == "kde");
    m_navigation->setCurrentRow(0);
    m_search->setText(s.value("provider") == "github" ? s.value("fileName").toString() : QString());
    query(true, 0, s.value("provider") == "kde" ? s.value("contentId").toString() : QString());
}
QJsonObject ThemeDownloadDialog::draft() const
{
    QJsonObject result;
    for (auto it = m_options.begin(); it != m_options.end(); ++it)
        result.insert(it.key(), it.value()->currentData().toString());
    result.insert("systemTitlebarPalette", m_titlebarPalette->isChecked());
    return result;
}
void ThemeDownloadDialog::refreshChoices(bool fromState)
{
    auto config = fromState ? m_service->state().value("requested").toObject() : draft();
    if (config.isEmpty())
        config = m_service->state().value("requested").toObject();
    m_filling = true;
    for (auto combo : m_options)
        combo->clear();
    m_options["mode"]->addItem(tr("跟随系统外观"), "system");
    m_options["mode"]->addItem(tr("独立外观"), "independent");
    m_options["colorMode"]->addItem(tr("固定配色"), "fixed");
    m_options["colorMode"]->addItem(tr("随系统切换明暗"), "system");
    m_options["decoration"]->addItem(tr("系统窗口装饰"), "system");
    if (m_service->capabilities().value("customDecoration").toBool())
        m_options["decoration"]->addItem(tr("Luna 原生装饰"), "luna");
    for (const auto& key : { "palette", "lightPalette", "darkPalette" })
        m_options[key]->addItem(tr("使用控件主题的原始配色"), "");
    m_options["decorationTheme"]->addItem(tr("Luna 默认装饰"), "");
    for (const auto& value : m_service->components()) {
        const auto data = value.toObject();
        const auto kind = data.value("kind").toString(), name = data.value("name").toString(), id = data.value("id").toString();
        if (data.contains("unavailable"))
            continue;
        if (kind == "widgets")
            m_options["theme"]->addItem(name, id);
        else if (kind == "palette")
            for (const auto& key : { "palette", "lightPalette", "darkPalette" })
                m_options[key]->addItem(name, id);
        else if (kind == "decoration")
            m_options["decorationTheme"]->addItem(name, id);
        else if (m_options.contains(kind))
            m_options[kind]->addItem(name, id);
    }
    for (auto it = m_options.begin(); it != m_options.end(); ++it) {
        const auto id = config.value(it.key()).toString();
        int index = it.value()->findData(id);
        if (index < 0) {
            it.value()->addItem(tr("缺失组件：%1").arg(id), id);
            index = it.value()->count() - 1;
        }
        it.value()->setCurrentIndex(index);
    }
    m_titlebarPalette->setChecked(config.value("systemTitlebarPalette").toBool());
    if (fromState)
        m_baseRevision = m_service->state().value("revision").toInteger();
    m_filling = false;
    updateState();
}
QString ThemeDownloadDialog::describe(const QJsonObject& config) const
{
    auto name = [this, &config](const QString& key) {
        const auto id = config.value(key).toString();
        return m_service->component(id).value("name").toString(id);
    };
    return (config.value("mode") == "system" ? tr("系统外观") : name("theme")) + " · " +
           (config.value("palette").toString().isEmpty() ? tr("原始配色") : name("palette")) + " · " + name("icons") + " · " +
           (config.value("decoration") == "luna" ? tr("Luna 装饰") : tr("系统装饰"));
}
void ThemeDownloadDialog::updateState()
{
    const auto state = m_service->state();
    auto text = tr("当前生效：%1").arg(describe(state.value("effective").toObject()));
    if (state.value("requiresRestart").toBool())
        text += "\n" + tr("下次启动生效：%1").arg(describe(state.value("requested").toObject()));
    if (!state.value("error").toString().isEmpty())
        text += "\n" + state.value("error").toString();
    m_summary->setText(text);
    m_stateLabel->setText(text);
    m_undo->setEnabled(state.value("requiresRestart").toBool());
    const bool independent = m_options["mode"]->currentData() == "independent",
               automatic = m_options["colorMode"]->currentData() == "system";
    for (const auto& key : { "theme", "colorMode" })
        m_options[key]->setEnabled(independent);
    m_options["palette"]->setEnabled(independent && !automatic);
    for (const auto& key : { "lightPalette", "darkPalette" })
        m_options[key]->setEnabled(independent && automatic);
    m_options["decorationTheme"]->setEnabled(m_options["decoration"]->currentData() == "luna");
    if (m_dirty && m_baseRevision != state.value("revision").toInteger())
        m_draftStatus->setText(tr("外观已在其他位置修改。草稿已保留，请重载已保存组合后重新审阅。"));
}
Task* ThemeDownloadDialog::selectedTask() const
{
    const auto id = selection(m_tasks).value("operationId").toString();
    for (const auto& task : m_service->tasks())
        if (task->getUid().toString() == id)
            return task.get();
    return nullptr;
}
void ThemeDownloadDialog::addTask(Task* task)
{
    auto row = new QTreeWidgetItem(m_tasks);
    row->setData(0, Qt::UserRole, QJsonObject{ { "operationId", task->getUid().toString() } });
    auto update = [this, task, row] {
        row->setText(0, task->getStatus());
        row->setText(
            1, task->getTotalProgress() > 0 ? QString::number(task->getProgress() * 100 / task->getTotalProgress()) + '%' : tr("处理中"));
        row->setText(2, task->wasSuccessful()                            ? tr("已安装，可在外观组合中使用")
                        : task->isRunning()                              ? tr("进行中")
                        : task->getState() == Task::State::AbortedByUser ? tr("已取消")
                        : task->isFinished()                             ? task->failReason()
                                                                         : tr("等待"));
        row->setToolTip(2, task->failReason());
        if (m_tasks->currentItem() == row) {
            m_cancel->setEnabled(task->isRunning() && task->canAbort());
            m_retry->setEnabled(task->isFinished() && !task->wasSuccessful());
        }
    };
    connect(task, &Task::status, this, update);
    connect(task, &Task::progress, this, update);
    connect(task, &Task::finished, this, update);
    connect(task, &QObject::destroyed, this, [row] { delete row; });
    update();
    m_tasks->setCurrentItem(row);
}
