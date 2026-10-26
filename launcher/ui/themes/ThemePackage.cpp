// SPDX-License-Identifier: GPL-3.0-only
#include "ThemePackage.h"

#include <QColor>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QUrl>
#include <QSvgRenderer>
#include <QXmlStreamReader>
#include <algorithm>
#include "archive/ArchiveReader.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace {
QColor colorValue(const QVariant& value)
{
    auto text = value.metaType().id() == QMetaType::QStringList ? value.toStringList().join(',') : value.toString();
    auto channels = text.split(',');
    if (channels.size() == 3 || channels.size() == 4) {
        int values[4] = { 0, 0, 0, 255 };
        for (int i = 0; i < channels.size(); ++i) {
            bool valid = false;
            values[i] = channels[i].trimmed().toInt(&valid);
            if (!valid || values[i] < 0 || values[i] > 255)
                return {};
        }
        return QColor(values[0], values[1], values[2], values[3]);
    }
    return QColor(text);
}

QColor mix(QColor a, QColor b, double amount)
{
    amount = std::clamp(amount, 0.0, 1.0);
    return QColor::fromRgbF(a.redF() * (1 - amount) + b.redF() * amount, a.greenF() * (1 - amount) + b.greenF() * amount,
                            a.blueF() * (1 - amount) + b.blueF() * amount, a.alphaF() * (1 - amount) + b.alphaF() * amount);
}

bool safeRelative(const QString& name)
{
    if (name.isEmpty() || name.startsWith('/') || name.contains(':') || name.contains('\\'))
        return false;
    static const QRegularExpression invalid(R"([<>"|?*\x00-\x1f])");
    static const QRegularExpression reserved(R"(^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$))", QRegularExpression::CaseInsensitiveOption);
    for (const auto& part : name.split('/')) {
        if (part.isEmpty() || part == "." || part == ".." || part.endsWith('.') || part.endsWith(' ') || invalid.match(part).hasMatch() ||
            reserved.match(part).hasMatch())
            return false;
    }
    return true;
}

QJsonObject jsonFile(const QString& path, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
        error = QObject::tr("Cannot read theme manifest: %1").arg(path);
        return {};
    }
    QJsonParseError parse;
    auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject())
        error = QObject::tr("Invalid theme manifest: %1").arg(path);
    return doc.object();
}
}  // namespace

bool ThemePackage::readColors(const QString& path, Palette& result, QString& error)
{
    QFileInfo info(path);
    if (!info.isFile() || info.size() > 1024 * 1024) {
        error = QObject::tr("Invalid color scheme file.");
        return false;
    }
    QSettings settings(path, QSettings::IniFormat);
    if (!settings.childGroups().contains("Colors:View") || !settings.childGroups().contains("Colors:Window")) {
        error = QObject::tr("The file does not contain a KDE color scheme.");
        return false;
    }
    bool malformed = false;
    auto read = [&](const QString& group, const QString& key, QColor fallback) {
        const auto name = group + '/' + key;
        if (!settings.contains(name))
            return fallback;
        auto value = colorValue(settings.value(name));
        if (!value.isValid())
            malformed = true;
        return value.isValid() ? value : fallback;
    };
    const auto window = read("Colors:Window", "BackgroundNormal", QColor(239, 240, 241));
    const auto fg = read("Colors:Window", "ForegroundNormal", QColor(35, 38, 41));
    const auto base = read("Colors:View", "BackgroundNormal", QColor(Qt::white));
    const auto text = read("Colors:View", "ForegroundNormal", fg);
    QPalette palette;
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, fg);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::AlternateBase, read("Colors:View", "BackgroundAlternate", mix(base, window, .5)));
    palette.setColor(QPalette::Button, read("Colors:Button", "BackgroundNormal", window));
    palette.setColor(QPalette::ButtonText, read("Colors:Button", "ForegroundNormal", fg));
    palette.setColor(QPalette::Highlight, read("Colors:Selection", "BackgroundNormal", QColor(61, 174, 233)));
    palette.setColor(QPalette::HighlightedText, read("Colors:Selection", "ForegroundNormal", QColor(Qt::white)));
    palette.setColor(QPalette::ToolTipBase, read("Colors:Tooltip", "BackgroundNormal", window));
    palette.setColor(QPalette::ToolTipText, read("Colors:Tooltip", "ForegroundNormal", fg));
    palette.setColor(QPalette::Link, read("Colors:View", "ForegroundLink", QColor(41, 128, 185)));
    palette.setColor(QPalette::LinkVisited, read("Colors:View", "ForegroundVisited", QColor(127, 140, 141)));
    palette.setColor(QPalette::BrightText, read("Colors:View", "ForegroundNegative", QColor(218, 68, 83)));
    palette.setColor(QPalette::Light, mix(window, Qt::white, .45));
    palette.setColor(QPalette::Midlight, mix(window, Qt::white, .2));
    palette.setColor(QPalette::Mid, mix(window, Qt::black, .2));
    palette.setColor(QPalette::Dark, mix(window, Qt::black, .4));
    palette.setColor(QPalette::Shadow, mix(window, Qt::black, .65));
    palette.setColor(QPalette::PlaceholderText, mix(text, base, .5));
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    palette.setColor(QPalette::Accent, palette.highlight().color());
#endif
    // Keep the separate inactive selection colors, rather than fading all roles.
    if (settings.contains("Colors:Selection/BackgroundInactive"))
        palette.setColor(QPalette::Inactive, QPalette::Highlight, read("Colors:Selection", "BackgroundInactive", window));
    for (auto group : { QPalette::Inactive, QPalette::Disabled }) {
        const auto section =
            group == QPalette::Disabled ? QStringLiteral("ColorEffects:Disabled") : QStringLiteral("ColorEffects:Inactive");
        if (!settings.value(section + "/Enable", group == QPalette::Disabled).toBool())
            continue;
        const auto effectColor = read(section, "Color", window);
        const int colorEffect = settings.value(section + "/ColorEffect", group == QPalette::Disabled ? 0 : 2).toInt();
        const double amount = settings.value(section + "/ColorAmount", .025).toDouble();
        const int contrastEffect = settings.value(section + "/ContrastEffect", group == QPalette::Disabled ? 1 : 2).toInt();
        const double contrast = settings.value(section + "/ContrastAmount", group == QPalette::Disabled ? .65 : .1).toDouble();
        const int intensityEffect = settings.value(section + "/IntensityEffect", 0).toInt();
        const double intensity = settings.value(section + "/IntensityAmount", 0).toDouble();
        for (int role = 0; role < QPalette::NColorRoles; ++role) {
            if (role == QPalette::NoRole)
                continue;
            auto value = palette.color(group, QPalette::ColorRole(role));
            if (intensityEffect == 1)
                value = mix(value, intensity >= 0 ? Qt::white : Qt::black, std::abs(intensity));
            if (intensityEffect == 2)
                value = mix(value, Qt::black, std::abs(intensity));
            if (intensityEffect == 3)
                value = mix(value, Qt::white, std::abs(intensity));
            if (colorEffect == 1)
                value = mix(value, QColor::fromHslF(value.hslHueF(), 0, value.lightnessF()), std::abs(amount));
            if (colorEffect >= 2)
                value = mix(value, effectColor, std::abs(amount));
            const bool foreground = role == QPalette::WindowText || role == QPalette::Text || role == QPalette::ButtonText ||
                                    role == QPalette::HighlightedText || role == QPalette::Link || role == QPalette::LinkVisited ||
                                    role == QPalette::PlaceholderText;
            if (foreground && contrastEffect)
                value = mix(value, role == QPalette::HighlightedText ? palette.color(group, QPalette::Highlight) : window, contrast);
            palette.setColor(group, QPalette::ColorRole(role), value);
        }
    }
    if (malformed || settings.status() != QSettings::NoError) {
        error = QObject::tr("The color scheme contains invalid colors.");
        return false;
    }
    // QSettings maps an INI [General] section onto top-level keys.
    result.name = settings.value("Name", settings.value("General/Name", info.completeBaseName())).toString();
    result.colors = palette;
    result.dark = window.lightnessF() < .5;
    for (const auto& group : { "Colors:View", "Colors:Window", "Colors:Header", "Colors:Complementary", "WM" }) {
        settings.beginGroup(group);
        QJsonObject values;
        for (const auto& key : settings.childKeys())
            values.insert(key, settings.value(key).toString());
        settings.endGroup();
        result.semantic.insert(group, values);
    }
    return true;
}

QString ThemePackage::containedPath(const QDir& root, const QString& relative, QString& error)
{
    if (!safeRelative(relative)) {
        error = QObject::tr("Unsafe theme path: %1").arg(relative);
        return {};
    }
    const auto path = QFileInfo(root.filePath(relative)).canonicalFilePath();
    const auto base = QFileInfo(root.absolutePath()).canonicalFilePath();
    if (base.isEmpty() || path.isEmpty() || !path.startsWith(base + '/', Qt::CaseInsensitive)) {
        error = QObject::tr("Theme resource is missing or outside its package: %1").arg(relative);
        return {};
    }
    return path;
}

bool ThemePackage::enableCaseSensitiveDirectory(const QDir& directory)
{
#if defined(Q_OS_WIN)
    if (!directory.exists())
        return false;

    // FileCaseSensitiveInformation is available in the Windows 10 SDK, but
    // older SDKs used by some Qt/MSVC installations do not expose the enum or
    // structure. The value is stable in the Windows API, so keep a private
    // ABI-compatible definition here instead of making the whole project
    // depend on a newer SDK.
    constexpr auto fileCaseSensitiveInformation = static_cast<FILE_INFO_BY_HANDLE_CLASS>(23);
    constexpr ULONG caseSensitiveDirectoryFlag = 0x00000001u;
    struct FileCaseSensitiveInformation {
        ULONG flags;
    } info{};

    const auto path = QDir::toNativeSeparators(directory.absolutePath()).toStdWString();
    const auto handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return false;

    bool result = false;
    if (GetFileInformationByHandleEx(handle, fileCaseSensitiveInformation, &info, sizeof(info))) {
        if (info.flags & caseSensitiveDirectoryFlag) {
            result = true;
        } else {
            info.flags |= caseSensitiveDirectoryFlag;
            result = SetFileInformationByHandle(handle, fileCaseSensitiveInformation, &info, sizeof(info)) != FALSE;
        }
    }
    CloseHandle(handle);
    return result;
#else
    Q_UNUSED(directory)
    return false;
#endif
}

bool ThemePackage::extract(const QString& archivePath, const QDir& destination, QString& error, const std::function<bool()>& cancelled)
{
    struct Link {
        QString name, target;
        bool hard;
    };
    QList<Link> links;

    // Probe names before writing anything. If the archive contains entries
    // that differ only by case, a normal Windows directory cannot represent
    // them. A newly-created package staging tree is a safe place to opt into
    // NTFS' directory-local case-sensitive mode, but the operation is
    // intentionally best-effort and must fall back on portable extraction.
    bool preserveCaseCollisions = false;
    {
        QHash<QString, QString> probeNames;
        QSet<QString> probeFiles;
        QSet<QString> probeDirectories;
        bool caseCollision = false;
        int probeCount = 0;
        MMCZip::ArchiveReader probe(archivePath);
        const auto probeOk = probe.parse([&](MMCZip::ArchiveReader::File* file) {
            auto name = file->filename();
            while (name.startsWith("./"))
                name.remove(0, 2);
            if (name.endsWith('/'))
                name.chop(1);
            if ((name.isEmpty() || name == ".") && file->isDirectory())
                return true;
            if (!safeRelative(name) || ++probeCount > 50000) {
                error = QObject::tr("Unsafe or excessive theme archive entries.");
                return false;
            }
            if (probeNames.contains(name)) {
                error = QObject::tr("Duplicate archive path: %1").arg(name);
                return false;
            }
            probeNames.insert(name, name);
            if (file->isDirectory())
                probeDirectories.insert(name);
            else
                probeFiles.insert(name);

            auto parent = QFileInfo(name).path();
            while (!parent.isEmpty() && parent != ".") {
                probeDirectories.insert(parent);
                parent = QFileInfo(parent).path();
            }
            // Keep a separate folded map below: probeNames remains exact so
            // that a pair such as foo.svg/foo.SVG is never mistaken for a
            // duplicate.
            return file->skip();
        });
        if (!probeOk) {
            if (error.isEmpty())
                error = QObject::tr("Could not read the theme archive.");
            return false;
        }

        // The exact-name map above intentionally keeps archive paths intact;
        // detect folds in a second, bounded pass without touching disk.
        QHash<QString, QString> foldedNames;
        for (const auto& name : probeNames.keys()) {
            const auto folded = name.toCaseFolded();
            if (foldedNames.contains(folded) && foldedNames.value(folded) != name)
                caseCollision = true;
            else
                foldedNames.insert(folded, name);
        }

        // Reject an archive that describes a file and a directory at the same
        // path before pre-creating directories for the case-sensitive pass.
        for (const auto& directory : probeDirectories) {
            if (probeFiles.contains(directory)) {
                error = QObject::tr("Conflicting archive paths: %1").arg(directory);
                return false;
            }
        }

        if (caseCollision) {
            const bool rootEnabled = destination.mkpath(".") && enableCaseSensitiveDirectory(destination);
            if (rootEnabled) {
                auto directories = probeDirectories.values();
                std::sort(directories.begin(), directories.end(), [](const QString& left, const QString& right) {
                    const auto leftDepth = left.count('/');
                    const auto rightDepth = right.count('/');
                    return leftDepth == rightDepth ? left.size() < right.size() : leftDepth < rightDepth;
                });
                preserveCaseCollisions = true;
                for (const auto& relative : directories) {
                    const auto directory = destination.filePath(relative);
                    if (!QDir().mkpath(directory) || !enableCaseSensitiveDirectory(QDir(directory))) {
                        preserveCaseCollisions = false;
                        break;
                    }
                }
                if (preserveCaseCollisions)
                    qInfo() << "Using NTFS case-sensitive staging for theme archive" << archivePath;
                else
                    qInfo() << "Falling back to portable case-folded theme extraction for" << archivePath;
            } else {
                qInfo() << "NTFS case-sensitive staging is unavailable; using portable case-folded theme extraction for" << archivePath;
            }
        }
    }

    // Windows filesystems are case-insensitive, while a number of KDE icon
    // archives contain both spellings of the same path (for example
    // text-x-r.svg and text-x-R.svg).  Keep the first spelling so the same
    // archive has deterministic contents on every platform.  An exact
    // duplicate remains an error because it is ambiguous even on
    // case-sensitive filesystems.
    enum class EntryKind { File, Directory, Link };
    QHash<QString, QString> seen;
    QHash<QString, EntryKind> seenKinds;
    qint64 total = 0;
    int count = 0;
    MMCZip::ArchiveReader archive(archivePath);
    const bool ok = archive.parse([&](MMCZip::ArchiveReader::File* file) {
        if (cancelled && cancelled()) {
            error = QObject::tr("Theme installation cancelled.");
            return false;
        }
        auto name = file->filename();
        while (name.startsWith("./"))
            name.remove(0, 2);
        if (name.endsWith('/'))
            name.chop(1);
        if ((name.isEmpty() || name == ".") && file->isDirectory())
            return true;
        if (!safeRelative(name) || ++count > 50000) {
            error = QObject::tr("Unsafe or excessive theme archive entries.");
            return false;
        }
        const auto kind = !file->linkTarget().isEmpty() ? EntryKind::Link : file->isDirectory() ? EntryKind::Directory : EntryKind::File;
        const auto key = preserveCaseCollisions ? name : name.toCaseFolded();
        if (seen.contains(key)) {
            if (seen.value(key) == name) {
                error = QObject::tr("Duplicate archive path: %1").arg(name);
                return false;
            }
            // Prefer a real file or directory over an alias with the same
            // spelling on a case-insensitive filesystem.  KDE icon archives
            // commonly list aliases before the file they point at.
            if (seenKinds.value(key) == EntryKind::Link && kind != EntryKind::Link) {
                const auto previous = seen.value(key);
                for (int i = links.size() - 1; i >= 0; --i)
                    if (links.at(i).name == previous)
                        links.removeAt(i);
                seen[key] = name;
                seenKinds[key] = kind;
            } else if (seenKinds.value(key) == EntryKind::Link && kind == EntryKind::Link) {
                // Flatten aliases that only differ by case.  Keeping the
                // first name while adopting the later target avoids a chain
                // such as Thunar.svg -> thunar.svg -> file-manager.svg,
                // where Windows cannot materialize both spellings.
                const auto previous = seen.value(key);
                for (auto& pending : links)
                    if (pending.name == previous) {
                        pending.target = file->linkTarget();
                        pending.hard = file->isHardLink();
                        break;
                    }
                return true;
            } else {
                qWarning() << "Skipping case-colliding theme archive path" << name << "(keeping" << seen.value(key) << ")";
                return true;
            }
        }
        else {
            seen.insert(key, name);
            seenKinds.insert(key, kind);
        }
        if (file->isDirectory())
            return destination.mkpath(name);
        if (!file->linkTarget().isEmpty()) {
            links.append({ name, file->linkTarget(), file->isHardLink() });
            return true;
        }
        if (!file->isFile() || file->size() < 0 || file->size() > 64LL * 1024 * 1024) {
            error = QObject::tr("Unsupported theme archive entry: %1").arg(name);
            return false;
        }
        total += file->size();
        if (total > 512LL * 1024 * 1024) {
            error = QObject::tr("The expanded theme exceeds 512 MiB.");
            return false;
        }
        const auto path = destination.filePath(name);
        if (!QDir().mkpath(QFileInfo(path).absolutePath()))
            return false;
        QFile output(path);
        if (!output.open(QIODevice::WriteOnly) || !file->copyTo(output, file->size()) || output.size() != file->size()) {
            error = QObject::tr("Could not extract theme resource: %1").arg(name);
            return false;
        }
        return true;
    });
    if (!ok) {
        if (error.isEmpty())
            error = QObject::tr("Could not read the theme archive.");
        return false;
    }
    // Resolve internal icon aliases without creating OS links. Directory aliases
    // wait until all aliases below their source have been materialized.
    for (int pass = 0; !links.isEmpty() && pass <= count; ++pass) {
        bool progressed = false;
        for (int i = links.size() - 1; i >= 0; --i) {
            if (cancelled && cancelled()) {
                error = QObject::tr("Theme installation cancelled.");
                return false;
            }
            const auto& link = links[i];
            if (link.target.startsWith('/') || link.target.contains(':') || link.target.contains('\\')) {
                // KDE icon themes occasionally carry a link to a system icon
                // outside the archive (for example /usr/share/icons/breeze-
                // dark/...).  That target cannot be materialized on Windows,
                // but it must not make the portable parts of the package
                // unusable.  Leave this one alias absent so Qt can use the
                // configured fallback icon theme.
                const bool knownIconFallback = link.target.startsWith("/usr/share/icons/") ||
                                               link.target.startsWith("/usr/local/share/icons/");
                if (!knownIconFallback) {
                    error = QObject::tr("Icon link escapes its package.");
                    return false;
                }
                qWarning() << "Skipping external theme icon link" << link.name << "->" << link.target;
                links.removeAt(i);
                progressed = true;
                continue;
            }
            auto relative = QDir::cleanPath(link.hard ? link.target : QFileInfo(link.name).path() + '/' + link.target);
            if (!safeRelative(relative)) {
                error = QObject::tr("Icon link escapes its package.");
                return false;
            }
            auto target = destination.filePath(relative);
            QFileInfo source(target);
            if (!source.exists()) {
                // A few KDE icon archives contain optional SVG aliases whose
                // target was removed upstream.  Keep the usable icons instead
                // of rejecting the whole archive, but wait while another
                // pending alias could still materialize this target.  The
                // path has already passed safeRelative(), so skipping it
                // cannot allow an escape from the package root.
                bool pendingTarget = false;
                for (const auto& pending : links)
                    if (pending.name != link.name &&
                        (pending.name == relative || pending.name.startsWith(relative + '/') || relative.startsWith(pending.name + '/'))) {
                        pendingTarget = true;
                        break;
                    }
                if (!pendingTarget && !link.hard && link.name.endsWith(".svg", Qt::CaseInsensitive)) {
                    qWarning() << "Skipping missing optional theme icon link" << link.name << "->" << link.target;
                    links.removeAt(i);
                    progressed = true;
                }
                continue;
            }
            if (source.isSymLink()) {
                error = QObject::tr("Unsafe icon link.");
                return false;
            }
            if (source.isDir()) {
                if (link.hard || link.name == relative || link.name.startsWith(relative + '/') || relative.startsWith(link.name + '/')) {
                    error = QObject::tr("Cyclic directory icon link.");
                    return false;
                }
                bool unresolved = false;
                for (const auto& pending : links)
                    if (pending.name.startsWith(relative + '/')) {
                        unresolved = true;
                        break;
                    }
                if (unresolved)
                    continue;
                const auto outputRoot = destination.filePath(link.name);
                if (QFileInfo::exists(outputRoot) || !QDir().mkpath(outputRoot)) {
                    error = QObject::tr("Conflicting icon alias directory.");
                    return false;
                }
                QDirIterator children(target, QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
                while (children.hasNext()) {
                    children.next();
                    const auto child = children.fileInfo();
                    if ((cancelled && cancelled()) || ++count > 50000 || child.isSymLink()) {
                        error = QObject::tr("Icon alias expansion cancelled or too large.");
                        return false;
                    }
                    const auto output = QDir(outputRoot).filePath(QDir(target).relativeFilePath(child.absoluteFilePath()));
                    if (child.isDir()) {
                        if (!QDir().mkpath(output)) {
                            error = QObject::tr("Could not materialize icon directory.");
                            return false;
                        }
                    } else {
                        total += child.size();
                        if (!child.isFile() || total > 512LL * 1024 * 1024 || !QDir().mkpath(QFileInfo(output).absolutePath()) ||
                            !QFile::copy(child.absoluteFilePath(), output)) {
                            error = QObject::tr("Could not materialize icon directory.");
                            return false;
                        }
                    }
                }
                links.removeAt(i);
                progressed = true;
                continue;
            }
            if (!source.isFile()) {
                error = QObject::tr("Unsupported icon link.");
                return false;
            }
            total += source.size();
            if (total > 512LL * 1024 * 1024 || !QDir().mkpath(QFileInfo(destination.filePath(link.name)).absolutePath()) ||
                !QFile::copy(target, destination.filePath(link.name))) {
                error = QObject::tr("Could not materialize icon link.");
                return false;
            }
            links.removeAt(i);
            progressed = true;
        }
        if (!progressed) {
            const auto& pending = links.constFirst();
            error = QObject::tr("Broken or cyclic icon link: %1 -> %2.").arg(pending.name, pending.target);
            return false;
        }
    }
    return true;
}

QString ThemePackage::digest(const QString& path, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 256LL * 1024 * 1024) {
        error = QObject::tr("Cannot read theme package, or it exceeds 256 MiB.");
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        error = file.errorString();
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString ThemePackage::scopedStyleSheet(const QString& text, const QDir& root, const QString& resourcePrefix, QString& error)
{
    if (text.size() > 4 * 1024 * 1024) {
        error = QObject::tr("Theme stylesheet is too large.");
        return {};
    }
    QString output;
    bool comment = false;
    QChar quote;
    for (qsizetype i = 0; i < text.size();) {
        if (comment) {
            if (text.mid(i, 2) == "*/") {
                comment = false;
                i += 2;
            } else
                ++i;
            continue;
        }
        if (!quote.isNull()) {
            auto c = text[i++];
            output += c;
            if (c == '\\' && i < text.size())
                output += text[i++];
            else if (c == quote)
                quote = {};
            continue;
        }
        if (text.mid(i, 2) == "/*") {
            comment = true;
            i += 2;
            continue;
        }
        if (text[i] == '\'' || text[i] == '"') {
            quote = text[i];
            output += text[i++];
            continue;
        }
        if (text.mid(i, 7).compare("@import", Qt::CaseInsensitive) == 0) {
            error = QObject::tr("Stylesheet imports are unsupported.");
            return {};
        }
        if (text.mid(i, 3).compare("url", Qt::CaseInsensitive) == 0) {
            qsizetype open = i + 3;
            while (open < text.size() && text[open].isSpace())
                ++open;
            if (open < text.size() && text[open] == '(') {
                qsizetype close = text.indexOf(')', open + 1);
                if (close < 0) {
                    error = QObject::tr("Unclosed stylesheet URL.");
                    return {};
                }
                auto path = text.mid(open + 1, close - open - 1).trimmed();
                if (path.size() >= 2 && ((path.front() == '"' && path.back() == '"') || (path.front() == '\'' && path.back() == '\'')))
                    path = path.mid(1, path.size() - 2);
                if (path.contains('\\') || path.contains('\n') || path.contains('\r') || path.contains('"') || path.contains('\'')) {
                    error = QObject::tr("Escaped or malformed stylesheet URL.");
                    return {};
                }
                if (!path.startsWith(":/")) {
                    if (path.startsWith("theme:"))
                        path = "resources/" + path.mid(6);
                    if (containedPath(root, path, error).isEmpty())
                        return {};
                    path = resourcePrefix + ':' + path;
                }
                output += "url(\"" + path + "\")";
                i = close + 1;
                continue;
            }
        }
        output += text[i++];
    }
    if (comment || !quote.isNull()) {
        error = QObject::tr("Unclosed stylesheet comment or string.");
        return {};
    }
    return output;
}

bool ThemePackage::validateStyleSheet(const QString& text, const QDir& root, QString& error)
{
    scopedStyleSheet(text, root, "validation", error);
    return error.isEmpty();
}

bool ThemePackage::validateKvantum(const QString& configuration, QString& error)
{
    QFileInfo config(configuration);
    if (!config.isFile() || config.size() > 1024 * 1024) {
        error = QObject::tr("Invalid Kvantum configuration.");
        return false;
    }
    QSettings settings(configuration, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError || settings.childGroups().isEmpty()) {
        error = QObject::tr("Invalid Kvantum configuration.");
        return false;
    }
    auto svgPath = containedPath(config.dir(), config.completeBaseName() + ".svg", error);
    QFile svg(svgPath);
    if (svgPath.isEmpty() || !svg.open(QIODevice::ReadOnly) || svg.size() > 16 * 1024 * 1024) {
        error = QObject::tr("Kvantum requires a matching SVG beside its configuration.");
        return false;
    }
    const auto bytes = svg.readAll();
    QXmlStreamReader xml(bytes);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isDTD() || xml.tokenType() == QXmlStreamReader::EntityReference) {
            error = QObject::tr("Theme SVG declarations and external entities are unsupported.");
            return false;
        }
        if (!xml.isStartElement())
            continue;
        for (const auto& attribute : xml.attributes()) {
            const auto value = attribute.value().toString().trimmed();
            if ((attribute.name() == QLatin1StringView("href") && !value.startsWith('#') &&
                 !value.startsWith("data:image/")) ||
                (value.contains("url(", Qt::CaseInsensitive) &&
                 QRegularExpression(R"(url\(\s*['\"]?[^#\s])", QRegularExpression::CaseInsensitiveOption).match(value).hasMatch())) {
                error = QObject::tr("Theme SVG must not reference external resources.");
                return false;
            }
        }
    }
    QSvgRenderer renderer;
    if (xml.hasError() || !renderer.load(bytes)) {
        error = QObject::tr("Invalid Kvantum SVG.");
        return false;
    }
    return true;
}

QJsonObject ThemePackage::readKvantumEffects(const QString& configuration, QString& error)
{
    QFileInfo config(configuration);
    if (!config.isFile() || config.size() > 1024 * 1024) {
        error = QObject::tr("Invalid Kvantum configuration.");
        return {};
    }
    QSettings settings(configuration, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        error = QObject::tr("Invalid Kvantum configuration.");
        return {};
    }

    // Kvantum stores these in [General]. Keep the original names out of the
    // public API: the launcher only needs the semantic window capabilities.
    auto valueFor = [&](const QString& key) {
        // Kvantum uses a [%General] group for most compositor settings and a
        // [Hacks] group for blur_translucent in older theme revisions.
        auto value = settings.value("General/" + key);
        if (!value.isValid())
            value = settings.value("Hacks/" + key);
        return value;
    };
    auto boolean = [&](const QString& key, bool fallback = false) {
        const auto value = valueFor(key);
        if (!value.isValid())
            return fallback;
        if (value.metaType().id() == QMetaType::QString) {
            const auto text = value.toString().trimmed();
            if (text.compare("true", Qt::CaseInsensitive) == 0 || text == "1" || text.compare("yes", Qt::CaseInsensitive) == 0)
                return true;
            if (text.compare("false", Qt::CaseInsensitive) == 0 || text == "0" || text.compare("no", Qt::CaseInsensitive) == 0)
                return false;
        }
        return value.toBool();
    };
    auto integer = [&](const QString& key, int fallback = 0) {
        bool ok = false;
        const auto value = valueFor(key).toString().trimmed().toInt(&ok);
        return ok ? qBound(0, value, 100) : fallback;
    };

    const auto composite = boolean("composite", true);
    const auto translucent = boolean("translucent_windows");
    const auto blurring = boolean("blurring");
    const auto blurTranslucent = boolean("blur_translucent");
    const auto popupBlurring = boolean("popup_blurring");
    const auto reduceOpacity = integer("reduce_window_opacity");
    const auto menuShadow = integer("menu_shadow_depth");
    const auto tooltipShadow = integer("tooltip_shadow_depth");

    return { { "composite", composite },
             { "translucentWindows", translucent },
             { "blurring", blurring },
             { "blurTranslucent", blurTranslucent },
             { "popupBlurring", popupBlurring },
             { "reduceWindowOpacity", reduceOpacity },
             { "menuShadowDepth", menuShadow },
             { "tooltipShadowDepth", tooltipShadow },
             // These are deliberately derived rather than copied from a
             // platform-specific config. A compositor can choose the closest
             // native material (Acrylic on Win10, Mica on Win11, blur hints on
             // X11) while preserving the user's intent.
             { "backdrop", composite && translucent && (blurring || blurTranslucent) ? "acrylic" : composite && translucent ? "mica" : "none" },
             { "windowShadow", composite && (menuShadow > 0 || tooltipShadow > 0) } };
}

QJsonObject ThemePackage::readGlobalTheme(const QString& defaults, QString& error)
{
    QFile input(defaults);
    if (!input.open(QIODevice::ReadOnly) || input.size() > 1024 * 1024) {
        error = QObject::tr("Cannot read KDE global theme defaults.");
        return {};
    }
    // KConfig nested groups use [file][group], unlike QSettings INI groups.
    QHash<QString, QString> values;
    QString group;
    for (auto line : QString::fromUtf8(input.readAll()).split('\n')) {
        line = line.trimmed();
        if (line.startsWith('#') || line.startsWith(';') || line.isEmpty())
            continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            group = line.mid(1, line.size() - 2).replace("][", "/");
        } else if (const auto separator = line.indexOf('='); separator > 0) {
            values.insert(group + '/' + line.left(separator).trimmed(), line.mid(separator + 1).trimmed());
        }
    }
    QJsonObject references;
    for (const auto& pair : QList<QPair<QString, QString>>{
             { "widgets", "kdeglobals/KDE/widgetStyle" }, { "palette", "kdeglobals/General/ColorScheme" },
             { "icons", "kdeglobals/Icons/Theme" } })
        if (!values.value(pair.second).isEmpty())
            references.insert(pair.first, values.value(pair.second));
    QDir directory = QFileInfo(defaults).dir();
    directory.cdUp();
    QJsonObject metadata;
    const auto manifest = directory.filePath("metadata.json");
    if (QFileInfo::exists(manifest)) {
        metadata = jsonFile(manifest, error);
        if (!error.isEmpty())
            return {};
    } else {
        QSettings desktop(directory.filePath("metadata.desktop"), QSettings::IniFormat);
        metadata.insert("KPlugin", QJsonObject{ { "Name", desktop.value("Desktop Entry/Name").toString() } });
        metadata.insert("X-KPackage-Dependencies", QJsonArray::fromStringList(
            desktop.value("Desktop Entry/X-KPackage-Dependencies").toString().split(';', Qt::SkipEmptyParts)));
    }
    QJsonArray dependencies;
    const auto declared = metadata.value("X-KPackage-Dependencies");
    const auto list = declared.isString() ? QJsonArray{ declared } : declared.toArray();
    if (list.size() > 32) {
        error = QObject::tr("The theme declares too many dependencies.");
        return {};
    }
    for (const auto& value : list) {
        const QUrl url(value.toString());
        const auto parts = url.path().split('/', Qt::SkipEmptyParts);
        QString category;
        if (url.host() == "icons.knsrc")
            category = "icons";
        else if (url.host() == "colorschemes.knsrc")
            category = "palette";
        else if (url.host() == "kvantum.knsrc")
            category = "widgets";
        const bool recognized = url.scheme() == "kns" && !category.isEmpty() && parts.size() == 2 &&
                                QStringList{ "api.kde-look.org", "api.opendesktop.org" }.contains(parts.value(0)) &&
                                QRegularExpression("^[0-9]{1,12}$").match(parts.value(1)).hasMatch() &&
                                url.userInfo().isEmpty() && !url.hasQuery() && !url.hasFragment();
        dependencies.append(QJsonObject{ { "reference", value }, { "supported", recognized },
                                          { "provider", "kde" }, { "category", category },
                                          { "contentId", recognized ? parts[1] : QString() } });
    }
    return { { "name", metadata.value("KPlugin").toObject().value("Name").toString(directory.dirName()) },
             { "references", references }, { "dependencies", dependencies } };
}

QJsonArray ThemePackage::inspect(const QDir& root, QString& error)
{
    QJsonArray components;
    QSet<QString> manifests;
    QStringList files;
    QDirIterator iterator(root.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext())
        files.append(iterator.next());
    std::sort(files.begin(), files.end());
    auto append = [&](const QDir& dir, const QString& relative, const QString& kind, const QString& format, const QString& name,
                      const QString& suffix = QString(), QJsonObject extra = {}) {
        const auto full = containedPath(dir, relative, error);
        if (full.isEmpty())
            return;
        const auto key = root.relativeFilePath(full) + suffix;
        const auto id = QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
        extra.insert("id", id);
        extra.insert("kind", kind);
        extra.insert("format", format);
        extra.insert("name", name);
        extra.insert("file", root.relativeFilePath(full));
        extra.insert("root", root.relativeFilePath(dir.absolutePath()));
        components.append(extra);
    };
    for (const auto& path : files) {
        QFileInfo info(path);
        if (info.fileName() != "theme.json")
            continue;
        const auto dir = info.dir();
        auto manifest = jsonFile(path, error);
        if (!error.isEmpty())
            return {};
        auto version = manifest.value("schemaVersion").toInt(1);
        if (version != 1 && version != 2) {
            error = QObject::tr("Unsupported theme schema version: %1").arg(version);
            return {};
        }
        const auto name = manifest.value("name").toString();
        if (name.isEmpty()) {
            error = QObject::tr("Theme name is missing.");
            return {};
        }
        manifests.insert(dir.absolutePath());
        const QJsonObject metadata{ { "version", manifest.value("version") },
                                    { "license", manifest.value("license") },
                                    { "description", manifest.value("description") },
                                    { "author", manifest.value("author") } };
        if (version == 1) {
            if (manifest.value("widgets").toString().isEmpty()) {
                error = QObject::tr("Legacy theme widget style is missing.");
                return {};
            }
            const auto qss = manifest.value("qssFilePath").toString("themeStyle.css");
            if (QFileInfo::exists(dir.filePath(qss))) {
                QFile stylesheet(containedPath(dir, qss, error));
                if (!error.isEmpty() || !stylesheet.open(QIODevice::ReadOnly) ||
                    !validateStyleSheet(QString::fromUtf8(stylesheet.readAll()), dir, error))
                    return {};
            }
            append(dir, "theme.json", "widgets", "prism", name, {}, metadata);
            continue;
        }
        const auto entries = manifest.value("components").toObject();
        if (entries.isEmpty()) {
            error = QObject::tr("The theme has no components.");
            return {};
        }
        const auto widgets = entries.value("widgets").toObject();
        if (!widgets.isEmpty()) {
            auto data = metadata;
            data.insert("engine", widgets.value("engine").toString("Fusion"));
            if (widgets.value("engine").toString().compare("Kvantum", Qt::CaseInsensitive) == 0) {
                const auto configuration = widgets.value("config").toString();
                const auto location = containedPath(dir, configuration, error);
                if (location.isEmpty() || !validateKvantum(location, error))
                    return {};
                data.insert("fixedColors", true);
                auto effects = readKvantumEffects(location, error);
                if (!error.isEmpty())
                    return {};
                data.insert("effects", effects);
                // The resource root is the SVG/config directory, not the bundle root.
                append(QFileInfo(location).dir(), QFileInfo(location).fileName(), "widgets", "kvantum", name, {}, data);
            } else {
                const auto qss = widgets.value("qss").toString();
                QFile input(containedPath(dir, qss, error));
                if (!error.isEmpty() || !input.open(QIODevice::ReadOnly) || !validateStyleSheet(QString::fromUtf8(input.readAll()), dir, error))
                    return {};
                data.insert("paletteBinding", widgets.value("paletteBinding").toString("fixed"));
                append(dir, qss, "widgets", "qss-v2", name, {}, data);
            }
        }
        const auto palettes = entries.value("palettes").toObject();
        for (auto it = palettes.begin(); it != palettes.end(); ++it) {
            const auto value = it.value().toObject();
            if (value.value("format").toString() != "kde-colors") {
                error = QObject::tr("Unsupported palette format.");
                return {};
            }
            Palette parsed;
            auto location = containedPath(dir, value.value("file").toString(), error);
            if (location.isEmpty() || !readColors(location, parsed, error))
                return {};
            auto data = metadata;
            data.insert("variant", it.key());
            data.insert("dark", parsed.dark);
            append(dir, value.value("file").toString(), "palette", "kde-colors", name + " / " + parsed.name, {}, data);
        }
        const auto icons = entries.value("icons").toObject();
        if (!icons.isEmpty()) {
            const auto location = containedPath(dir, icons.value("directory").toString() + "/index.theme", error);
            if (location.isEmpty())
                return {};
            QSettings index(location, QSettings::IniFormat);
            if (index.value("Icon Theme/Name").toString().isEmpty()) {
                error = QObject::tr("Icon theme name is missing.");
                return {};
            }
            append(QFileInfo(location).dir(), "index.theme", "icons", "freedesktop", name, {}, metadata);
        }
        const auto background = entries.value("background").toObject();
        if (!background.isEmpty()) {
            const auto location = containedPath(dir, background.value("manifest").toString(), error);
            if (location.isEmpty())
                return {};
            const auto data = jsonFile(location, error);
            const auto directory = QFileInfo(location).dir();
            if (!error.isEmpty() || containedPath(directory, data.value("default").toString(), error).isEmpty())
                return {};
            for (const auto& variant : data.value("variants").toArray())
                if (containedPath(directory, variant.toObject().value("path").toString(), error).isEmpty())
                    return {};
            append(directory, QFileInfo(location).fileName(), "background", "prism", name, {}, metadata);
        }
        const auto decoration = entries.value("decoration").toObject();
        if (!decoration.isEmpty()) {
            if (decoration.value("renderer").toString() != "luna-widgets") {
                error = QObject::tr("Unsupported window decoration renderer.");
                return {};
            }
            const auto location = containedPath(dir, decoration.value("config").toString(), error);
            auto config = jsonFile(location, error);
            if (!error.isEmpty())
                return {};
            for (auto it = config.begin(); it != config.end(); ++it) {
                if (!QStringList{ "height", "activeBackground", "inactiveBackground", "activeForeground", "inactiveForeground",
                                  "boldTitle" }
                         .contains(it.key())) {
                    error = QObject::tr("Unsupported window decoration option: %1").arg(it.key());
                    return {};
                }
                if (it.key() == "height" && (!it.value().isDouble() || it.value().toInt() < 28 || it.value().toInt() > 64)) {
                    error = QObject::tr("Decoration height must be between 28 and 64.");
                    return {};
                }
                if (it.key().endsWith("ground") && (!it.value().isString() || !QColor(it.value().toString()).isValid())) {
                    error = QObject::tr("Invalid decoration color.");
                    return {};
                }
                if (it.key() == "boldTitle" && !it.value().isBool()) {
                    error = QObject::tr("Invalid decoration title option.");
                    return {};
                }
            }
            auto data = metadata;
            data.insert("config", config);
            append(dir, decoration.value("config").toString(), "decoration", "luna-widgets", name, {}, data);
        }
    }
    for (const auto& path : files) {
        QFileInfo info(path);
        bool covered = false;
        for (const auto& dir : manifests)
            if (path.startsWith(dir + '/')) {
                covered = true;
                break;
            }
        if (covered)
            continue;
        if (info.fileName() == "defaults" && info.dir().dirName() == "contents") {
            auto data = readGlobalTheme(path, error);
            if (!error.isEmpty())
                return {};
            append(info.dir(), info.fileName(), "bundle", "kde-global", data.value("name").toString(), {}, data);
        } else if (info.suffix().compare("kvconfig", Qt::CaseInsensitive) == 0 &&
            QFileInfo::exists(info.dir().filePath(info.completeBaseName() + ".svg"))) {
            if (!validateKvantum(path, error))
                return {};
            auto effects = readKvantumEffects(path, error);
            if (!error.isEmpty())
                return {};
            append(info.dir(), info.fileName(), "widgets", "kvantum", info.completeBaseName(), {},
                   { { "engine", "Kvantum" }, { "fixedColors", true }, { "effects", effects } });
        } else if (info.suffix().compare("colors", Qt::CaseInsensitive) == 0 || info.fileName() == "colors") {
            Palette palette;
            if (!readColors(path, palette, error))
                return {};
            append(info.dir(), info.fileName(), "palette", "kde-colors", palette.name, {}, { { "dark", palette.dark } });
        } else if (info.fileName() == "index.theme") {
            QSettings settings(path, QSettings::IniFormat);
            const auto name = settings.value("Icon Theme/Name").toString();
            if (name.isEmpty()) {
                error = QObject::tr("Icon theme name is missing.");
                return {};
            }
            append(info.dir(), info.fileName(), "icons", "freedesktop", name);
        } else if (info.fileName() == "catpack.json") {
            auto data = jsonFile(path, error);
            if (!error.isEmpty())
                return {};
            if (containedPath(info.dir(), data.value("default").toString(), error).isEmpty())
                return {};
            for (const auto& variant : data.value("variants").toArray())
                if (containedPath(info.dir(), variant.toObject().value("path").toString(), error).isEmpty())
                    return {};
            append(info.dir(), info.fileName(), "background", "prism", data.value("name").toString(info.dir().dirName()));
        } else if (info.suffix() == "qss" || info.suffix() == "css") {
            if (info.suffix() == "css" && QRegularExpression("/gtk-[234](?:\\.[0-9]+)?/").match(path).hasMatch())
                continue;  // GTK stylesheets in desktop bundles are not Qt skins.
            QFile input(path);
            if (!input.open(QIODevice::ReadOnly) || !validateStyleSheet(QString::fromUtf8(input.readAll()), info.dir(), error))
                return {};
            append(info.dir(), info.fileName(), "widgets", "legacy-qss", info.completeBaseName());
        }
    }
    if (components.isEmpty())
        error = QObject::tr("This package contains no compatible theme components.");
    return components;
}
