// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Prism Launcher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */
#include "ThemeDownloadDialog.h"

#include <algorithm>

#include <QComboBox>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "Application.h"
#include "FileSystem.h"
#include "MMCZip.h"
#include "StringUtils.h"
#include "archive/ArchiveReader.h"
#include "net/Download.h"
#include "net/NetJob.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/themes/ThemeManager.h"

namespace {
constexpr auto OFFICIAL_SOURCE_URL = "https://github.com/PrismLauncher/Themes";

struct RepositorySource {
    QString url;
    QString owner;
    QString repository;
};

bool parseRepositorySource(const QString& value, RepositorySource& source)
{
    const auto input = QUrl::fromUserInput(value.trimmed());
    if (!input.isValid() || input.scheme() != "https" || input.host().compare("github.com", Qt::CaseInsensitive) != 0) {
        return false;
    }

    const auto parts = input.path().split('/', Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return false;
    }

    auto repository = parts.at(1);
    if (repository.endsWith(".git", Qt::CaseInsensitive)) {
        repository.chop(4);
    }

    static const QRegularExpression ownerPattern("^[A-Za-z0-9][A-Za-z0-9-]*$");
    static const QRegularExpression repositoryPattern("^[A-Za-z0-9_.-]+$");
    if (!ownerPattern.match(parts.at(0)).hasMatch() || !repositoryPattern.match(repository).hasMatch()) {
        return false;
    }

    QUrl normalized;
    normalized.setScheme("https");
    normalized.setHost("github.com");
    normalized.setPath('/' + parts.at(0) + '/' + repository);

    source = { normalized.toString(), parts.at(0), repository };
    return true;
}

QStringList normalizedSources(const QStringList& values)
{
    QStringList result;
    for (const auto& value : values) {
        RepositorySource source;
        if (!parseRepositorySource(value, source)) {
            continue;
        }

        const auto duplicate = std::any_of(result.cbegin(), result.cend(), [&source](const QString& existing) {
            return existing.compare(source.url, Qt::CaseInsensitive) == 0;
        });
        if (!duplicate) {
            result.append(source.url);
        }
    }
    return result;
}

enum AssetRole {
    UrlRole = Qt::UserRole,
    FileNameRole,
    KindRole,
    SizeRole,
    DigestRole,
};

QString sha256ForFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        hash.addData(file.read(1024 * 1024));
    }
    return QString::fromLatin1(hash.result().toHex());
}
}  // namespace

ThemeDownloadDialog::ThemeDownloadDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Download Themes"));
    resize(680, 500);

    auto mainLayout = new QVBoxLayout(this);

    auto sourceLayout = new QHBoxLayout();
    sourceLayout->addWidget(new QLabel(tr("Source:"), this));
    m_sourceCombo = new QComboBox(this);
    sourceLayout->addWidget(m_sourceCombo, 1);

    auto manageSourcesButton = new QToolButton(this);
    manageSourcesButton->setIcon(QIcon::fromTheme("configure", style()->standardIcon(QStyle::SP_FileDialogDetailedView)));
    manageSourcesButton->setToolTip(tr("Manage Sources"));
    sourceLayout->addWidget(manageSourcesButton);

    auto refreshButton = new QToolButton(this);
    refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    refreshButton->setToolTip(tr("Refresh"));
    sourceLayout->addWidget(refreshButton);
    mainLayout->addLayout(sourceLayout);

    m_releaseLabel = new QLabel(tr("Loading release..."), this);
    mainLayout->addWidget(m_releaseLabel);

    auto searchLayout = new QHBoxLayout();
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Search"));
    m_searchBox->setClearButtonEnabled(true);
    searchLayout->addWidget(m_searchBox);

    mainLayout->addLayout(searchLayout);

    m_assetList = new QTreeWidget(this);
    m_assetList->setAlternatingRowColors(true);
    m_assetList->setRootIsDecorated(false);
    m_assetList->setUniformRowHeights(true);
    m_assetList->setHeaderLabels({ tr("Name"), tr("Type"), tr("Size") });
    m_assetList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_assetList->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_assetList->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mainLayout->addWidget(m_assetList);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto selectAllButton = buttonBox->addButton(tr("Select All"), QDialogButtonBox::ActionRole);
    m_installButton = buttonBox->addButton(tr("Install"), QDialogButtonBox::ActionRole);
    m_installButton->setIcon(QIcon::fromTheme("download"));
    m_installButton->setEnabled(false);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(refreshButton, &QToolButton::clicked, this, &ThemeDownloadDialog::loadRelease);
    connect(manageSourcesButton, &QToolButton::clicked, this, &ThemeDownloadDialog::manageSources);
    connect(selectAllButton, &QPushButton::clicked, this, &ThemeDownloadDialog::selectAllVisible);
    connect(m_installButton, &QPushButton::clicked, this, &ThemeDownloadDialog::installChecked);
    connect(m_searchBox, &QLineEdit::textChanged, this, &ThemeDownloadDialog::filterAssets);
    connect(m_assetList, &QTreeWidget::itemChanged, this, &ThemeDownloadDialog::updateInstallButton);
    connect(m_sourceCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) {
            return;
        }
        APPLICATION->settings()->set("ThemeDownloadSource", m_sourceCombo->itemData(index).toString());
        if (m_loadStarted) {
            loadRelease();
        }
    });

    reloadSources();
}

void ThemeDownloadDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (!m_loadStarted) {
        m_loadStarted = true;
        QTimer::singleShot(0, this, &ThemeDownloadDialog::loadRelease);
    }
}

bool ThemeDownloadDialog::assetKind(const QString& fileName, AssetKind& kind)
{
    if (fileName.endsWith("-theme.zip", Qt::CaseInsensitive)) {
        kind = AssetKind::Theme;
    } else if (fileName.endsWith("-icons.zip", Qt::CaseInsensitive)) {
        kind = AssetKind::Icons;
    } else if (fileName.endsWith("-catpack.zip", Qt::CaseInsensitive)) {
        kind = AssetKind::CatPack;
    } else {
        return false;
    }
    return true;
}

QString ThemeDownloadDialog::assetName(const QString& fileName, AssetKind kind)
{
    QString suffix;
    switch (kind) {
        case AssetKind::Theme:
            suffix = "-theme.zip";
            break;
        case AssetKind::Icons:
            suffix = "-icons.zip";
            break;
        case AssetKind::CatPack:
            suffix = "-catpack.zip";
            break;
    }
    return fileName.left(fileName.size() - suffix.size());
}

QString ThemeDownloadDialog::assetTypeName(AssetKind kind)
{
    switch (kind) {
        case AssetKind::Theme:
            return tr("Theme");
        case AssetKind::Icons:
            return tr("Icons");
        case AssetKind::CatPack:
            return tr("Cat Pack");
    }
    return {};
}

QString ThemeDownloadDialog::archivePrefix(AssetKind kind)
{
    switch (kind) {
        case AssetKind::Theme:
            return "themes/";
        case AssetKind::Icons:
            return "icons/";
        case AssetKind::CatPack:
            return "cats/";
    }
    return {};
}

QString ThemeDownloadDialog::installPath(AssetKind kind)
{
    switch (kind) {
        case AssetKind::Theme:
            return APPLICATION->themeManager()->getApplicationThemesFolder().absolutePath();
        case AssetKind::Icons:
            return APPLICATION->themeManager()->getIconThemesFolder().absolutePath();
        case AssetKind::CatPack:
            return APPLICATION->themeManager()->getCatPacksFolder().absolutePath();
    }
    return {};
}

void ThemeDownloadDialog::reloadSources(const QString& preferredSource)
{
    auto settings = APPLICATION->settings();
    auto sources = normalizedSources(settings->get("ThemeDownloadSources").toStringList());
    if (sources.isEmpty()) {
        sources.append(OFFICIAL_SOURCE_URL);
    }
    settings->set("ThemeDownloadSources", sources);

    RepositorySource preferred;
    const auto requestedSource = preferredSource.isEmpty() ? settings->get("ThemeDownloadSource").toString() : preferredSource;
    parseRepositorySource(requestedSource, preferred);

    const QSignalBlocker blocker(m_sourceCombo);
    m_sourceCombo->clear();
    for (const auto& url : sources) {
        RepositorySource source;
        parseRepositorySource(url, source);
        m_sourceCombo->addItem(source.owner + '/' + source.repository, source.url);
        m_sourceCombo->setItemData(m_sourceCombo->count() - 1, source.url, Qt::ToolTipRole);
    }

    auto selectedIndex = m_sourceCombo->findData(preferred.url);
    if (selectedIndex < 0) {
        selectedIndex = 0;
    }
    m_sourceCombo->setCurrentIndex(selectedIndex);
    settings->set("ThemeDownloadSource", m_sourceCombo->currentData().toString());
}

void ThemeDownloadDialog::manageSources()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Manage Theme Sources"));
    dialog.resize(560, 360);

    auto layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Add GitHub repositories that publish compatible theme packages in their latest release."), &dialog));

    auto sourceList = new QListWidget(&dialog);
    sourceList->addItems(normalizedSources(APPLICATION->settings()->get("ThemeDownloadSources").toStringList()));
    sourceList->setSelectionMode(QAbstractItemView::SingleSelection);
    sourceList->setCurrentRow(0);
    layout->addWidget(sourceList);

    auto actionLayout = new QHBoxLayout();
    auto addButton = new QPushButton(QIcon::fromTheme("list-add"), tr("Add..."), &dialog);
    auto editButton = new QPushButton(QIcon::fromTheme("document-edit"), tr("Edit..."), &dialog);
    auto removeButton = new QPushButton(QIcon::fromTheme("list-remove"), tr("Remove"), &dialog);
    auto defaultsButton = new QPushButton(tr("Restore Default"), &dialog);
    actionLayout->addWidget(addButton);
    actionLayout->addWidget(editButton);
    actionLayout->addWidget(removeButton);
    actionLayout->addStretch();
    actionLayout->addWidget(defaultsButton);
    layout->addLayout(actionLayout);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);

    auto updateButtons = [sourceList, editButton, removeButton] {
        const auto hasSelection = sourceList->currentRow() >= 0;
        editButton->setEnabled(hasSelection);
        removeButton->setEnabled(hasSelection && sourceList->count() > 1);
    };
    auto promptForSource = [&dialog](const QString& current, const QString& title) {
        while (true) {
            bool accepted = false;
            const auto value = QInputDialog::getText(&dialog, title, tr("GitHub repository URL:"), QLineEdit::Normal, current, &accepted);
            if (!accepted) {
                return QString();
            }

            RepositorySource source;
            if (parseRepositorySource(value, source)) {
                return source.url;
            }
            QMessageBox::warning(&dialog, title, tr("Enter a valid HTTPS GitHub repository URL."));
        }
    };
    auto containsSource = [sourceList](const QString& url, int ignoredRow = -1) {
        for (int i = 0; i < sourceList->count(); ++i) {
            if (i != ignoredRow && sourceList->item(i)->text().compare(url, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    };
    auto editCurrent = [&] {
        const auto row = sourceList->currentRow();
        if (row < 0) {
            return;
        }
        const auto source = promptForSource(sourceList->item(row)->text(), tr("Edit Theme Source"));
        if (source.isNull()) {
            return;
        }
        if (containsSource(source, row)) {
            QMessageBox::warning(&dialog, tr("Edit Theme Source"), tr("That source is already in the list."));
            return;
        }
        sourceList->item(row)->setText(source);
    };

    connect(addButton, &QPushButton::clicked, &dialog, [&] {
        const auto source = promptForSource({}, tr("Add Theme Source"));
        if (source.isNull()) {
            return;
        }
        if (containsSource(source)) {
            QMessageBox::warning(&dialog, tr("Add Theme Source"), tr("That source is already in the list."));
            return;
        }
        sourceList->addItem(source);
        sourceList->setCurrentRow(sourceList->count() - 1);
    });
    connect(editButton, &QPushButton::clicked, &dialog, editCurrent);
    connect(sourceList, &QListWidget::itemDoubleClicked, &dialog, editCurrent);
    connect(removeButton, &QPushButton::clicked, &dialog, [sourceList] { delete sourceList->takeItem(sourceList->currentRow()); });
    connect(defaultsButton, &QPushButton::clicked, &dialog, [sourceList] {
        sourceList->clear();
        sourceList->addItem(OFFICIAL_SOURCE_URL);
        sourceList->setCurrentRow(0);
    });
    connect(sourceList, &QListWidget::currentRowChanged, &dialog, updateButtons);
    connect(sourceList->model(), &QAbstractItemModel::rowsInserted, &dialog, updateButtons);
    connect(sourceList->model(), &QAbstractItemModel::rowsRemoved, &dialog, updateButtons);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    updateButtons();

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList sources;
    for (int i = 0; i < sourceList->count(); ++i) {
        sources.append(sourceList->item(i)->text());
    }
    APPLICATION->settings()->set("ThemeDownloadSources", sources);
    const auto preferred = sourceList->currentItem() ? sourceList->currentItem()->text() : QString();
    reloadSources(preferred);
    if (m_loadStarted) {
        loadRelease();
    }
}

void ThemeDownloadDialog::loadRelease()
{
    m_releaseLabel->setText(tr("Loading release..."));
    m_assetList->clear();
    m_installButton->setEnabled(false);

    int assetCount = 0;
    QString tag;
    RepositorySource source;
    if (!parseRepositorySource(m_sourceCombo->currentData().toString(), source)) {
        m_releaseLabel->setText(tr("No valid source selected"));
        return;
    }

    const auto repositoryPath = '/' + source.owner + '/' + source.repository;
    QUrl apiUrl;
    apiUrl.setScheme("https");
    apiUrl.setHost("api.github.com");
    apiUrl.setPath("/repos" + repositoryPath + "/releases/latest");

    QUrl releasePageUrl;
    releasePageUrl.setScheme("https");
    releasePageUrl.setHost("github.com");
    releasePageUrl.setPath(repositoryPath + "/releases/latest");

    auto fetch = [this](const QUrl& url, const QString& jobName, QByteArray& output) {
        auto [download, response] = Net::Download::makeByteArray(url);
        NetJob job(jobName, APPLICATION->network());
        job.setAskRetry(false);
        job.addNetAction(download);

        ProgressDialog progress(this);
        if (progress.execWithTask(&job) != QDialog::Accepted) {
            return false;
        }
        output = *response;
        return true;
    };

    auto addAsset = [this, &assetCount](const QString& fileName, const QString& url, qint64 size, const QString& sizeText,
                                        const QString& digest) {
        AssetKind kind;
        if (!assetKind(fileName, kind) || url.isEmpty() || size == 0) {
            return;
        }

        auto item = new QTreeWidgetItem(m_assetList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Unchecked);
        item->setText(0, assetName(fileName, kind));
        item->setText(1, assetTypeName(kind));
        item->setText(2, sizeText);
        item->setData(0, UrlRole, url);
        item->setData(0, FileNameRole, fileName);
        item->setData(0, KindRole, static_cast<int>(kind));
        item->setData(0, SizeRole, size);
        item->setData(0, DigestRole, digest);
        ++assetCount;
    };

    m_assetList->blockSignals(true);

    QByteArray releaseData;
    if (fetch(apiUrl, tr("Download theme list"), releaseData)) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(releaseData, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const auto release = document.object();
            tag = release.value("tag_name").toString();
            for (const auto& value : release.value("assets").toArray()) {
                const auto asset = value.toObject();
                const auto size = asset.value("size").toInteger();
                addAsset(asset.value("name").toString(), asset.value("browser_download_url").toString(), size,
                         StringUtils::humanReadableFileSize(size), asset.value("digest").toString());
            }
        }
    }

    // GitHub's unauthenticated REST limit is shared by every user behind the same public IP.
    if (assetCount == 0) {
        QByteArray releasePage;
        QByteArray assetsPage;
        const QRegularExpression expandedAssetsPattern(
            QString(R"((?:https://github\.com)?(%1/releases/expanded_assets/[^"\s<]+))").arg(QRegularExpression::escape(repositoryPath)));

        if (fetch(releasePageUrl, tr("Download theme list"), releasePage)) {
            const auto match = expandedAssetsPattern.match(QString::fromUtf8(releasePage));
            if (match.hasMatch()) {
                const QUrl assetsUrl = QUrl("https://github.com").resolved(QUrl(match.captured(1)));
                tag = assetsUrl.fileName();
                if (assetsUrl.host() == "github.com" && assetsUrl.path().startsWith(repositoryPath + "/releases/expanded_assets/") &&
                    fetch(assetsUrl, tr("Download theme list"), assetsPage)) {
                    const QRegularExpression rowPattern(R"(<li\b[^>]*class="[^"]*Box-row[^"]*"[^>]*>(.*?)</li>)",
                                                        QRegularExpression::DotMatchesEverythingOption);
                    const QRegularExpression linkPattern(
                        QString(R"regex(href="(%1/releases/download/[^"]+)")regex").arg(QRegularExpression::escape(repositoryPath)));
                    const QRegularExpression digestPattern(R"(sha256:([0-9a-fA-F]{64}))");
                    const QRegularExpression sizePattern(R"(>([0-9]+(?:\.[0-9]+)?\s+(?:Bytes|KB|MB|GB))</span>)");

                    auto rows = rowPattern.globalMatch(QString::fromUtf8(assetsPage));
                    while (rows.hasNext()) {
                        const auto row = rows.next().captured(1);
                        const auto linkMatch = linkPattern.match(row);
                        if (!linkMatch.hasMatch()) {
                            continue;
                        }

                        const auto relativeUrl = linkMatch.captured(1);
                        const auto fileName = QUrl::fromPercentEncoding(relativeUrl.section('/', -1).toUtf8());
                        const auto digestMatch = digestPattern.match(row);
                        const auto sizeMatch = sizePattern.match(row);
                        addAsset(fileName, QUrl("https://github.com").resolved(QUrl(relativeUrl)).toString(), -1,
                                 sizeMatch.hasMatch() ? sizeMatch.captured(1) : tr("Unknown"),
                                 digestMatch.hasMatch() ? "sha256:" + digestMatch.captured(1) : QString());
                    }
                }
            }
        }
    }

    m_assetList->blockSignals(false);

    if (assetCount == 0) {
        m_releaseLabel->setText(tr("Failed to load themes"));
        QMessageBox::warning(this, tr("Download Themes"), tr("Could not load the latest theme release from %1.").arg(source.url));
        return;
    }

    m_releaseLabel->setText(tag.isEmpty() ? tr("Latest release - %n package(s)", nullptr, assetCount)
                                          : tr("Release %1 - %n package(s)", nullptr, assetCount).arg(tag));
    filterAssets(m_searchBox->text());
}

void ThemeDownloadDialog::filterAssets(const QString& text)
{
    for (int i = 0; i < m_assetList->topLevelItemCount(); ++i) {
        auto item = m_assetList->topLevelItem(i);
        item->setHidden(!item->text(0).contains(text, Qt::CaseInsensitive) && !item->text(1).contains(text, Qt::CaseInsensitive));
    }
}

void ThemeDownloadDialog::selectAllVisible()
{
    m_assetList->blockSignals(true);
    for (int i = 0; i < m_assetList->topLevelItemCount(); ++i) {
        auto item = m_assetList->topLevelItem(i);
        if (!item->isHidden()) {
            item->setCheckState(0, Qt::Checked);
        }
    }
    m_assetList->blockSignals(false);
    updateInstallButton();
}

void ThemeDownloadDialog::updateInstallButton()
{
    for (int i = 0; i < m_assetList->topLevelItemCount(); ++i) {
        if (m_assetList->topLevelItem(i)->checkState(0) == Qt::Checked) {
            m_installButton->setEnabled(true);
            return;
        }
    }
    m_installButton->setEnabled(false);
}

bool ThemeDownloadDialog::validateArchive(const QString& path, AssetKind kind, QString& error) const
{
    MMCZip::ArchiveReader archive(path);
    if (!archive.collectFiles()) {
        error = tr("The downloaded file is not a readable ZIP archive.");
        return false;
    }

    const auto files = archive.getFiles();
    const auto prefix = archivePrefix(kind);
    bool manifestFound = false;

    for (const auto& rawPath : files) {
        const auto normalized = QDir::fromNativeSeparators(rawPath);
        if (!normalized.startsWith(prefix) || normalized.startsWith('/') || normalized.split('/').contains("..")) {
            error = tr("The ZIP archive contains files outside its expected folder.");
            return false;
        }

        if (kind == AssetKind::Theme && QRegularExpression("^themes/[^/]+/theme\\.json$").match(normalized).hasMatch()) {
            manifestFound = true;
        } else if (kind == AssetKind::Icons && QRegularExpression("^icons/[^/]+/index\\.theme$").match(normalized).hasMatch()) {
            manifestFound = true;
        } else if (kind == AssetKind::CatPack && QRegularExpression("^cats/[^/]+/catpack\\.json$").match(normalized).hasMatch()) {
            manifestFound = true;
        }
    }

    if (files.isEmpty() || !manifestFound) {
        error = tr("The ZIP archive does not contain a valid theme package.");
        return false;
    }
    return true;
}

bool ThemeDownloadDialog::validateDownload(QTreeWidgetItem* item, const QString& path, QString& error) const
{
    const auto expectedSize = item->data(0, SizeRole).toLongLong();
    if (expectedSize > 0 && QFileInfo(path).size() != expectedSize) {
        error = tr("The downloaded file size does not match the release metadata.");
        return false;
    }

    const auto digest = item->data(0, DigestRole).toString();
    if (digest.startsWith("sha256:", Qt::CaseInsensitive)) {
        const auto expectedHash = digest.mid(7);
        const auto actualHash = sha256ForFile(path);
        if (actualHash.isEmpty() || actualHash.compare(expectedHash, Qt::CaseInsensitive) != 0) {
            error = tr("The downloaded file checksum does not match the release metadata.");
            return false;
        }
    }

    return validateArchive(path, static_cast<AssetKind>(item->data(0, KindRole).toInt()), error);
}

void ThemeDownloadDialog::installChecked()
{
    QList<QTreeWidgetItem*> selected;
    for (int i = 0; i < m_assetList->topLevelItemCount(); ++i) {
        auto item = m_assetList->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked) {
            selected.append(item);
        }
    }
    if (selected.isEmpty()) {
        return;
    }

    QTemporaryDir temporaryDir(QDir::tempPath() + "/prism-themes-XXXXXX");
    if (!temporaryDir.isValid()) {
        QMessageBox::warning(this, tr("Download Themes"), tr("Could not create a temporary download folder."));
        return;
    }

    NetJob job(tr("Download themes"), APPLICATION->network());
    for (auto item : selected) {
        const auto fileName = FS::RemoveInvalidFilenameChars(item->data(0, FileNameRole).toString());
        const auto path = temporaryDir.filePath(fileName);
        job.addNetAction(Net::Download::makeFile(QUrl(item->data(0, UrlRole).toString()), path));
    }

    ProgressDialog progress(this);
    if (progress.execWithTask(&job) != QDialog::Accepted) {
        return;
    }

    QStringList installed;
    QStringList failures;
    for (auto item : selected) {
        const auto fileName = FS::RemoveInvalidFilenameChars(item->data(0, FileNameRole).toString());
        const auto path = temporaryDir.filePath(fileName);
        const auto kind = static_cast<AssetKind>(item->data(0, KindRole).toInt());

        QString error;
        if (!validateDownload(item, path, error)) {
            failures.append(tr("%1: %2").arg(item->text(0), error));
            continue;
        }

        const auto target = installPath(kind);
        if (!FS::ensureFolderPathExists(target) || !MMCZip::extractDir(path, archivePrefix(kind), target).has_value()) {
            failures.append(tr("%1: Could not extract the package.").arg(item->text(0)));
            continue;
        }

        installed.append(item->text(0));
        item->setCheckState(0, Qt::Unchecked);
    }

    if (!installed.isEmpty()) {
        emit themesInstalled();
    }

    if (!failures.isEmpty()) {
        QMessageBox::warning(this, tr("Theme Installation"), tr("Some packages could not be installed:\n\n%1").arg(failures.join('\n')));
    } else {
        QMessageBox::information(this, tr("Theme Installation"), tr("Installed %n package(s).", nullptr, installed.size()));
    }
    updateInstallButton();
}
