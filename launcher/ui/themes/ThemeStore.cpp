// SPDX-License-Identifier: GPL-3.0-only
#include "ThemeStore.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextDocument>
#include <QTimer>
#include <QUrlQuery>

namespace {
QString field(const QDomElement& element, const QString& name)
{
    return element.firstChildElement(name).text();
}
QString plain(const QString& text)
{
    QTextDocument document;
    document.setHtml(text.left(128 * 1024));
    return document.toPlainText();
}
bool secureUrl(const QString& value)
{
    const QUrl url(value);
    return url.scheme() == "https" && !url.host().isEmpty() && url.userInfo().isEmpty();
}
bool numeric(const QString& value)
{
    return QRegularExpression("^[0-9]{1,12}$").match(value).hasMatch();
}
QString categoryId(const QString& category)
{
    if (category == "palette")
        return "112";
    if (category == "icons")
        return "132";
    if (category == "widgets")
        return "123";
    if (category == "decoration")
        return "717";
    if (category == "bundle")
        return "722,121";
    if (category == "plasma")
        return "104";
    return {};
}
QString compatibility(const QString& category)
{
    if (category == "palette" || category == "icons" || category == "widgets")
        return QObject::tr("Recognized component format · checked after download");
    if (category == "bundle")
        return QObject::tr("Imports included Kvantum widgets, palettes and icons; external dependencies are checked separately");
    return QObject::tr("Browse only: this renderer is unavailable in Luna");
}
}  // namespace

ThemeStoreTask::ThemeStoreTask(QNetworkAccessManager* network, QJsonObject request, QString cacheDirectory)
    : m_network(network), m_request(std::move(request)), m_cacheDirectory(std::move(cacheDirectory))
{
    setAbortable(true);
}
QString ThemeStoreTask::githubRepository(const QString& source)
{
    const QUrl url(source);
    auto parts = url.path().split('/', Qt::SkipEmptyParts);
    static const QRegularExpression identifier("^[A-Za-z0-9_.-]+$");
    if (url.scheme() != "https" || url.host().compare("github.com", Qt::CaseInsensitive) != 0 || !url.userInfo().isEmpty() ||
        parts.size() != 2)
        return {};
    if (parts[1].endsWith(".git"))
        parts[1].chop(4);
    for (const auto& part : parts)
        if (part == "." || part == ".." || !identifier.match(part).hasMatch())
            return {};
    return parts.join('/');
}

ThemeStoreTask::~ThemeStoreTask()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
}
QJsonObject ThemeStoreTask::parseOcs(const QByteArray& bytes, const QJsonObject& request, QString& error)
{
    QDomDocument document;
    if (bytes.size() > 8 * 1024 * 1024 || bytes.toUpper().contains("<!DOCTYPE") || bytes.toUpper().contains("<!ENTITY") ||
        !document.setContent(bytes)) {
        error = tr("Invalid OCS response.");
        return {};
    }
    const auto root = document.documentElement(), meta = root.firstChildElement("meta"), data = root.firstChildElement("data");
    if (root.tagName() != "ocs" || field(meta, "statuscode") != "100") {
        error = tr("Theme store error %1: %2").arg(field(meta, "statuscode"), plain(field(meta, "message")));
        return {};
    }
    const auto mode = request.value("mode").toString("query");
    if (mode == "resolve") {
        auto content = data.firstChildElement("content");
        const auto url = field(content, "downloadlink");
        if (field(content, "downloadway") != "1" || !secureUrl(url)) {
            error = tr("This file requires an external or unsupported download method.");
            return {};
        }
        return { { "url", url }, { "md5", field(content, "downloadmd5sum") } };
    }
    const auto category = request.value("category").toString("palette");
    QJsonArray items, files;
    for (auto content = data.firstChildElement("content"); !content.isNull(); content = content.nextSiblingElement("content")) {
        QJsonObject item{ { "id", field(content, "id") },
                          { "provider", "kde" },
                          { "category", category },
                          { "name", field(content, "name") },
                          { "author", field(content, "personid") },
                          { "version", field(content, "version") },
                          { "description", plain(field(content, "description")) },
                          { "license", field(content, "license") },
                          { "changed", field(content, "changed") },
                          { "pageUrl", field(content, "detailpage") },
                          { "thumbnail", field(content, "smallpreviewpic1") },
                          { "preview", field(content, "previewpic1") },
                          { "compatibility", compatibility(category) },
                          { "installable", category == "palette" || category == "icons" || category == "widgets" || category == "bundle" } };
        if (!numeric(item.value("id").toString()))
            continue;
        items.append(item);
        if (mode == "files")
            for (int i = 1; i <= 100; ++i) {
                const auto suffix = QString::number(i), name = field(content, "downloadname" + suffix);
                if (name.isEmpty())
                    continue;
                const bool direct =
                    field(content, "downloadway" + suffix) == "1" && field(content, "downloadprice" + suffix).toDouble() == 0;
                // downloadsize is provider KiB, not an exact byte checksum.
                files.append(QJsonObject{ { "id", suffix },
                                          { "name", name },
                                          { "sizeKiB", field(content, "downloadsize" + suffix).toLongLong() },
                                          { "version", field(content, "download_version" + suffix) },
                                          { "md5", field(content, "downloadmd5sum" + suffix) },
                                          { "installable", direct && item.value("installable").toBool() } });
            }
    }
    const int page = meta.firstChildElement("page").isNull() ? request.value("page").toInt() : field(meta, "page").toInt();
    const int perPage = field(meta, "pagesize").isEmpty() ? field(meta, "itemsperpage").toInt() : field(meta, "pagesize").toInt();
    const int total = field(meta, "totalitems").toInt();
    return { { "items", items },
             { "files", files },
             { "page", meta.firstChildElement("page").isNull() ? request.value("page").toInt() : page },
             { "pageSize", perPage > 0 ? perPage : items.size() },
             { "total", total },
             { "hasNext", perPage > 0 ? (page + 1) * perPage < total : !items.isEmpty() },
             { "provider", "kde" },
             { "category", category } };
}
QJsonObject ThemeStoreTask::parseGitHub(const QByteArray& bytes, const QJsonObject& request, QString& error)
{
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(bytes, &parse);
    if (bytes.size() > 8 * 1024 * 1024 || parse.error != QJsonParseError::NoError || !document.isObject() ||
        !document.object().value("assets").isArray()) {
        error = tr("Invalid GitHub release response.");
        return {};
    }
    const auto release = document.object();
    QJsonArray items, files;
    for (const auto& value : release.value("assets").toArray()) {
        const auto asset = value.toObject();
        const auto name = asset.value("name").toString();
        const auto kind = name.endsWith("-theme.zip", Qt::CaseInsensitive)     ? "widgets"
                          : name.endsWith("-icons.zip", Qt::CaseInsensitive)   ? "icons"
                          : name.endsWith("-catpack.zip", Qt::CaseInsensitive) ? "background"
                                                                               : "";
        if (!*kind || !secureUrl(asset.value("browser_download_url").toString()))
            continue;
        const auto id = QString::number(asset.value("id").toInteger());
        QJsonObject file{ { "id", id },
                          { "name", name },
                          { "url", asset.value("browser_download_url") },
                          { "size", asset.value("size") },
                          { "digest", asset.value("digest") },
                          { "version", release.value("tag_name") },
                          { "installable", true } };
        items.append(QJsonObject{ { "id", id },
                                  { "name", name },
                                  { "provider", "github" },
                                  { "category", kind },
                                  { "author", release.value("author").toObject().value("login") },
                                  { "version", release.value("tag_name") },
                                  { "description", release.value("body") },
                                  { "pageUrl", release.value("html_url") },
                                  { "compatibility", tr("Prism theme package · checked after download") },
                                  { "installable", true },
                                  { "file", file } });
        if (request.value("contentId").toString() == id)
            files.append(file);
    }
    return { { "items", items },   { "files", files },       { "page", 0 },
             { "hasNext", false }, { "provider", "github" }, { "tag", release.value("tag_name") } };
}
void ThemeStoreTask::executeTask()
{
    setStatus(tr("Loading theme catalog"));
    setProgress(0, 0);
    m_bytes.clear();
    m_networkError.clear();
    const auto provider = m_request.value("provider").toString("kde"), mode = m_request.value("mode").toString("query");
    QUrl url;
    if (provider == "github") {
        const auto repository = githubRepository(m_request.value("source").toString());
        if (repository.isEmpty()) {
            emitFailed(tr("Invalid HTTPS GitHub source."));
            return;
        }
        url = QUrl("https://api.github.com/repos/" + repository + "/releases/latest");
    } else if (provider == "kde") {
        const auto category = m_request.value("category").toString("palette"), id = categoryId(category);
        if (id.isEmpty()) {
            emitFailed(tr("Unknown theme category."));
            return;
        }
        QString endpoint = "https://api.kde-look.org/ocs/v1/";
        if (category == "widgets" || category == "bundle")
            endpoint = "https://api.opendesktop.org/ocs/v1/";
        if (mode == "query") {
            const auto sort = m_request.value("sort").toString("high");
            const int page = m_request.value("page").toInt();
            if (!QStringList{ "high", "new", "alpha", "down" }.contains(sort) || page < 0 || page > 10000) {
                emitFailed(tr("Invalid catalog query."));
                return;
            }
            url = QUrl(endpoint + "content/data");
            QUrlQuery query;
            query.addQueryItem("categories", id);
            query.addQueryItem("sortmode", sort);
            query.addQueryItem("page", QString::number(page));
            query.addQueryItem("pagesize", "24");
            url.setQuery(query);
        } else {
            const auto content = m_request.value("contentId").toString(), file = m_request.value("fileId").toString();
            if (!numeric(content) || (mode == "resolve" && !numeric(file))) {
                emitFailed(tr("Invalid theme identity."));
                return;
            }
            url = QUrl(endpoint + (mode == "resolve" ? "content/download/" + content + '/' + file : "content/data/" + content));
        }
    } else {
        emitFailed(tr("Unknown theme provider."));
        return;
    }
    if (!QStringList{ "query", "files", "resolve" }.contains(mode)) {
        emitFailed(tr("Invalid theme request."));
        return;
    }
    m_cachePath = QDir(m_cacheDirectory)
                      .filePath(QString::fromLatin1(QCryptographicHash::hash(
                                                        url.toEncoded() + mode.toUtf8() + m_request.value("contentId").toString().toUtf8(),
                                                        QCryptographicHash::Sha256)
                                                        .toHex()) +
                                ".json");
    if (mode != "resolve" && !m_request.value("refresh").toBool() && loadCache(false))
        return;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "LunaLauncher ThemeCenter/1");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);
    request.setRawHeader("OCS-APIREQUEST", "true");
    m_reply = m_network->get(request);
    connect(m_reply, &QIODevice::readyRead, this, [this] {
        m_bytes += m_reply->readAll();
        if (m_bytes.size() > 8 * 1024 * 1024) {
            m_networkError = tr("Theme catalog response is too large.");
            m_reply->abort();
        }
    });
    connect(m_reply, &QNetworkReply::finished, this, &ThemeStoreTask::finishNetwork);
}
bool ThemeStoreTask::loadCache(bool offline)
{
    if (m_request.value("mode") == "resolve")
        return false;
    QFile file(m_cachePath);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024)
        return false;
    auto cached = QJsonDocument::fromJson(file.readAll()).object();
    const auto time = QDateTime::fromString(cached.value("cachedAt").toString(), Qt::ISODate);
    if (!time.isValid() || (!offline && time.secsTo(QDateTime::currentDateTimeUtc()) > 3600) || !cached.value("items").isArray())
        return false;
    m_result = cached;
    m_result.insert("offline", offline);
    if (offline)
        m_result.insert("warning", m_networkError);
    emitSucceeded();
    return true;
}
void ThemeStoreTask::fail(const QString& error)
{
    m_networkError = error;
    if (!loadCache(true))
        emitFailed(error);
}
void ThemeStoreTask::finishNetwork()
{
    if (!m_reply)
        return;
    auto reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();
    if (isFinished())
        return;
    m_bytes += reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        fail(m_networkError.isEmpty() ? reply->errorString() : m_networkError);
        return;
    }
    QString error;
    m_result = m_request.value("provider") == "github" ? parseGitHub(m_bytes, m_request, error) : parseOcs(m_bytes, m_request, error);
    m_bytes.clear();
    if (!error.isEmpty()) {
        fail(error);
        return;
    }
    m_result.insert("cachedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    m_result.insert("offline", false);
    if (m_request.value("mode") != "resolve") {
        QDir().mkpath(m_cacheDirectory);
        // Bound the private catalog cache. Installed packages live elsewhere.
        auto entries = QDir(m_cacheDirectory).entryInfoList({ "*.json" }, QDir::Files, QDir::Time | QDir::Reversed);
        const auto bytes = QJsonDocument(m_result).toJson(QJsonDocument::Compact);
        qint64 total = bytes.size();
        for (const auto& entry : entries)
            total += entry.size();
        while (!entries.isEmpty() && (entries.size() >= 100 || total > 32LL * 1024 * 1024)) {
            const auto entry = entries.takeFirst();
            total -= entry.size();
            QFile::remove(entry.absoluteFilePath());
        }
        QSaveFile file(m_cachePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(bytes);
            file.commit();
        }
    }
    emitSucceeded();
}
bool ThemeStoreTask::abort()
{
    if (!isRunning())
        return false;
    emitAborted();
    if (m_reply)
        m_reply->abort();
    return true;
}
