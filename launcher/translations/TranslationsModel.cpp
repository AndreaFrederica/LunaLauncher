// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "TranslationsModel.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QLocale>
#include <QSet>
#include <QTranslator>
#include <locale>

#include "BuildConfig.h"
#include "FileSystem.h"
#include "Json.h"
#include "net/ChecksumValidator.h"
#include "net/NetJob.h"

#include "POTranslator.h"

#include "Application.h"
#include "settings/SettingsObject.h"

const static QLatin1String defaultLangCode("en_US");

enum class FileType { NONE, QM, PO };

struct Language {
    Language() { updated = true; }
    Language(const QString& _key)
    {
        key = _key;
        locale = QLocale(key);
        updated = (key == defaultLangCode);
    }

    QString languageName() const
    {
        QString result;
        if (key == "ja_KANJI") {
            result = locale.nativeLanguageName() + u8" (漢字)";
        } else if (key == "es_UY") {
            result = u8"Español de Latinoamérica";
        } else if (key == "en_NZ") {
            result = u8"New Zealand English";  // No idea why qt translates this to just english and not to New Zealand English
        } else if (key == "en@pirate") {
            result = u8"Tongue of the High Seas";
        } else if (key == "en@uwu") {
            result = u8"Cute Engwish";
        } else if (key == "tok") {
            result = u8"toki pona";
        } else if (key == "nan") {
            result = u8"閩南語";  // Using traditional Chinese script. Not sure if we should use simplified instead?
        } else {
            result = locale.nativeLanguageName();
        }

        if (result.isEmpty()) {
            result = key;
        }
        return result;
    }

    float percentTranslated() const
    {
        if (total == 0) {
            return 100.0f;
        }
        return 100.0f * float(translated) / float(total);
    }

    void setTranslationStats(unsigned _translated, unsigned _untranslated, unsigned _fuzzy)
    {
        translated = _translated;
        untranslated = _untranslated;
        fuzzy = _fuzzy;
        total = translated + untranslated + fuzzy;
    }

    bool isOfSameNameAs(const Language& other) const { return key == other.key; }

    bool isIdenticalTo(const Language& other) const
    {
        return (key == other.key && file_name == other.file_name && file_size == other.file_size && file_sha1 == other.file_sha1 &&
                translated == other.translated && fuzzy == other.fuzzy && total == other.fuzzy && localFileType == other.localFileType);
    }

    Language& apply(Language& other)
    {
        if (!isOfSameNameAs(other)) {
            return *this;
        }
        file_name = other.file_name;
        file_size = other.file_size;
        file_sha1 = other.file_sha1;
        translated = other.translated;
        fuzzy = other.fuzzy;
        total = other.total;
        localFileType = other.localFileType;
        return *this;
    }

    QString key;
    QLocale locale;
    bool updated;

    QString file_name = QString();
    std::size_t file_size = 0;
    QString file_sha1 = QString();

    unsigned translated = 0;
    unsigned untranslated = 0;
    unsigned fuzzy = 0;
    unsigned total = 0;

    FileType localFileType = FileType::NONE;
};

struct TranslationsModel::Private {
    QDir m_dir;
    QDir m_install_dir;

    // initial state is just english
    QList<Language> m_languages = { Language(defaultLangCode) };

    QString m_selectedLanguage = defaultLangCode;
    std::unique_ptr<QTranslator> m_qt_translator;
    std::unique_ptr<QTranslator> m_app_translator;

    Net::Download* m_index_task;
    QString m_downloadingTranslation;
    NetJob::Ptr m_dl_job;
    NetJob::Ptr m_index_job;
    QString m_nextDownload;

    std::unique_ptr<POTranslator> m_po_translator;
    QFileSystemWatcher* watcher;

    const QString m_system_locale = QLocale::system().name();
    const QString m_system_language = m_system_locale.split('_').front();

    bool no_language_set = false;
};

TranslationsModel::TranslationsModel(QString path, QObject* parent) : QAbstractListModel(parent)
{
    d.reset(new Private);
    d->m_dir.setPath(path);
    FS::ensureFolderPathExists(path);

    // Set install translations directory
    QString appDir = QCoreApplication::applicationDirPath();
    QString installPath;
#ifdef Q_OS_MACOS
    // On macOS, appDir is .app/Contents/MacOS, resources are in Resources
    installPath = FS::PathCombine(appDir, "..", "Resources", "translations");
#else
    // On Windows/Linux, resources are in resource subdirectory
    installPath = FS::PathCombine(appDir, "resource", "translations");
#endif
    d->m_install_dir.setPath(installPath);

    reloadLocalFiles();

    d->watcher = new QFileSystemWatcher(this);
    connect(d->watcher, &QFileSystemWatcher::directoryChanged, this, &TranslationsModel::translationDirChanged);
    d->watcher->addPath(d->m_dir.canonicalPath());
    if (d->m_install_dir.exists()) {
        d->watcher->addPath(d->m_install_dir.canonicalPath());
    }
}

TranslationsModel::~TranslationsModel() {}

void TranslationsModel::translationDirChanged(const QString& path)
{
    qDebug() << "Dir changed:" << path;
    if (!d->no_language_set) {
        reloadLocalFiles();
    }
    selectLanguage(selectedLanguage());
}

void TranslationsModel::indexReceived()
{
    qDebug() << "Got translations index!";
    d->m_index_job.reset();

    if (d->no_language_set) {
        reloadLocalFiles();

        auto language = d->m_system_locale;
        if (!findLanguageAsOptional(language).has_value()) {
            language = d->m_system_language;
        }
        selectLanguage(language);
        if (selectedLanguage() != defaultLangCode) {
            updateLanguage(selectedLanguage());
        }
        APPLICATION->settings()->set("Language", selectedLanguage());
        d->no_language_set = false;
    }

    else if (d->m_selectedLanguage != defaultLangCode) {
        downloadTranslation(d->m_selectedLanguage);
    }
}

namespace {
void readIndex(const QString& path, QMap<QString, Language>& languages)
{
    QByteArray data;
    try {
        data = FS::read(path);
    } catch ([[maybe_unused]] const Exception& e) {
        qCritical() << "Translations Download Failed: index file not readable";
        return;
    }

    try {
        auto toplevel_doc = Json::requireDocument(data);
        auto doc = Json::requireObject(toplevel_doc);
        auto file_type = Json::requireString(doc, "file_type");
        if (file_type != "MMC-TRANSLATION-INDEX") {
            qCritical() << "Translations Download Failed: index file is of unknown file type" << file_type;
            return;
        }
        auto version = Json::requireInteger(doc, "version");
        if (version > 2) {
            qCritical() << "Translations Download Failed: index file is of unknown format version" << file_type;
            return;
        }
        auto langObjs = Json::requireObject(doc, "languages");
        for (auto iter = langObjs.begin(); iter != langObjs.end(); iter++) {
            Language lang(iter.key());

            auto langObj = Json::requireObject(iter.value());
            lang.setTranslationStats(langObj["translated"].toInt(), langObj["untranslated"].toInt(), langObj["fuzzy"].toInt());
            lang.file_name = Json::requireString(langObj, "file");
            lang.file_sha1 = Json::requireString(langObj, "sha1");
            lang.file_size = Json::requireInteger(langObj, "size");

            languages.insert(lang.key, lang);
        }
    } catch ([[maybe_unused]] Json::JsonException& e) {
        qCritical() << "Translations Download Failed: index file could not be parsed as json";
    }
}
}  // namespace

void TranslationsModel::reloadLocalFiles()
{
    QMap<QString, Language> languages = { { defaultLangCode, Language(defaultLangCode) } };

    // Read index from user data directory
    readIndex(d->m_dir.absoluteFilePath("index_v2.json"), languages);

    // Collect translation files from both directories
    QList<QDir> translationDirs;
    translationDirs.append(d->m_dir);

    // Add install directory if it exists
    if (d->m_install_dir.exists()) {
        translationDirs.append(d->m_install_dir);
    }

    // Helper function to process a file entry
    auto processEntry = [&](const QFileInfo& entry) -> QPair<QString, QPair<FileType, bool>> {
        auto completeSuffix = entry.completeSuffix();
        QString fileName = entry.fileName();
        bool isPriority = false;

        // Check for priority prefix (!)
        if (fileName.startsWith('!')) {
            isPriority = true;
            fileName = fileName.mid(1);  // Remove the ! prefix
        }

        QString langCode;
        FileType fileType = FileType::NONE;
        if (completeSuffix == "qm") {
            if (fileName.startsWith("mmc_")) {
                langCode = fileName.mid(4);  // Remove "mmc_" prefix
            } else {
                return qMakePair(QString(), qMakePair(FileType::NONE, false));  // Invalid .qm file name
            }
            fileType = FileType::QM;
        } else if (completeSuffix == "po") {
            langCode = fileName;  // .po files use the whole filename as langCode
            fileType = FileType::PO;
        } else {
            return qMakePair(QString(), qMakePair(FileType::NONE, false));
        }

        return qMakePair(langCode, qMakePair(fileType, isPriority));
    };

    // Scan translation files from all directories
    // First pass: scan all files, collect priority (!) and normal files separately
    QList<QPair<QString, QPair<FileType, bool>>> priorityFiles;
    QMap<QString, QPair<FileType, bool>> normalFiles;

    for (const QDir& dir : translationDirs) {
        auto entries = dir.entryInfoList({ "mmc_*.qm", "*.po", "!*.po" }, QDir::Files | QDir::NoDotAndDotDot);
        for (auto& entry : entries) {
            auto result = processEntry(entry);
            if (result.first.isEmpty() || result.second.first == FileType::NONE)
                continue;

            auto langCode = result.first;
            auto fileType = result.second.first;
            bool isPriority = result.second.second;

            if (isPriority) {
                priorityFiles.append(qMakePair(langCode, qMakePair(fileType, true)));
            } else {
                // Only keep normal file if we don't have one yet (user dir processed first)
                if (!normalFiles.contains(langCode)) {
                    normalFiles.insert(langCode, qMakePair(fileType, false));
                }
            }
        }
    }

    // Second pass: process normal files
    for (auto iter = normalFiles.begin(); iter != normalFiles.end(); ++iter) {
        QString langCode = iter.key();
        auto fileType = iter.value().first;

        auto langIter = languages.find(langCode);
        if (langIter != languages.end()) {
            auto& language = *langIter;
            if (int(fileType) > int(language.localFileType)) {
                language.localFileType = fileType;
            }
        } else {
            if (fileType == FileType::PO) {
                Language localFound(langCode);
                localFound.localFileType = FileType::PO;
                languages.insert(langCode, localFound);
            }
        }
    }

    // Third pass: process priority (!) files - these override everything
    for (const auto& priorityFile : priorityFiles) {
        QString langCode = priorityFile.first;
        auto fileType = priorityFile.second.first;

        auto langIter = languages.find(langCode);
        if (langIter != languages.end()) {
            auto& language = *langIter;
            if (int(fileType) > int(language.localFileType)) {
                language.localFileType = fileType;
            }
        } else {
            if (fileType == FileType::PO) {
                Language localFound(langCode);
                localFound.localFileType = FileType::PO;
                languages.insert(langCode, localFound);
            }
        }
    }

    // changed and removed languages
    for (auto iter = d->m_languages.begin(); iter != d->m_languages.end();) {
        auto& language = *iter;
        auto row = iter - d->m_languages.begin();

        auto updatedLanguageIter = languages.find(language.key);
        if (updatedLanguageIter != languages.end()) {
            if (language.isIdenticalTo(*updatedLanguageIter)) {
                languages.remove(language.key);
            } else {
                language.apply(*updatedLanguageIter);
                emit dataChanged(index(row), index(row));
                languages.remove(language.key);
            }
            iter++;
        } else {
            beginRemoveRows(QModelIndex(), row, row);
            iter = d->m_languages.erase(iter);
            endRemoveRows();
        }
    }
    // added languages
    if (languages.isEmpty()) {
        return;
    }
    beginInsertRows(QModelIndex(), 0, d->m_languages.size() + languages.size() - 1);
    for (auto& language : languages) {
        d->m_languages.append(language);
    }
    std::sort(d->m_languages.begin(), d->m_languages.end(), [this](const Language& a, const Language& b) {
        if (a.key != b.key) {
            if (a.key == d->m_system_locale || a.key == d->m_system_language) {
                return true;
            }
            if (b.key == d->m_system_locale || b.key == d->m_system_language) {
                return false;
            }
        }
        return a.languageName().toLower() < b.languageName().toLower();
    });
    endInsertRows();
}

namespace {
enum class Column { Language, Completeness };
}

QVariant TranslationsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int row = index.row();
    auto column = static_cast<Column>(index.column());

    if (row < 0 || row >= d->m_languages.size())
        return QVariant();

    auto& lang = d->m_languages[row];
    switch (role) {
        case Qt::DisplayRole: {
            switch (column) {
                case Column::Language: {
                    return lang.languageName();
                }
                case Column::Completeness: {
                    return QString("%1%").arg(lang.percentTranslated(), 3, 'f', 1);
                }
            }
            qWarning("TranslationModel::data not implemented when role is DisplayRole");
        }
        case Qt::ToolTipRole: {
            return tr("%1:\n%2 translated\n%3 fuzzy\n%4 total")
                .arg(lang.key, QString::number(lang.translated), QString::number(lang.fuzzy), QString::number(lang.total));
        }
        case Qt::UserRole:
            return lang.key;
        default:
            return QVariant();
    }
}

QVariant TranslationsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    auto column = static_cast<Column>(section);
    if (role == Qt::DisplayRole) {
        switch (column) {
            case Column::Language: {
                return tr("Language");
            }
            case Column::Completeness: {
                return tr("Completeness");
            }
        }
    } else if (role == Qt::ToolTipRole) {
        switch (column) {
            case Column::Language: {
                return tr("The native language name.");
            }
            case Column::Completeness: {
                return tr("Completeness is the percentage of fully translated strings, not counting automatically guessed ones.");
            }
        }
    }
    return QAbstractListModel::headerData(section, orientation, role);
}

int TranslationsModel::rowCount([[maybe_unused]] const QModelIndex& parent) const
{
    return d->m_languages.size();
}

int TranslationsModel::columnCount([[maybe_unused]] const QModelIndex& parent) const
{
    return 2;
}

QList<Language>::Iterator TranslationsModel::findLanguage(const QString& key)
{
    return std::find_if(d->m_languages.begin(), d->m_languages.end(), [key](Language& lang) { return lang.key == key; });
}

std::optional<Language> TranslationsModel::findLanguageAsOptional(const QString& key)
{
    auto found = findLanguage(key);
    if (found != d->m_languages.end())
        return *found;
    return {};
}

void TranslationsModel::setUseSystemLocale(bool useSystemLocale)
{
    APPLICATION->settings()->set("UseSystemLocale", useSystemLocale);
    QLocale::setDefault(QLocale(useSystemLocale ? QString::fromStdString(std::locale().name()) : defaultLangCode));
}

bool TranslationsModel::selectLanguage(QString key)
{
    QString& langCode = key;
    auto langPtr = findLanguageAsOptional(key);

    if (langCode.isEmpty()) {
        d->no_language_set = true;
    }

    if (!langPtr.has_value()) {
        qWarning() << "Selected invalid language" << key << ", defaulting to" << defaultLangCode;
        langCode = defaultLangCode;
    } else {
        langCode = langPtr->key;
    }

    // uninstall existing translators if there are any
    if (d->m_app_translator) {
        QCoreApplication::removeTranslator(d->m_app_translator.get());
        d->m_app_translator.reset();
    }
    if (d->m_qt_translator) {
        QCoreApplication::removeTranslator(d->m_qt_translator.get());
        d->m_qt_translator.reset();
    }

    /*
     * FIXME: potential source of crashes:
     * In a multithreaded application, the default locale should be set at application startup, before any non-GUI threads are created.
     * This function is not reentrant.
     */
    QLocale::setDefault(
        QLocale(APPLICATION->settings()->get("UseSystemLocale").toBool() ? QString::fromStdString(std::locale().name()) : langCode));

    // if it's the default UI language, finish
    if (langCode == defaultLangCode) {
        d->m_selectedLanguage = langCode;
        return true;
    }

    // otherwise install new translations
    bool successful = false;
    // FIXME: this is likely never present. FIX IT.
    d->m_qt_translator.reset(new QTranslator());
    if (d->m_qt_translator->load("qt_" + langCode, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        qDebug() << "Loading Qt Language File for" << langCode.toLocal8Bit().constData() << "...";
        if (!QCoreApplication::installTranslator(d->m_qt_translator.get())) {
            qCritical() << "Loading Qt Language File failed.";
            d->m_qt_translator.reset();
        } else {
            successful = true;
        }
    } else {
        d->m_qt_translator.reset();
    }

    // Check if we have PO and/or QM files
    bool hasPO = QFile::exists(FS::PathCombine(d->m_dir.path(), langCode + ".po"));
    bool hasQM = QFile::exists(FS::PathCombine(d->m_dir.path(), "mmc_" + langCode + ".qm"));

    // Load QM first as fallback (lower priority)
    if (hasQM) {
        d->m_app_translator.reset(new QTranslator());
        if (d->m_app_translator->load("mmc_" + langCode, d->m_dir.path())) {
            qDebug() << "Loading Application Language File (QM) for" << langCode.toLocal8Bit().constData() << "...";
            if (!QCoreApplication::installTranslator(d->m_app_translator.get())) {
                qCritical() << "Installing Application Language File (QM) failed.";
                d->m_app_translator.reset();
            } else {
                successful = true;
            }
        } else {
            d->m_app_translator.reset();
        }
    }

    // Load PO as override (higher priority - installed last takes precedence)
    if (hasPO) {
        qDebug() << "Loading Application Language File (PO) for" << langCode.toLocal8Bit().constData() << "...";
        auto poTranslator = new POTranslator(FS::PathCombine(d->m_dir.path(), langCode + ".po"));
        if (!poTranslator->isEmpty()) {
            if (!QCoreApplication::installTranslator(poTranslator)) {
                delete poTranslator;
                qCritical() << "Installing Application Language File (PO) failed.";
            } else {
                d->m_po_translator.reset(poTranslator);
                successful = true;
            }
        } else {
            qCritical() << "Loading Application Language File (PO) failed.";
            d->m_po_translator.reset();
        }
    }

    // If neither PO nor QM exists, clear translator
    if (!hasPO && !hasQM) {
        d->m_app_translator.reset();
    }
    d->m_selectedLanguage = langCode;
    return successful;
}

QModelIndex TranslationsModel::selectedIndex()
{
    auto found = findLanguage(d->m_selectedLanguage);
    if (found != d->m_languages.end()) {
        return index(std::distance(d->m_languages.begin(), found), 0, QModelIndex());
    }
    return QModelIndex();
}

QString TranslationsModel::selectedLanguage()
{
    return d->m_selectedLanguage;
}

void TranslationsModel::downloadIndex()
{
    if (d->m_index_job || d->m_dl_job) {
        return;
    }
    qDebug() << "Downloading Translations Index...";
    d->m_index_job.reset(new NetJob("Translations Index", APPLICATION->network()));
    MetaEntryPtr entry = APPLICATION->metacache()->resolveEntry("translations", "index_v2.json");
    entry->setStale(true);
    auto task = Net::Download::makeCached(QUrl(BuildConfig.TRANSLATION_FILES_URL + "index_v2.json"), entry);
    d->m_index_task = task.get();
    d->m_index_job->addNetAction(task);
    d->m_index_job->setAskRetry(false);
    connect(d->m_index_job.get(), &NetJob::failed, this, &TranslationsModel::indexFailed);
    connect(d->m_index_job.get(), &NetJob::succeeded, this, &TranslationsModel::indexReceived);
    d->m_index_job->start();
}

void TranslationsModel::updateLanguage(QString key)
{
    if (key == defaultLangCode) {
        qWarning() << "Cannot update builtin language" << key;
        return;
    }
    auto found = findLanguageAsOptional(key);
    if (!found.has_value()) {
        qWarning() << "Cannot update invalid language" << key;
        return;
    }
    if (!found->updated) {
        downloadTranslation(key);
    }
}

void TranslationsModel::downloadTranslation(QString key)
{
    if (d->m_dl_job) {
        d->m_nextDownload = key;
        return;
    }
    auto lang = findLanguageAsOptional(key);
    if (!lang.has_value()) {
        qWarning() << "Will not download an unknown translation" << key;
        return;
    }

    d->m_downloadingTranslation = key;
    MetaEntryPtr entry = APPLICATION->metacache()->resolveEntry("translations", "mmc_" + key + ".qm");
    entry->setStale(true);

    auto dl = Net::Download::makeCached(QUrl(BuildConfig.TRANSLATION_FILES_URL + lang->file_name), entry);
    dl->addValidator(new Net::ChecksumValidator(QCryptographicHash::Sha1, lang->file_sha1));
    dl->setProgress(dl->getProgress(), lang->file_size);

    d->m_dl_job.reset(new NetJob("Translation for " + key, APPLICATION->network()));
    d->m_dl_job->addNetAction(dl);
    d->m_dl_job->setAskRetry(false);

    connect(d->m_dl_job.get(), &NetJob::succeeded, this, &TranslationsModel::dlGood);
    connect(d->m_dl_job.get(), &NetJob::failed, this, &TranslationsModel::dlFailed);

    d->m_dl_job->start();
}

void TranslationsModel::downloadNext()
{
    if (!d->m_nextDownload.isEmpty()) {
        downloadTranslation(d->m_nextDownload);
        d->m_nextDownload.clear();
    }
}

void TranslationsModel::dlFailed(QString reason)
{
    qCritical() << "Translations Download Failed:" << reason;
    d->m_dl_job.reset();
    downloadNext();
}

void TranslationsModel::dlGood()
{
    qDebug() << "Got translation:" << d->m_downloadingTranslation;

    if (d->m_downloadingTranslation == d->m_selectedLanguage) {
        selectLanguage(d->m_selectedLanguage);
    }
    d->m_dl_job.reset();
    downloadNext();
}

void TranslationsModel::indexFailed(QString reason)
{
    qCritical() << "Translations Index Download Failed:" << reason;
    d->m_index_job.reset();
}
