// SPDX-License-Identifier: GPL-3.0-only
#include "ThemeThumbnailLoader.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QtConcurrent>
#include <memory>

namespace {
constexpr int maxBytes = 4 * 1024 * 1024;
void saveImage(const QString& path, const QByteArray& bytes)
{
    const auto directory = QFileInfo(path).dir();
    if (!QDir().mkpath(directory.absolutePath()))
        return;
    auto entries = directory.entryInfoList({ "*.img" }, QDir::Files, QDir::Time | QDir::Reversed);
    qint64 total = bytes.size();
    for (const auto& entry : entries)
        total += entry.size();
    while (!entries.isEmpty() && (entries.size() >= 128 || total > 32LL * 1024 * 1024)) {
        const auto entry = entries.takeFirst();
        total -= entry.size();
        QFile::remove(entry.absoluteFilePath());
    }
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(bytes);
        file.commit();
    }
}
}  // namespace

ThemeThumbnailLoader::ThemeThumbnailLoader(QNetworkAccessManager* network, QString cacheDirectory, QObject* parent)
    : QObject(parent), m_network(network), m_directory(std::move(cacheDirectory))
{}
ThemeThumbnailLoader::~ThemeThumbnailLoader() { reset(); }
void ThemeThumbnailLoader::reset(bool forgetImages)
{
    ++m_generation;
    m_queue.clear();
    m_waiting.clear();
    m_active = 0;
    if (forgetImages)
        m_cache.clear();
    const auto replies = m_replies;
    m_replies.clear();
    for (const auto& reply : replies)
        if (reply)
            reply->abort();
}
void ThemeThumbnailLoader::load(const QString& key, Callback callback, bool priority)
{
    const QUrl url(key);
    if (url.scheme() != "https" || url.host().isEmpty() || !url.userInfo().isEmpty()) {
        callback({});
        return;
    }
    if (const auto image = m_cache.object(key)) {
        callback(*image);
        return;
    }
    const bool pending = m_waiting.contains(key);
    m_waiting[key].append(std::move(callback));
    if (!pending) {
        if (priority)
            m_queue.prepend(key);
        else
            m_queue.append(key);
    } else if (priority && m_queue.removeOne(key))
        m_queue.prepend(key);
    pump();
}
void ThemeThumbnailLoader::pump()
{
    while (m_active < 4 && !m_queue.isEmpty()) {
        const auto key = m_queue.takeFirst();
        const auto generation = m_generation;
        ++m_active;
        const QUrl url(key);
        const auto path = QDir(m_directory).filePath("images/" +
            QString::fromLatin1(QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha256).toHex()) + ".img");
        QFile cached(path);
        QByteArray old;
        if (cached.open(QIODevice::ReadOnly) && cached.size() <= maxBytes)
            old = cached.readAll();
        if (!old.isEmpty() && QFileInfo(cached).lastModified().secsTo(QDateTime::currentDateTimeUtc()) < 7 * 86400) {
            decode(key, old, {}, generation);
            continue;
        }
        QNetworkRequest request(url);
        request.setTransferTimeout(20000);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        auto reply = m_network->get(request);
        m_replies.append(reply);
        auto bytes = std::make_shared<QByteArray>();
        connect(reply, &QIODevice::readyRead, this, [reply, bytes] {
            *bytes += reply->readAll();
            if (bytes->size() > maxBytes)
                reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this, [this, key, reply, bytes, old, path, generation] {
            reply->deleteLater();
            m_replies.removeAll(reply);
            if (generation != m_generation)
                return;
            *bytes += reply->readAll();
            const bool valid = reply->error() == QNetworkReply::NoError && bytes->size() <= maxBytes && !bytes->isEmpty();
            decode(key, valid ? *bytes : old, valid ? path : QString(), generation);
        });
    }
}
void ThemeThumbnailLoader::decode(const QString& key, QByteArray bytes, const QString& savePath, quint64 generation)
{
    auto watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, key, bytes, savePath, generation] {
        const auto image = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation)
            return;
        if (!image.isNull() && !savePath.isEmpty())
            saveImage(savePath, bytes);
        finish(key, image, generation);
    });
    watcher->setFuture(QtConcurrent::run([bytes] {
        QBuffer buffer;
        buffer.setData(bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        reader.setAutoTransform(true);
        const auto size = reader.size();
        if (!size.isValid() || qint64(size.width()) * size.height() > 20000000)
            return QImage();
        reader.setScaledSize(size.scaled(640, 360, Qt::KeepAspectRatio));
        return reader.read();
    }));
}
void ThemeThumbnailLoader::finish(const QString& key, const QImage& image, quint64 generation)
{
    if (!image.isNull())
        m_cache.insert(key, new QImage(image), qMax(1, int(image.sizeInBytes() / 1024)));
    const auto callbacks = m_waiting.take(key);
    --m_active;
    for (const auto& callback : callbacks) {
        if (generation != m_generation)
            return;
        callback(image);
    }
    pump();
}
