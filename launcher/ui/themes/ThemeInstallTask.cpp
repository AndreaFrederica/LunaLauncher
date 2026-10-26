// SPDX-License-Identifier: GPL-3.0-only
#include "ThemeInstallTask.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QtConcurrent>
#include "ThemePackage.h"
#include "ThemeStore.h"
#include "net/NetJob.h"

namespace {
constexpr qint64 maxSize = 256LL * 1024 * 1024;
bool copyDirectory(const QDir& source, const QDir& target, QString& error, const std::shared_ptr<std::atomic_bool>& cancelled)
{
    // Directory imports need the same case-preserving staging as archive
    // imports.  Icon themes such as Candy contain paths that differ only by
    // case; on NTFS this is supported per directory, but only after the flag
    // is enabled before the first colliding entry is copied.  Keep the
    // portable fallback unchanged when the filesystem does not support it.
    target.mkpath(".");
    const bool caseSensitive = ThemePackage::enableCaseSensitiveDirectory(target);
    QDirIterator iterator(source.absolutePath(), QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    qint64 size = 0;
    int count = 0;
    while (iterator.hasNext()) {
        iterator.next();
        const auto info = iterator.fileInfo();
        const auto relative = source.relativeFilePath(info.absoluteFilePath());
        if (*cancelled) {
            error = QObject::tr("Installation cancelled.");
            return false;
        }
        if (++count > 50000 || info.isSymLink() || !info.isReadable() || (!info.isFile() && !info.isDir())) {
            error = QObject::tr("Local theme contains a link, special file, or too many entries.");
            return false;
        }
        if (ThemePackage::containedPath(source, relative, error).isEmpty())
            return false;
        if (info.isDir()) {
            if (!target.mkpath(relative)) {
                error = QObject::tr("Could not copy theme directory.");
                return false;
            }
            if (caseSensitive)
                ThemePackage::enableCaseSensitiveDirectory(QDir(target.filePath(relative)));
            continue;
        }
        size += info.size();
        if (size > 512LL * 1024 * 1024 || info.size() > 64LL * 1024 * 1024 ||
            !QDir().mkpath(QFileInfo(target.filePath(relative)).absolutePath()) ||
            !QFile::copy(info.absoluteFilePath(), target.filePath(relative))) {
            error = QObject::tr("Could not copy theme resource, or the theme is too large.");
            return false;
        }
    }
    return true;
}
QString directoryDigest(const QDir& root, QString& error)
{
    QStringList files;
    QDirIterator it(root.absolutePath(), QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext())
        files.append(it.next());
    std::sort(files.begin(), files.end());
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const auto& path : files) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            error = file.errorString();
            return {};
        }
        hash.addData(root.relativeFilePath(path).toUtf8());
        hash.addData(QByteArray(1, '\0'));
        if (!hash.addData(&file)) {
            error = file.errorString();
            return {};
        }
    }
    return QString::fromLatin1(hash.result().toHex());
}
}  // namespace
ThemeInstallTask::ThemeInstallTask(QNetworkAccessManager* network, QJsonObject request, QString packageDirectory, QString cacheDirectory)
    : m_network(network)
    , m_request(std::move(request))
    , m_packageDirectory(std::move(packageDirectory))
    , m_cacheDirectory(std::move(cacheDirectory))
    , m_cancelled(std::make_shared<std::atomic_bool>(false))
{
    setAbortable(true);
    connect(&m_worker, &QFutureWatcher<Staged>::finished, this, &ThemeInstallTask::publish);
}
ThemeInstallTask::~ThemeInstallTask()
{
    *m_cancelled = true;
    if (m_worker.isRunning())
        m_worker.waitForFinished();
}
QList<Task*> ThemeInstallTask::getSubTasks() const
{
    if (m_dependencyTask && m_dependencyTask->isRunning())
        return { m_dependencyTask.get() };
    if (m_dependencyResolver && m_dependencyResolver->isRunning())
        return { m_dependencyResolver.get() };
    if (m_download)
        return { m_download.get() };
    if (m_resolver)
        return { m_resolver.get() };
    return {};
}
void ThemeInstallTask::executeTask()
{
    *m_cancelled = false;
    m_result = {};
    m_dependencies = {};
    m_unresolvedDependencies = {};
    m_dependencyPackages = {};
    m_dependencyIndex = 0;
    m_downloadError.clear();
    const int sources = int(m_request.contains("path")) + int(m_request.contains("url")) + int(m_request.contains("provider"));
    if (sources != 1) {
        emitFailed(tr("Provide exactly one local path, URL, or store identity."));
        return;
    }
    if (!QDir().mkpath(m_packageDirectory)) {
        emitFailed(tr("Could not create theme package directory."));
        return;
    }
    if (m_request.contains("path")) {
        inspect(m_request.value("path").toString(), QFileInfo(m_request.value("path").toString()).fileName());
        return;
    }
    if (m_request.contains("provider")) {
        resolve();
        return;
    }
    download(m_request);
}
void ThemeInstallTask::resolve()
{
    setStatus(tr("Resolving theme download"));
    setProgress(0, 0);
    auto request = m_request;
    const bool kde = request.value("provider") == "kde";
    if (kde && !QStringList{ "palette", "icons", "widgets", "bundle" }.contains(request.value("category").toString())) {
        emitFailed(tr("This store category has no supported renderer."));
        return;
    }
    request.insert("mode", kde ? "resolve" : "files");
    request.insert("refresh", true);
    m_resolver = makeShared<ThemeStoreTask>(m_network, request, m_cacheDirectory);
    connect(m_resolver.get(), &Task::finished, this, [this, kde] {
        if (isFinished())
            return;
        if (!m_resolver->wasSuccessful()) {
            emitFailed(m_resolver->failReason());
            return;
        }
        if (kde) {
            auto file = m_resolver->result();
            file.insert("name", m_request.value("name"));
            download(file);
        } else {
            for (const auto& entry : m_resolver->result().value("files").toArray()) {
                auto file = entry.toObject();
                if (file.value("id") == m_request.value("fileId")) {
                    download(file);
                    return;
                }
            }
            emitFailed(tr("The selected release file no longer exists."));
        }
    });
    m_resolver->start();
}
void ThemeInstallTask::download(const QJsonObject& metadata)
{
    const QUrl url(metadata.value("url").toString());
    if (url.scheme() != "https" || url.host().isEmpty() || !url.userInfo().isEmpty()) {
        emitFailed(tr("Theme downloads require an HTTPS URL."));
        return;
    }
    m_downloadDirectory = std::make_shared<QTemporaryDir>(QDir(m_packageDirectory).filePath(".download-XXXXXX"));
    if (!m_downloadDirectory->isValid()) {
        emitFailed(tr("Could not create theme download directory."));
        return;
    }
    const auto path = m_downloadDirectory->filePath("download");
    // The application owns the network manager; never take ownership of this raw pointer.
    auto network =
        shared_qobject_ptr<QNetworkAccessManager>(QSharedPointer<QNetworkAccessManager>(m_network, [](QNetworkAccessManager*) {}));
    m_download = makeShared<NetJob>(tr("Download theme package"), network);
    m_download->setAskRetry(false);
    m_download->addNetAction(Net::Download::makeFile(url, path));
    setStatus(tr("Downloading theme package"));
    setProgress(0, 0);
    connect(m_download.get(), &Task::progress, this, [this](qint64 current, qint64 total) {
        setProgress(current, total);
        if (current > maxSize || total > maxSize) {
            m_downloadError = tr("Theme download exceeds 256 MiB.");
            m_download->abort();
        }
    });
    connect(m_download.get(), &Task::transferRate, this, &Task::setTransferRate);
    connect(m_download.get(), &Task::stepProgress, this, [this](const TaskStepProgress& step) { propagateStepProgress(step); });
    connect(m_download.get(), &Task::finished, this, [this, path, metadata, url] {
        if (isFinished())
            return;
        if (!m_downloadError.isEmpty() || !m_download->wasSuccessful()) {
            emitFailed(m_downloadError.isEmpty() ? m_download->failReason() : m_downloadError);
            return;
        }
        for (const auto& key : { "digest", "size", "md5" })
            if (metadata.contains(key))
                m_request.insert(key, metadata.value(key));
        auto name = metadata.value("name").toString();
        if (name.isEmpty())
            name = QFileInfo(url.path()).fileName();
        inspect(path, name);
    });
    m_download->start();
}
void ThemeInstallTask::inspect(const QString& path, const QString& name)
{
    setStatus(tr("Verifying, extracting and scanning theme components"));
    setProgress(0, 0);
    setTransferRate(0);
    const auto request = m_request;
    const auto destination = m_packageDirectory;
    const auto cancelled = m_cancelled;
    m_worker.setFuture(QtConcurrent::run([path, name, request, destination, cancelled] {
        Staged result;
        result.directory = std::make_shared<QTemporaryDir>(QDir(destination).filePath(".staging-XXXXXX"));
        if (!result.directory->isValid()) {
            result.error = QObject::tr("Could not stage theme installation.");
            return result;
        }
        QDir root(result.directory->filePath("package"));
        QDir().mkpath(root.absolutePath());
        QFileInfo source(path);
        if (source.isDir()) {
            if (!copyDirectory(QDir(path), root, result.error, cancelled))
                return result;
            result.digest = directoryDigest(root, result.error);
        } else {
            result.digest = ThemePackage::digest(path, result.error);
            if (result.digest.isEmpty())
                return result;
            if (request.contains("size") && source.size() != request.value("size").toInteger()) {
                result.error = QObject::tr("Theme package size mismatch.");
                return result;
            }
            const auto digest = request.value("digest").toString();
            if (!digest.isEmpty() && (!digest.startsWith("sha256:") || digest.mid(7).compare(result.digest, Qt::CaseInsensitive) != 0)) {
                result.error = QObject::tr("Theme package SHA-256 mismatch.");
                return result;
            }
            const auto md5 = request.value("md5").toString();
            if (!md5.isEmpty()) {
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly)) {
                    result.error = file.errorString();
                    return result;
                }
                QCryptographicHash hash(QCryptographicHash::Md5);
                if (!hash.addData(&file) || md5.compare(QString::fromLatin1(hash.result().toHex()), Qt::CaseInsensitive) != 0) {
                    result.error = QObject::tr("Theme package download checksum mismatch.");
                    return result;
                }
            }
            auto suffix = QFileInfo(name).suffix().toLower();
            if (QStringList{ "colors", "qss", "css" }.contains(suffix)) {
                if (!QFile::copy(path, root.filePath("imported." + suffix))) {
                    result.error = QObject::tr("Could not copy theme file.");
                    return result;
                }
            } else if (!ThemePackage::extract(path, root, result.error, [cancelled] { return cancelled->load(); }))
                return result;
        }
        if (*cancelled) {
            result.error = QObject::tr("Installation cancelled.");
            return result;
        }
        auto entries = ThemePackage::inspect(root, result.error);
        if (!result.error.isEmpty())
            return result;
        QJsonArray selected;
        const auto allowed = request.value("components").toArray();
        for (const auto& value : entries) {
            const auto entry = value.toObject();
            if (allowed.isEmpty() || allowed.contains(entry.value("kind")) || allowed.contains(entry.value("id")))
                selected.append(entry.value("id"));
        }
        if (selected.isEmpty()) {
            result.error = QObject::tr(
                "No selected compatible components were found. Supported widgets require QSS or a Kvantum configuration and SVG pair.");
            return result;
        }
        QJsonArray dependencies;
        for (const auto& value : entries) {
            const auto entry = value.toObject();
            if (entry.value("kind") != "bundle" || !selected.contains(entry.value("id")))
                continue;
            for (const auto& value : entry.value("dependencies").toArray()) {
                const auto dep = value.toObject();
                if (dep.value("supported").toBool() &&
                    (allowed.isEmpty() || allowed.contains(dep.value("category"))) && !dependencies.contains(dep))
                    dependencies.append(dep);
            }
        }
        result.record = { { "name", request.value("title").toString(name) },
                          { "installedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
                          { "provider", request.value("provider").toString("local") },
                          { "source", request.value("source") },
                          { "contentId", request.value("contentId") },
                          { "fileId", request.value("fileId") },
                          { "category", request.value("category") },
                          { "fileName", name },
                          { "version", request.value("version") },
                          { "digest", "sha256:" + result.digest },
                          { "selectedComponents", selected },
                          { "dependencies", dependencies },
                          { "catalog", entries } };
        QSaveFile file(root.filePath(".luna-package.json"));
        const auto bytes = QJsonDocument(result.record).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            result.error = QObject::tr("Could not save the installed package manifest.");
        return result;
    }));
}
void ThemeInstallTask::publish()
{
    auto staged = m_worker.result();
    if (*m_cancelled) {
        if (!isFinished())
            emitAborted();
        return;
    }
    if (!staged.error.isEmpty()) {
        emitFailed(staged.error);
        return;
    }
    setStatus(tr("Publishing immutable theme package"));
    setAbortable(false);
    QLockFile lock(QDir(m_packageDirectory).filePath(".install.lock"));
    if (!lock.tryLock(0)) {
        emitFailed(tr("Another process is installing a theme. Retry when it finishes."));
        return;
    }
    const auto target = QDir(m_packageDirectory).filePath(staged.digest);
    bool existing = QFileInfo::exists(target);
    if (existing) {
        QFile record(QDir(target).filePath(".luna-package.json"));
        if (QFileInfo(target).isSymLink() || !record.open(QIODevice::ReadOnly)) {
            emitFailed(tr("An invalid package occupies the immutable installation directory."));
            return;
        }
        auto previous = QJsonDocument::fromJson(record.readAll()).object();
        record.close();
        if (previous.value("digest") != "sha256:" + staged.digest) {
            emitFailed(tr("An invalid package occupies the immutable installation directory."));
            return;
        }
        // Resource versions stay immutable. Registration choices may expand when
        // the same archive is imported again with additional component kinds.
        auto selected = previous.value("selectedComponents").toArray();
        for (const auto& value : staged.record.value("selectedComponents").toArray())
            if (!selected.contains(value))
                selected.append(value);
        previous.insert("selectedComponents", selected);
        previous.insert("catalog", staged.record.value("catalog"));
        previous.insert("dependencies", staged.record.value("dependencies"));
        staged.record.insert("selectedComponents", selected);
        QSaveFile update(QDir(target).filePath(".luna-package.json"));
        const auto bytes = QJsonDocument(previous).toJson();
        if (!update.open(QIODevice::WriteOnly) || update.write(bytes) != bytes.size() || !update.commit()) {
            emitFailed(tr("Could not register additional theme components."));
            return;
        }
    } else if (!QDir().rename(staged.directory->filePath("package"), target)) {
        emitFailed(tr("Could not publish theme package."));
        return;
    }
    m_result = { { "packageId", staged.digest },
                 { "installed", QJsonArray{ staged.digest } },
                 { "alreadyInstalled", existing },
                 { "components", staged.record.value("selectedComponents") } };
    m_downloadDirectory.reset();
    m_resolver.reset();
    m_download.reset();
    if (!m_request.value("skipDependencies").toBool())
        m_dependencies = staged.record.value("dependencies").toArray();
    setAbortable(true);
    installNextDependency();
}
void ThemeInstallTask::installNextDependency()
{
    if (isFinished())
        return;
    if (m_dependencyIndex >= m_dependencies.size()) {
        finishInstall();
        return;
    }
    const auto dependency = m_dependencies.at(m_dependencyIndex++).toObject();
    setStatus(tr("Resolving theme dependency %1 (%2/%3)").arg(dependency.value("contentId").toString())
                  .arg(m_dependencyIndex).arg(m_dependencies.size()));
    setProgress(0, 0);
    auto query = dependency;
    query.insert("mode", "files");
    m_dependencyResolver = makeShared<ThemeStoreTask>(m_network, query, m_cacheDirectory);
    auto resolver = m_dependencyResolver.get();
    connect(resolver, &Task::finished, this, [this, resolver, dependency] {
        if (isFinished())
            return;
        auto unresolved = dependency;
        if (!resolver->wasSuccessful()) {
            unresolved.insert("error", resolver->failReason());
            m_unresolvedDependencies.append(unresolved);
            installNextDependency();
            return;
        }
        QJsonArray files;
        const auto override = m_request.value("dependencyFiles").toObject().value(dependency.value("contentId").toString()).toString();
        for (const auto& value : resolver->result().value("files").toArray()) {
            const auto file = value.toObject();
            if (file.value("installable").toBool() && (override.isEmpty() || file.value("id") == override))
                files.append(file);
        }
        if (files.size() != 1) {
            unresolved.insert("files", files);
            unresolved.insert("error", files.isEmpty() ? tr("No supported download file is available.")
                                                      : tr("Choose a download variant for this dependency."));
            m_unresolvedDependencies.append(unresolved);
            installNextDependency();
            return;
        }
        const auto file = files.first().toObject();
        auto request = dependency;
        request.insert("fileId", file.value("id"));
        request.insert("name", file.value("name"));
        request.insert("skipDependencies", true);
        request.insert("components", QJsonArray{ dependency.value("category") });
        m_dependencyTask = makeShared<ThemeInstallTask>(m_network, request, m_packageDirectory, m_cacheDirectory);
        auto task = m_dependencyTask.get();
        connect(task, &Task::status, this, [this](const QString& message) { setStatus(tr("Theme dependency: %1").arg(message)); });
        connect(task, &Task::progress, this, [this](qint64 current, qint64 total) { setProgress(current, total); });
        connect(task, &Task::finished, this, [this, task, dependency] {
            if (isFinished())
                return;
            if (task->wasSuccessful())
                m_dependencyPackages.append(task->result().value("packageId"));
            else {
                auto unresolved = dependency;
                unresolved.insert("error", task->failReason());
                m_unresolvedDependencies.append(unresolved);
            }
            installNextDependency();
        });
        task->start();
    });
    resolver->start();
}
void ThemeInstallTask::finishInstall()
{
    m_result.insert("dependencyPackages", m_dependencyPackages);
    m_result.insert("unresolvedDependencies", m_unresolvedDependencies);
    setProgress(1, 1);
    setStatus(m_unresolvedDependencies.isEmpty() ? tr("Installed · choose components to apply")
                                               : tr("Installed · some dependencies require selection or retry"));
    emitSucceeded();
}
bool ThemeInstallTask::abort()
{
    if (!isRunning() || !canAbort())
        return false;
    *m_cancelled = true;
    // Extraction owns its staging directory until its worker returns.
    emitAborted();
    if (m_download)
        m_download->abort();
    if (m_resolver)
        m_resolver->abort();
    if (m_dependencyResolver)
        m_dependencyResolver->abort();
    if (m_dependencyTask)
        m_dependencyTask->abort();
    return true;
}
