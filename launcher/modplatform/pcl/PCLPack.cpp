// SPDX-License-Identifier: GPL-3.0-only
#include "PCLPack.h"

#include "Application.h"
#include "FileSystem.h"
#include "Json.h"
#include "icons/IconList.h"
#include "icons/IconUtils.h"
#include "minecraft/MinecraftInstance.h"
#include "settings/SettingsObject.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QtMath>

#include <exception>

namespace PCL {
namespace {

constexpr auto GENERATOR_ID = "luna-pcl-compat-v1";

bool parseBool(const QString& value, bool defaultValue = false)
{
    const auto normalized = value.trimmed().toLower();
    if (normalized == "true" || normalized == "1" || normalized == "yes")
        return true;
    if (normalized == "false" || normalized == "0" || normalized == "no")
        return false;
    return defaultValue;
}

bool containsPclRuntimeVariable(const QString& value)
{
    static const QRegularExpression marker(
        R"(\{(?:pcl_[^{}]+|identify|path|path_with_name|path_temp|date|time|java|minecraft|version_path|verpath|version_indie|verindie|name|version|forge|fabric|neoforge|liteloader|user|uuid|login|hint|cave|setup:[^{}]+|varible:[^{}]+|variable:[^{}]+)\})");
    return value.contains(marker);
}

QJsonObject localized(const QString& english, const QString& chinese)
{
    return { { "en_US", english }, { "zh_CN", chinese }, { "zh_TW", chinese } };
}

QJsonObject entry(const QString& key, const QString& value, const QString& status, const QString& target, const QString& detail)
{
    QJsonObject result{ { "key", key }, { "value", value }, { "status", status } };
    if (!target.isEmpty())
        result.insert("target", target);
    if (!detail.isEmpty())
        result.insert("detail", detail);
    return result;
}

bool writeJson(const QString& path, const QJsonObject& object, QString& error)
{
    try {
        FS::ensureFolderPathExists(QFileInfo(path).absolutePath());
        Json::write(object, path);
        return true;
    } catch (const std::exception& e) {
        error = QString::fromUtf8(e.what());
        return false;
    }
}

bool isGeneratedFile(const QString& path)
{
    QFile file(path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return false;
    const auto object = document.object();
    return object.value("generator").toString() == GENERATOR_ID || object.value("generatedBy").toString() == GENERATOR_ID;
}

QString replaceCommandVariables(QString value)
{
    value.replace("{java}java.exe", "${INST_JAVA}", Qt::CaseInsensitive);
    value.replace("{java}javaw.exe", "${INST_JAVA}", Qt::CaseInsensitive);
    value.replace("{version_path}", "${INST_DIR}", Qt::CaseInsensitive);
    value.replace("{verpath}", "${INST_DIR}", Qt::CaseInsensitive);
    value.replace("{version_indie}", "${INST_MC_DIR}", Qt::CaseInsensitive);
    value.replace("{verindie}", "${INST_MC_DIR}", Qt::CaseInsensitive);
    value.replace("{minecraft}", "${INST_MC_DIR}", Qt::CaseInsensitive);
    value.replace("{name}", "${INST_NAME}", Qt::CaseInsensitive);
    return value;
}

QString findBundledJava(const QString& root)
{
#ifdef Q_OS_WIN
    const QStringList names{ "javaw.exe", "java.exe" };
#else
    const QStringList names{ "java" };
#endif
    QDirIterator it(root, names, QDir::Files | QDir::Executable, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto path = it.next();
        if (QFileInfo(path).dir().dirName().compare("bin", Qt::CaseInsensitive) == 0)
            return path;
    }
    return {};
}

QJsonObject settingInput(const QString& id,
                         const QString& type,
                         const QString& setting,
                         const QString& english,
                         const QString& chinese)
{
    return { { "id", id },
             { "type", type },
             { "label_i18n", localized(english, chinese) },
             { "sourceSetting", setting },
             { "action", QJsonObject{ { "action", "setInstanceSetting" }, { "setting", setting } } } };
}

QJsonObject settingToggle(const QString& id, const QString& setting, const QString& english, const QString& chinese)
{
    return { { "id", id },
             { "type", "toggle" },
             { "label_i18n", localized(english, chinese) },
             { "sourceSetting", setting },
             { "action", QJsonObject{ { "action", "setInstanceSetting" }, { "setting", setting } } } };
}

bool normalizeInstanceJavaAgentPath(const QString& path, QString& normalizedPath)
{
    normalizedPath = path;
    normalizedPath.replace('\\', '/');
    normalizedPath = QDir::cleanPath(normalizedPath);
    static const QRegularExpression windowsAbsolutePath(R"(^[A-Za-z]:[/\\])");
    return !normalizedPath.isEmpty() && normalizedPath != "." && normalizedPath != ".." && !normalizedPath.startsWith("../") &&
           !QDir::isAbsolutePath(normalizedPath) && !normalizedPath.startsWith("//") &&
           !normalizedPath.contains(windowsAbsolutePath);
}

QString installPortableInstanceIcon(const QString& sourcePath)
{
    const auto suffix = QFileInfo(sourcePath).suffix().toLower();
    if (!IconUtils::isIconSuffix(suffix))
        return {};

    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly))
        return {};
    const auto data = source.readAll();
    const auto hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    const auto iconKey = QString("PCL_%1_Icon").arg(QString::fromLatin1(hash.toHex().left(12)));
    const auto iconName = iconKey + '.' + suffix;
    auto* icons = APPLICATION->icons();
    const auto installedPath = FS::PathCombine(icons->getDirectory(), iconName);

    if (!QFileInfo::exists(installedPath)) {
        try {
            FS::write(installedPath, data);
        } catch (const std::exception&) {
            return {};
        }
    }
    if (!QFileInfo(installedPath).isFile() || !icons->addIcon(iconKey, iconKey, installedPath, IconType::FileBased))
        return {};
    return iconKey;
}

bool installBundledIconLicense(const QString& instanceRoot)
{
    QFile source(":/pcl/lucide/LICENSE.txt");
    if (!source.open(QIODevice::ReadOnly))
        return false;
    const auto data = source.readAll();
    const auto hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex().left(12);
    const auto fileName = QString("PCL-LUCIDE-LICENSE-%1.txt").arg(QString::fromLatin1(hash));
    const auto targetPath = FS::PathCombine(instanceRoot, fileName);

    QFile existing(targetPath);
    if (existing.exists())
        return existing.open(QIODevice::ReadOnly) && existing.readAll() == data;
    try {
        FS::write(targetPath, data);
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

}  // namespace

Setup parseSetup(const QByteArray& data)
{
    Setup setup;
    setup.sourceSha256 = QCryptographicHash::hash(data, QCryptographicHash::Sha256);

    const auto text = QString::fromUtf8(data);
    for (const auto& rawLine : text.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts)) {
        const auto line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';'))
            continue;
        const auto separator = line.indexOf(':');
        if (separator <= 0)
            continue;
        setup.values.insert(line.left(separator).trimmed(), line.mid(separator + 1));
    }
    return setup;
}

InstanceConfig parseInstanceConfig(const QByteArray& data)
{
    InstanceConfig config;
    config.sourceSha256 = QCryptographicHash::hash(data, QCryptographicHash::Sha256);

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        config.error = parseError.errorString();
        return config;
    }
    if (!document.isObject()) {
        config.error = "PCL config root must be an object";
        return config;
    }

    const auto root = document.object();
    config.valid = true;
    config.migratedJava = root.value("InstanceMigratedJava").toBool(false);
    const auto forcedJava = root.value("InstanceForcedJava").toObject();
    config.javaFolder = forcedJava.value("Folder").toString();
    const auto version = forcedJava.value("Version").toObject();
    config.javaMajor = version.value("Major").toInt(-1);
    config.javaMinor = version.value("Minor").toInt(-1);
    config.javaBuild = version.value("Build").toInt(-1);
    config.javaRevision = version.value("Revision").toInt(-1);
    config.javaMajorRevision = version.value("MajorRevision").toInt(-1);
    config.javaMinorRevision = version.value("MinorRevision").toInt(-1);
    return config;
}

int customRamMegabytes(int value)
{
    double gib = 0;
    if (value <= 12)
        gib = value * 0.1 + 0.3;
    else if (value <= 25)
        gib = (value - 12) * 0.5 + 1.5;
    else if (value <= 33)
        gib = (value - 25) + 8;
    else
        gib = (value - 33) * 2 + 16;
    return qRound(gib * 1024);
}

QStringList findNestedPackCandidates(const QStringList& archiveFiles)
{
    QStringList result;
    for (const auto& rawPath : archiveFiles) {
        const auto path = QDir::fromNativeSeparators(rawPath);
        const auto parts = path.split('/', Qt::SkipEmptyParts);
        if (parts.size() < 1 || parts.size() > 2)
            continue;
        const auto name = parts.constLast();
        if (name.compare("modpack.mrpack", Qt::CaseInsensitive) == 0 || name.compare("modpack.zip", Qt::CaseInsensitive) == 0)
            result.append(rawPath);
    }
    result.removeDuplicates();
    return result;
}

QStringList javaAgentPaths(const QString& arguments)
{
    QStringList result;
    constexpr auto prefix = "-javaagent:";
    for (const auto& argument : QProcess::splitCommand(arguments)) {
        if (!argument.startsWith(prefix, Qt::CaseInsensitive))
            continue;

        auto specification = argument.mid(QLatin1String(prefix).size());
        const auto jarSuffix = specification.lastIndexOf(".jar", -1, Qt::CaseInsensitive);
        if (jarSuffix >= 0 && (jarSuffix + 4 == specification.size() || specification.at(jarSuffix + 4) == '=')) {
            specification.truncate(jarSuffix + 4);
        } else {
            const auto optionSeparator = specification.indexOf('=');
            if (optionSeparator >= 0)
                specification.truncate(optionSeparator);
        }
        if (!specification.isEmpty())
            result.append(specification);
    }
    result.removeDuplicates();
    return result;
}

QStringList unavailableJavaAgentFiles(const QString& arguments, const QString& gameRoot)
{
    QStringList result;
    const QDir root(gameRoot);
    const auto rootUrl = QUrl::fromLocalFile(root.absolutePath() + '/');

    for (const auto& originalPath : javaAgentPaths(arguments)) {
        QString normalizedPath;
        const bool instanceRelative = normalizeInstanceJavaAgentPath(originalPath, normalizedPath);
        const auto resolvedPath = root.absoluteFilePath(normalizedPath);
        const QFileInfo resolvedFile(resolvedPath);
        if (!instanceRelative || !rootUrl.isParentOf(QUrl::fromLocalFile(resolvedPath)) || !resolvedFile.isFile() ||
            resolvedFile.isSymLink())
            result.append(originalPath);
    }
    return result;
}

QString pclBuiltinIconCandidate(const QString& logo)
{
    static const QRegularExpression resourcePath(R"(/Images/Blocks/([^/]+)\.(?:png|jpg|jpeg|ico)$)",
                                                 QRegularExpression::CaseInsensitiveOption);
    const auto match = resourcePath.match(logo);
    if (!match.hasMatch())
        return {};

    const auto name = match.captured(1).toLower();
    static const QSet<QString> exactResources{ "anvil", "egg" };
    return exactResources.contains(name) ? name : QString{};
}

ConversionResult convertInstanceConfig(MinecraftInstance& instance)
{
    ConversionResult result;
    const auto sourcePath = FS::PathCombine(instance.gameRoot(), "PCL", "Setup.ini");
    QFile source(sourcePath);
    const bool setupFound = source.exists();
    const auto configPath = FS::PathCombine(instance.gameRoot(), "PCL", "config.json");
    QFile configSource(configPath);
    const bool configFound = configSource.exists();
    if (!setupFound && !configFound)
        return result;
    result.found = true;

    Setup setup;
    if (setupFound) {
        if (!source.open(QIODevice::ReadOnly)) {
            result.error = source.errorString();
            return result;
        }
        setup = parseSetup(source.readAll());
    }

    InstanceConfig pclConfig;
    if (configFound) {
        if (configSource.open(QIODevice::ReadOnly))
            pclConfig = parseInstanceConfig(configSource.readAll());
        else
            pclConfig.error = configSource.errorString();
    }
    auto* settings = instance.settings();
    QSet<QString> handled;
    auto record = [&result, &handled](const QString& key,
                                     const QString& value,
                                     const QString& status,
                                     const QString& target = {},
                                     const QString& detail = {}) {
        handled.insert(key);
        result.entries.append(entry(key, value, status, target, detail));
    };
    auto apply = [settings](const QString& key, const QVariant& value) { return settings->set(key, value); };
    QString bundledJavaCandidate;
    QString forcedJavaVersion;
    QString forcedJavaSummary;
    bool needsJavaSelection = false;
    QStringList unavailableJvmAgents;

    if (configFound) {
        if (!pclConfig.valid) {
            record("PCL/config.json", {}, "invalid", {}, pclConfig.error.isEmpty() ? "Invalid PCL instance config" : pclConfig.error);
        } else if (!pclConfig.migratedJava) {
            record("InstanceMigratedJava", "false", "mapped-noop", {}, "No migrated PCL Java selection is active");
        } else if (pclConfig.javaFolder.isEmpty() || pclConfig.javaMajor < 0) {
            record("InstanceMigratedJava", "true", "invalid", {}, "InstanceForcedJava is missing a folder or Java version");
        } else {
            forcedJavaVersion = QString("%1.%2.%3.%4")
                                    .arg(pclConfig.javaMajor)
                                    .arg(qMax(pclConfig.javaMinor, 0))
                                    .arg(qMax(pclConfig.javaBuild, 0))
                                    .arg(qMax(pclConfig.javaRevision, 0));
            forcedJavaSummary = QString("PCL selected Java %1 from %2").arg(forcedJavaVersion, pclConfig.javaFolder);
            needsJavaSelection = true;

            QString javaCandidate;
#ifdef Q_OS_WIN
            const QDir javaFolder(QDir::fromNativeSeparators(pclConfig.javaFolder));
            for (const auto& executable : { "javaw.exe", "java.exe" }) {
                const auto candidate = javaFolder.filePath(executable);
                if (QFileInfo(candidate).isFile()) {
                    javaCandidate = candidate;
                    break;
                }
            }
#endif
            if (!javaCandidate.isEmpty()) {
                apply("JavaPath", javaCandidate);
                record("InstanceForcedJava", pclConfig.javaFolder, "mapped-with-prompt", "OverrideJavaLocation=false, JavaPath",
                       forcedJavaSummary + "; the executable still exists but must be enabled explicitly in Luna");
            } else {
                record("InstanceForcedJava", pclConfig.javaFolder, "mapped-with-prompt", "OverrideJavaLocation, JavaPath",
                       forcedJavaSummary + "; the source-machine path is unavailable, select an equivalent Java in Luna");
            }
            record("InstanceMigratedJava", "true", "mapped", "PCL config migration metadata");
        }
    }

    if (auto value = setup.values.value("CustomInfo"); !value.isEmpty()) {
        instance.setNotes(value);
        record("CustomInfo", value, "mapped", "notes");
    }

    if (auto value = setup.values.value("VersionAdvanceJvm"); !value.isEmpty()) {
        if (containsPclRuntimeVariable(value)) {
            record("VersionAdvanceJvm", value, "mapped-with-prompt", {},
                   "PCL runtime replacement markers cannot be evaluated safely during import");
        } else {
            apply("JvmArgs", value);
            unavailableJvmAgents = unavailableJavaAgentFiles(value, instance.gameRoot());
            if (unavailableJvmAgents.isEmpty()) {
                apply("OverrideJavaArgs", true);
                record("VersionAdvanceJvm", value, "mapped", "OverrideJavaArgs, JvmArgs");
            } else {
                apply("OverrideJavaArgs", false);
                record("VersionAdvanceJvm", value, "mapped-with-prompt", "OverrideJavaArgs=false, JvmArgs",
                       QString("Missing or external JVM agent files: %1. Relative paths must be placed at the same path under the instance game directory; absolute or parent paths must first be changed to an instance-relative path")
                           .arg(unavailableJvmAgents.join(", ")));
            }
        }
    }

    if (auto value = setup.values.value("VersionAdvanceGame"); !value.isEmpty()) {
        if (containsPclRuntimeVariable(value)) {
            record("VersionAdvanceGame", value, "mapped-with-prompt", {},
                   "PCL runtime replacement markers cannot be evaluated safely during import");
        } else {
            apply("ExtraGameArgs", value);
            record("VersionAdvanceGame", value, "mapped", "ExtraGameArgs");
        }
    }

    if (auto value = setup.values.value("VersionServerEnter"); !value.isEmpty()) {
        apply("JoinServerOnLaunch", true);
        apply("JoinServerOnLaunchAddress", value);
        record("VersionServerEnter", value, "mapped", "JoinServerOnLaunch, JoinServerOnLaunchAddress");
    }

    if (auto value = setup.values.value("VersionArgumentTitle"); !value.isEmpty()) {
        if (containsPclRuntimeVariable(value)) {
            record("VersionArgumentTitle", value, "mapped-with-prompt", {},
                   "Dynamic PCL window-title markers have no exact Luna equivalent");
        } else {
            apply("OverrideWindowTitle", true);
            apply("WindowTitle", value);
            record("VersionArgumentTitle", value, "mapped", "OverrideWindowTitle, WindowTitle");
        }
    }

    if (auto value = setup.values.value("VersionArgumentInfo"); !value.isEmpty()) {
        if (containsPclRuntimeVariable(value)) {
            record("VersionArgumentInfo", value, "mapped-with-prompt", {},
                   "PCL runtime replacement markers cannot be evaluated safely during import");
        } else {
            apply("OverrideVersionType", true);
            apply("VersionType", value);
            record("VersionArgumentInfo", value, "mapped", "OverrideVersionType, VersionType");
        }
    }

    if (setup.values.contains("VersionRamType")) {
        const auto value = setup.values.value("VersionRamType");
        bool ok = false;
        const auto type = value.toInt(&ok);
        if (!ok) {
            record("VersionRamType", value, "invalid", {}, "Not an integer");
        } else if (type == 2) {
            apply("OverrideMemory", false);
            record("VersionRamType", value, "mapped", "OverrideMemory=false");
        } else if (type == 1 && setup.values.contains("VersionRamCustom")) {
            bool customOk = false;
            const auto custom = setup.values.value("VersionRamCustom").toInt(&customOk);
            if (customOk && custom >= 0 && custom <= 49) {
                const auto memory = customRamMegabytes(custom);
                apply("OverrideMemory", true);
                apply("MaxMemAlloc", memory);
                record("VersionRamType", value, "mapped", "OverrideMemory=true");
                record("VersionRamCustom", setup.values.value("VersionRamCustom"), "mapped", QString("MaxMemAlloc=%1").arg(memory));
            } else {
                record("VersionRamType", value, "invalid", {}, "VersionRamCustom must be a PCL slider value from 0 to 49");
                record("VersionRamCustom", setup.values.value("VersionRamCustom"), "invalid");
            }
        } else if (type == 1) {
            record("VersionRamType", value, "invalid", {}, "Custom memory mode requires VersionRamCustom");
        } else if (type == 0) {
            record("VersionRamType", value, "unsupported", {}, "PCL dynamic memory allocation has no exact Luna equivalent");
        } else {
            record("VersionRamType", value, "invalid", {}, "Unknown PCL memory mode");
        }
    }
    if (setup.values.contains("VersionRamCustom") && !handled.contains("VersionRamCustom")) {
        bool typeOk = false;
        const auto type = setup.values.value("VersionRamType").toInt(&typeOk);
        record("VersionRamCustom", setup.values.value("VersionRamCustom"), typeOk && type != 1 ? "mapped-noop" : "unsupported", {},
               typeOk && type != 1 ? "The selected PCL memory mode does not use the custom slider value"
                                   : "VersionRamCustom cannot be interpreted without a valid custom memory mode");
    }

    if (setup.values.contains("VersionArgumentJavaV2")) {
        const auto value = setup.values.value("VersionArgumentJavaV2");
        bool ok = false;
        const auto type = value.toInt(&ok);
        if (!ok) {
            record("VersionArgumentJavaV2", value, "invalid", {}, "Not an integer");
        } else if (type == 0) {
            apply("OverrideJavaLocation", false);
            record("VersionArgumentJavaV2", value, "mapped", "OverrideJavaLocation=false");
        } else if (type == 2) {
            const auto java = findBundledJava(instance.gameRoot());
            if (!java.isEmpty()) {
                // Keep the executable selected but disabled until the user explicitly trusts it.
                apply("JavaPath", java);
                bundledJavaCandidate = java;
                record("VersionArgumentJavaV2", value, "mapped-with-prompt", "OverrideJavaLocation, JavaPath",
                       "A bundled executable was found; enable it explicitly in the generated LunaUI after verifying the pack");
            } else {
                record("VersionArgumentJavaV2", value, "mapped-with-prompt", {}, "Bundled Java was requested but no Java executable was found");
            }
        } else if (type == 1) {
            record("VersionArgumentJavaV2", value, "mapped-with-prompt", "VersionArgumentJavaRange",
                   "Luna can select a compatible Java major, but PCL exact ranges require validation");
        } else if (type == 3) {
            record("VersionArgumentJavaV2", value, "mapped-with-prompt", {}, "A specific Java must be selected in Luna");
        } else {
            record("VersionArgumentJavaV2", value, "invalid", {}, "Unknown PCL Java mode");
        }
    }

    if (parseBool(setup.values.value("LogoCustom")) && setup.values.contains("Logo")) {
        const auto relative = QDir::fromNativeSeparators(setup.values.value("Logo"));
        const auto logoPath = QDir(instance.gameRoot()).absoluteFilePath(relative);
        const QFileInfo logoInfo(logoPath);
        const auto canonicalRoot = QFileInfo(instance.gameRoot()).canonicalFilePath();
        const auto canonicalLogo = logoInfo.canonicalFilePath();
        const auto rootUrl = QUrl::fromLocalFile(canonicalRoot + '/');
        if (!QDir::isAbsolutePath(relative) && !canonicalRoot.isEmpty() && !canonicalLogo.isEmpty() && logoInfo.isFile() &&
            rootUrl.isParentOf(QUrl::fromLocalFile(canonicalLogo)) && IconUtils::isIconSuffix(logoInfo.suffix().toLower())) {
            const auto iconKey = installPortableInstanceIcon(canonicalLogo);
            if (!iconKey.isEmpty()) {
                instance.setIconKey(iconKey);
                record("Logo", setup.values.value("Logo"), "mapped", QString("iconKey=%1").arg(iconKey));
                record("LogoCustom", setup.values.value("LogoCustom"), "mapped", QString("iconKey=%1").arg(iconKey));
            } else {
                record("Logo", setup.values.value("Logo"), "invalid", {}, "Could not install the icon");
                record("LogoCustom", setup.values.value("LogoCustom"), "invalid");
            }
        } else {
            record("Logo", setup.values.value("Logo"), "invalid", {}, "Logo path is missing, unsafe, or unsupported");
            record("LogoCustom", setup.values.value("LogoCustom"), "invalid");
        }
    } else if (setup.values.contains("Logo")) {
        const auto value = setup.values.value("Logo");
        const auto resourceName = pclBuiltinIconCandidate(value);
        const auto resourcePath = QString(":/pcl/lucide/%1.svg").arg(resourceName);
        const auto iconKey = resourceName.isEmpty() ? QString{} : installPortableInstanceIcon(resourcePath);
        if (!iconKey.isEmpty() && installBundledIconLicense(instance.instanceRoot())) {
            instance.setIconKey(iconKey);
            record("Logo", value, "mapped", QString("iconKey=%1").arg(iconKey),
                   "An exact same-name bundled resource was installed as a portable MMC custom icon with its license notice");
            if (setup.values.contains("LogoCustom"))
                record("LogoCustom", setup.values.value("LogoCustom"), "mapped", QString("iconKey=%1").arg(iconKey));
        } else if (!resourceName.isEmpty()) {
            record("Logo", value, "invalid", {}, "Could not install the bundled icon or its license notice");
            if (setup.values.contains("LogoCustom"))
                record("LogoCustom", setup.values.value("LogoCustom"), "mapped-noop", {}, "No custom PCL icon is enabled");
        } else {
            record("Logo", value, "unsupported", {}, "The referenced PCL built-in icon has no exact same-name bundled resource");
            if (setup.values.contains("LogoCustom"))
                record("LogoCustom", setup.values.value("LogoCustom"), "mapped-noop", {}, "No custom PCL icon is enabled");
        }
    } else if (setup.values.contains("LogoCustom")) {
        record("LogoCustom", setup.values.value("LogoCustom"), "mapped-noop", {}, "No PCL icon is selected");
    }

    for (const auto& key : { "VersionArgumentIndie", "VersionArgumentIndieV2", "VersionAdvanceDisableJLW", "VersionAdvanceDisableLUA" }) {
        if (setup.values.contains(key))
            record(key, setup.values.value(key), "mapped-noop", {}, "Luna instances are isolated and do not use PCL launcher patches");
    }

    for (const auto& key : { "CustomInfo", "VersionAdvanceJvm", "VersionAdvanceGame", "VersionServerEnter", "VersionArgumentTitle",
                             "VersionArgumentInfo", "VersionAdvanceRun" }) {
        if (setup.values.contains(key) && setup.values.value(key).isEmpty() && !handled.contains(key))
            record(key, {}, "mapped-noop", {}, "The PCL override is empty");
    }

    if (setup.values.contains("VersionAdvanceRun") && !setup.values.value("VersionAdvanceRun").isEmpty()) {
        const auto wait = !setup.values.contains("VersionAdvanceRunWait") || parseBool(setup.values.value("VersionAdvanceRunWait"), true);
#ifdef Q_OS_WIN
        if (wait) {
            apply("AppendPreLaunchCommand", replaceCommandVariables(setup.values.value("VersionAdvanceRun")));
            apply("AppendPreLaunchCommandUseShell", true);
            apply("EnableAppendPreLaunchCommand", false);
            record("VersionAdvanceRun", setup.values.value("VersionAdvanceRun"), "mapped-with-prompt", "AppendPreLaunchCommand",
                   "The command is retained but disabled until explicitly enabled in the generated LunaUI");
            record("VersionAdvanceRunWait", setup.values.value("VersionAdvanceRunWait", "True"), "mapped", {},
                   "When enabled, the appended command completes before game launch");
        }
#endif
        if (!handled.contains("VersionAdvanceRun")) {
            record("VersionAdvanceRun", setup.values.value("VersionAdvanceRun"), "unsupported", {},
                   wait ? "PCL commands use Windows cmd.exe and cannot be mapped on this platform"
                        : "Luna does not yet have an asynchronous pre-launch command mode");
        }
    }

    if (setup.values.contains("VersionAdvanceRunWait") && !handled.contains("VersionAdvanceRunWait")) {
        const auto wait = parseBool(setup.values.value("VersionAdvanceRunWait"), true);
        record("VersionAdvanceRunWait", setup.values.value("VersionAdvanceRunWait"), wait ? "mapped-noop" : "unsupported", {},
               wait ? "Waiting is Luna's default pre-launch command behavior"
                    : "Luna does not yet have an asynchronous pre-launch command mode");
    }

    if (setup.values.contains("VersionServerLogin")) {
        const auto value = setup.values.value("VersionServerLogin");
        bool ok = false;
        const auto type = value.toInt(&ok);
        if (!ok) {
            record("VersionServerLogin", value, "invalid", {}, "Not an integer");
        } else if (type >= 0 && type <= 4) {
            record("VersionServerLogin", value, type == 0 ? "mapped-noop" : "mapped-with-prompt", "InstanceAccountId",
                   type == 0 ? "Luna will use the account selected at launch" : "Select or add the matching account in Luna");
        } else {
            record("VersionServerLogin", value, "invalid", {}, "Unknown PCL login mode");
        }
    }

    bool serverModeOk = false;
    const auto serverMode = setup.values.value("VersionServerLogin").toInt(&serverModeOk);
    if (setup.values.contains("VersionServerNide")) {
        const auto value = setup.values.value("VersionServerNide");
        if (serverModeOk && serverMode == 3) {
            record("VersionServerNide", value, value.isEmpty() ? "invalid" : "mapped-with-prompt", "UnifiedPass account",
                   value.isEmpty() ? "UnifiedPass mode requires a server ID" : "Select or add the UnifiedPass account for this server ID");
        } else {
            record("VersionServerNide", value, "mapped-noop", {}, "The selected PCL login mode does not use UnifiedPass");
        }
    }
    for (const auto& key : { "VersionServerAuthServer", "VersionServerAuthRegister", "VersionServerAuthName" }) {
        if (!setup.values.contains(key))
            continue;
        const auto value = setup.values.value(key);
        if (serverModeOk && serverMode == 4) {
            const bool requiredAndMissing = QString::fromLatin1(key) == QLatin1String("VersionServerAuthServer") && value.isEmpty();
            record(key, value, requiredAndMissing ? "invalid" : (value.isEmpty() ? "mapped-noop" : "mapped-with-prompt"),
                   "Yggdrasil account",
                   requiredAndMissing ? "Authlib Injector mode requires an authentication server"
                                      : (value.isEmpty() ? "The optional PCL account field is empty"
                                                         : "Use this value when adding or selecting the matching Yggdrasil account in Luna"));
        } else {
            record(key, value, "mapped-noop", {}, "The selected PCL login mode does not use Authlib Injector");
        }
    }

    for (const auto& key : { "VersionAdvanceAssetsV2", "VersionAdvanceDisableModUpdate" }) {
        if (setup.values.contains(key)) {
            const auto enabled = parseBool(setup.values.value(key));
            record(key, setup.values.value(key), enabled ? "unsupported" : "mapped-noop", {},
                   enabled ? "The requested restriction has no exact Luna instance setting" : "The PCL option is disabled");
        }
    }
    for (const auto& key : { "VersionAdvanceAssets", "VersionRamOptimize", "VersionAdvanceGC" }) {
        if (setup.values.contains(key)) {
            bool ok = false;
            const auto value = setup.values.value(key).toInt(&ok);
            record(key, setup.values.value(key), ok && value == 0 ? "mapped-noop" : "unsupported", {},
                   ok && value == 0 ? "The PCL option is disabled" : "No safe, equivalent Luna mapping is available");
        }
    }

    if (setup.values.contains("VersionArgumentJavaRange") && !handled.contains("VersionArgumentJavaRange")) {
        const auto javaMode = setup.values.value("VersionArgumentJavaV2").toInt();
        record("VersionArgumentJavaRange", setup.values.value("VersionArgumentJavaRange"),
               javaMode == 1 ? "mapped-with-prompt" : "mapped-noop", {},
               javaMode == 1 ? "Validate and select a compatible Java in Luna" : "The selected PCL Java mode does not use this range");
    }

    if (setup.values.contains("VersionArgumentJavaSelect")) {
        const auto value = setup.values.value("VersionArgumentJavaSelect");
        const auto javaMode = setup.values.value("VersionArgumentJavaV2").toInt();
        record("VersionArgumentJavaSelect", value, javaMode == 3 ? "mapped-with-prompt" : "mapped-noop",
               javaMode == 3 ? "JavaPath" : QString(),
               javaMode == 3 ? "Select the corresponding local Java installation in Luna"
                             : "The selected PCL Java mode does not use this cached local selection");
    }

    const QSet<QString> ignoredCache{ "State",          "Info",            "ReleaseTime",      "VersionFabric",
                                      "VersionForge",   "VersionNeoForge", "VersionOptiFine",  "VersionLiteLoader",
                                      "VersionVanilla", "VersionVanillaName" };
    for (auto it = setup.values.cbegin(); it != setup.values.cend(); ++it) {
        if (handled.contains(it.key()))
            continue;
        if (ignoredCache.contains(it.key()))
            record(it.key(), it.value(), "ignored-cache", {}, "Luna uses the mrpack component metadata");
        else
            record(it.key(), it.value(), "unsupported", {}, "No safe, equivalent Luna mapping is available");
    }

    const auto lunaRoot = FS::PathCombine(instance.instanceRoot(), "lunaui");
    const auto primarySourceHash = setupFound ? setup.sourceSha256 : pclConfig.sourceSha256;
    const auto sourceHash = QString::fromLatin1(primarySourceHash.toHex());
    QJsonObject manifest{ { "formatVersion", 1 },
                          { "generator", GENERATOR_ID },
                          { "source", setupFound ? "minecraft/PCL/Setup.ini" : "minecraft/PCL/config.json" },
                          { "sourceSha256", sourceHash } };
    if (configFound) {
        manifest.insert("configSource", "minecraft/PCL/config.json");
        if (!pclConfig.sourceSha256.isEmpty())
            manifest.insert("configSourceSha256", QString::fromLatin1(pclConfig.sourceSha256.toHex()));
    }
    QJsonObject report = manifest;
    report.insert("entries", result.entries);

    QJsonArray controls;
    controls.append(QJsonObject{
        { "type", "text" },
        { "text_i18n",
          localized(setupFound && configFound
                        ? "Settings converted from PCL. The original Setup.ini and config.json are retained."
                        : (setupFound ? "Settings converted from PCL. The original Setup.ini is retained."
                                      : "Settings converted from PCL. The original config.json is retained."),
                    setupFound && configFound
                        ? "以下设置由 PCL 配置转换而来，原始 Setup.ini 和 config.json 已保留。"
                        : (setupFound ? "以下设置由 PCL 配置转换而来，原始 Setup.ini 已保留。"
                                      : "以下设置由 PCL 配置转换而来，原始 config.json 已保留。")) }
    });
    if (settings->get("notes").toString().size())
        controls.append(settingInput("pcl_notes", "textarea", "notes", "Instance notes", "实例备注"));
    if (!settings->get("JvmArgs").toString().isEmpty()) {
        if (!unavailableJvmAgents.isEmpty()) {
            controls.append(QJsonObject{
                { "type", "text" },
                { "text_i18n",
                  localized(QString("Missing or external JVM agent files: %1. Put each relative path at the same location under the instance game directory; for example, GraphicsFixer.jar belongs directly in <instance>/minecraft/. Change absolute or parent paths to paths inside the instance before enabling these arguments.")
                                .arg(unavailableJvmAgents.join(", ")),
                            QString("缺失或位于实例外部的 JVM 代理文件：%1。相对路径应按原目录结构放入实例游戏目录；例如 GraphicsFixer.jar 应直接放在 <实例>/minecraft/ 下。绝对路径或越界路径必须先改成实例内路径，再启用这些参数。")
                                .arg(unavailableJvmAgents.join(", "))) }
            });

            const auto gameRootRelative = QDir::fromNativeSeparators(QDir(instance.instanceRoot()).relativeFilePath(instance.gameRoot()));
            int openFolderIndex = 0;
            for (const auto& agentPath : unavailableJvmAgents) {
                QString normalizedAgentPath;
                if (!normalizeInstanceJavaAgentPath(agentPath, normalizedAgentPath))
                    continue;
                const auto agentDirectory = QFileInfo(normalizedAgentPath).path();
                const auto destination = QDir::cleanPath(
                    agentDirectory == "." ? gameRootRelative : QDir(gameRootRelative).filePath(agentDirectory));
                controls.append(QJsonObject{
                    { "id", QString("pcl_open_agent_dir_%1").arg(openFolderIndex++) },
                    { "type", "button" },
                    { "icon", "viewfolder" },
                    { "text_i18n",
                      localized(QString("Open destination for %1").arg(agentPath), QString("打开 %1 的放置目录").arg(agentPath)) },
                    { "tooltip_i18n",
                      localized(QString("Instance path: %1").arg(destination), QString("实例内路径：%1").arg(destination)) },
                    { "action", QJsonObject{ { "action", "openFolder" }, { "path", destination }, { "create", true } } }
                });
            }
        }
        controls.append(settingToggle("pcl_override_jvm", "OverrideJavaArgs", "Use instance JVM arguments", "使用实例 JVM 参数"));
        controls.append(settingInput("pcl_jvm_args", "textarea", "JvmArgs", "JVM arguments", "JVM 参数"));
    }
    if (settings->get("OverrideJavaLocation").toBool())
        controls.append(settingInput("pcl_java_path", "input", "JavaPath", "Java executable", "Java 可执行文件"));
    if (!forcedJavaSummary.isEmpty()) {
        controls.append(QJsonObject{
            { "type", "text" },
            { "text_i18n",
              localized(QString("PCL selected Java %1 from %2. This source-machine path was not enabled automatically; select or verify an equivalent Java installation.")
                            .arg(forcedJavaVersion, pclConfig.javaFolder),
                        QString("PCL 选择了位于 %2 的 Java %1。该来源机器路径不会自动启用，请选择或确认等价的 Java 安装。")
                            .arg(forcedJavaVersion, pclConfig.javaFolder)) }
        });
    }
    if (!bundledJavaCandidate.isEmpty()) {
        controls.append(settingToggle("pcl_enable_bundled_java", "OverrideJavaLocation", "Trust and use bundled Java",
                                      "信任并使用整合包内置 Java"));
        auto javaCandidate = settingInput("pcl_bundled_java", "input", "JavaPath", "Bundled Java candidate", "内置 Java 候选路径");
        javaCandidate.remove("sourceSetting");
        javaCandidate.insert("default", bundledJavaCandidate);
        controls.append(javaCandidate);
    }
    if (needsJavaSelection || setup.values.value("VersionArgumentJavaV2").toInt() == 1 ||
        setup.values.value("VersionArgumentJavaV2").toInt() == 3) {
        controls.append(settingToggle("pcl_override_java", "OverrideJavaLocation", "Use a specific Java executable", "使用指定 Java 可执行文件"));
        controls.append(settingInput("pcl_java_path_prompt", "input", "JavaPath", "Java executable", "Java 可执行文件"));
    }
    bool loginModeOk = false;
    const auto loginMode = setup.values.value("VersionServerLogin").toInt(&loginModeOk);
    if (loginModeOk && loginMode >= 1 && loginMode <= 4) {
        controls.append(QJsonObject{ { "id", "pcl_account" },
                                     { "type", "accountselect" },
                                     { "label_i18n", localized("Account for this instance", "此实例使用的账户") } });
    }
    if (settings->get("JoinServerOnLaunch").toBool()) {
        controls.append(settingToggle("pcl_join_server", "JoinServerOnLaunch", "Join a server on launch", "启动时进入服务器"));
        controls.append(settingInput("pcl_server", "input", "JoinServerOnLaunchAddress", "Server to join on launch", "启动时进入的服务器"));
    }
    if (settings->get("OverrideMemory").toBool()) {
        controls.append(settingToggle("pcl_override_memory", "OverrideMemory", "Use instance memory settings", "使用实例内存设置"));
        controls.append(settingInput("pcl_memory", "number", "MaxMemAlloc", "Maximum memory (MiB)", "最大内存 (MiB)"));
    }
    if (!settings->get("ExtraGameArgs").toString().isEmpty())
        controls.append(settingInput("pcl_game_args", "textarea", "ExtraGameArgs", "Additional game arguments", "附加游戏参数"));
    if (!settings->get("AppendPreLaunchCommand").toString().isEmpty()) {
        controls.append(settingToggle("pcl_enable_prelaunch", "EnableAppendPreLaunchCommand", "Trust and run this command before launch",
                                      "信任并在启动前运行此命令"));
        controls.append(settingInput("pcl_prelaunch", "textarea", "AppendPreLaunchCommand", "Appended pre-launch command", "追加的启动前命令"));
    }
    if (settings->get("OverrideWindowTitle").toBool()) {
        controls.append(settingToggle("pcl_override_title", "OverrideWindowTitle", "Use a custom game window title", "使用自定义游戏窗口标题"));
        controls.append(settingInput("pcl_window_title", "input", "WindowTitle", "Game window title", "游戏窗口标题"));
    }
    if (settings->get("OverrideVersionType").toBool()) {
        controls.append(settingToggle("pcl_override_version_type", "OverrideVersionType", "Use a custom version type", "使用自定义版本类型"));
        controls.append(settingInput("pcl_version_type", "input", "VersionType", "Version type", "版本类型"));
    }

    int unsupportedCount = 0;
    QStringList unsupportedKeys;
    for (const auto& item : result.entries) {
        const auto object = item.toObject();
        const auto status = object.value("status").toString();
        if (status == "unsupported" || status == "invalid" || status == "mapped-with-prompt" || status == "mapped-approximate") {
            unsupportedCount++;
            unsupportedKeys.append(object.value("key").toString());
        }
    }
    if (unsupportedCount) {
        controls.append(QJsonObject{ { "type", "separator" } });
        controls.append(QJsonObject{ { "type", "text" },
                                     { "text_i18n",
                                       localized(QString("%1 setting(s) need attention: %2. See migration/pcl-report.json for details.")
                                                     .arg(unsupportedCount)
                                                     .arg(unsupportedKeys.join(", ")),
                                                 QString("%1 项设置需要处理：%2。详情见 migration/pcl-report.json。")
                                                     .arg(unsupportedCount)
                                                     .arg(unsupportedKeys.join(", "))) } });
    }

    QJsonObject page{ { "panel", QJsonObject{ { "name_i18n", localized("PCL compatibility", "PCL 兼容设置") } } },
                      { "title_i18n", localized("Converted settings", "已转换设置") },
                      { "controls", controls } };
    page.insert("generatedBy", GENERATOR_ID);
    page.insert("sourceSha256", sourceHash);

    const auto manifestPath = FS::PathCombine(lunaRoot, "manifest.json");
    auto reportPath = FS::PathCombine(lunaRoot, "migration", "pcl-report.json");
    auto pagePath = FS::PathCombine(lunaRoot, "generated", "pcl-compat.json");
    const auto suffix = QString("-") + sourceHash.left(12);
    if (!isGeneratedFile(reportPath))
        reportPath = FS::PathCombine(lunaRoot, "migration", "pcl-report" + suffix + ".json");
    if (!isGeneratedFile(pagePath))
        pagePath = FS::PathCombine(lunaRoot, "generated", "pcl-compat" + suffix + ".json");

    if (!isGeneratedFile(reportPath) || !isGeneratedFile(pagePath)) {
        result.error = "Refusing to overwrite author-written LunaUI compatibility files";
        return result;
    }

    if ((isGeneratedFile(manifestPath) && !writeJson(manifestPath, manifest, result.error)) ||
        !writeJson(reportPath, report, result.error) || !writeJson(pagePath, page, result.error)) {
        return result;
    }

    result.generated = true;
    return result;
}

}  // namespace PCL
