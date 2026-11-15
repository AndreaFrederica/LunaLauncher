// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>
#include <QList>

// Deterministic local rules. A match is evidence for a diagnosis, never an
// instruction to modify the instance. Keep unknown failures explicitly unknown.
namespace LaunchDiagnostics {
struct Source { QString name; QString text; };

inline QJsonArray analyze(const QList<Source>& sources, bool english)
{
    struct Rule {
        const char *id, *pattern, *confidence;
        const char *titleZh, *titleEn, *adviceZh, *adviceEn;
    };
    static const Rule rules[] = {
        { "legacy-java", R"((?:ClassCastException:[^\n]*AppClassLoader[^\n]*URLClassLoader))", "high",
          "旧版启动组件与当前 Java 不兼容", "Legacy launcher is incompatible with this Java",
          "旧版 LaunchWrapper 依赖 Java 8 的类加载器。对于 Minecraft 1.12.2 / 旧版 Forge，在实例的 Java 设置中选择 64 位 Java 8，然后重试并确认控制台中的 Java 版本。不要修改其他实例的全局 Java。",
          "Legacy LaunchWrapper expects Java 8 class loaders. For Minecraft 1.12.2 / legacy Forge, select 64-bit Java 8 in this instance's Java settings, retry and verify the Java version in the console. Keep other instances' Java settings separate." },
        { "java-too-old", R"((?:UnsupportedClassVersionError[^\n]*|class file version [\d.]+[^\n]*only recognizes[^\n]*|Unsupported major\.minor version[^\n]*))", "high",
          "Java 版本低于组件要求", "Java is older than a component requires",
          "查看错误中的 class file version：52 对应 Java 8，60 对应 Java 16，61 对应 Java 17，65 对应 Java 21。为此实例选择同时满足游戏和加载器要求的 Java；若只有某个模组要求更高版本，应先核对该模组是否适用于此游戏版本。",
          "Check the class file version: 52 = Java 8, 60 = Java 16, 61 = Java 17, 65 = Java 21. Choose a Java runtime compatible with both the game and loader. If only one mod needs newer Java, first verify that mod's game version." },
        { "java-heap", R"((?:OutOfMemoryError:\s*(?:Java heap space|GC overhead limit exceeded)))", "high",
          "Java 堆内存不足", "Java heap memory exhausted",
          "关闭其他占用内存的程序，在实例设置中适当提高最大内存并给系统留出余量。若仍发生，检查近期新增模组或资源包；记录复现步骤以排查内存泄漏。",
          "Close memory-heavy applications and moderately increase this instance's maximum memory, leaving RAM for the OS. If it recurs, check recent mods/resource packs and record reproduction steps for a possible memory leak." },
        { "native-memory", R"((?:Could not reserve enough space[^\n]*|Native memory allocation[^\n]*failed[^\n]*|OutOfMemoryError:\s*unable to create native thread[^\n]*|There is insufficient memory for the Java Runtime Environment[^\n]*))", "high",
          "系统可用内存或虚拟内存不足", "Insufficient system or virtual memory",
          "检查是否使用 64 位 Java，关闭占用内存的程序，降低实例最大内存并检查系统分页文件是否可用。此类错误不应继续盲目增加最大内存。",
          "Verify 64-bit Java, close memory-heavy applications, reduce the instance's maximum heap and check the OS paging file. Increasing the heap further can worsen this failure." },
        { "invalid-jvm-option", R"((?:Unrecognized (?:VM )?option[^\n]*|Improperly specified VM option[^\n]*|Invalid maximum heap size[^\n]*))", "high",
          "Java 参数无效或不兼容", "Invalid or incompatible Java options",
          "根据证据定位具体参数，先备份自定义 JVM 参数，再移除该参数或恢复此实例的默认参数；确认参数支持当前 Java 版本。",
          "Locate the named option, back up custom JVM arguments, then remove that option or restore this instance's defaults. Verify the option is supported by the selected Java version." },
        { "graphics", R"((?:GLFW error (?:65542|65543)[^\n]*|Pixel format not accelerated[^\n]*|Failed to create (?:an? )?OpenGL[^\n]*|The driver does not appear to support OpenGL[^\n]*))", "high",
          "OpenGL 初始化失败", "OpenGL initialization failed",
          "从显卡厂商安装适用驱动，检查游戏使用的 GPU 是否支持此版本所需 OpenGL；远程桌面或虚拟机也可能影响图形能力。先关闭近期新增的着色器再重试。",
          "Install the appropriate GPU vendor driver and verify the selected GPU supports this game's OpenGL requirements. Remote desktop or virtual machines can limit graphics support. Disable recently added shaders and retry." },
        { "corrupt-archive", R"((?:ZipException:\s*(?:zip END header not found|error in opening zip file|invalid[^\n]*)|Invalid or corrupt jarfile[^\n]*))", "high",
          "JAR / ZIP 文件损坏或格式错误", "Invalid or damaged JAR / ZIP file",
          "根据报错路径定位文件。对游戏依赖运行文件校验或修复；对模组重新下载对应版本。先备份并仅替换报错文件，不要删除整个实例或存档。",
          "Find the file named in the error. Verify/repair game libraries or redownload the correct mod version. Back up and replace only the affected file; keep worlds and the rest of the instance." },
        { "duplicate-mods", R"((?:DuplicateModsFoundException[^\n]*|Duplicate mods found[^\n]*|Found duplicate mods[^\n]*))", "high",
          "检测到重复模组", "Duplicate mods detected",
          "查看错误中列出的模组 ID 和文件，将重复版本移到 mods 目录之外保留备份，只保留与当前游戏及加载器兼容的一份。",
          "Check the listed mod IDs and files. Move duplicate versions outside the mods directory as a backup, keeping one version compatible with the game and loader." },
        { "mod-dependencies", R"((?:MissingModsException[^\n]*|ModResolutionException[^\n]*|Incompatible mod set[^\n]*|Incompatible mods found[^\n]*|Missing (?:mandatory|required) dependencies[^\n]*|requires[^\n]+which is missing[^\n]*))", "high",
          "模组依赖缺失或版本不兼容", "Missing or incompatible mod dependencies",
          "按日志列出的模组、依赖及版本范围补齐依赖或更换兼容版本；同时核对 Minecraft 和 Forge / Fabric / NeoForge 类型，不要混用不同加载器的模组。",
          "Use the listed mod names and version ranges to install missing dependencies or compatible versions. Verify the Minecraft version and loader family (Forge/Fabric/NeoForge); do not mix incompatible mod builds." },
        { "mixin", R"((?:MixinApplyError[^\n]*|MixinTransformerError[^\n]*|InvalidMixinException[^\n]*|Mixin apply[^\n]*failed[^\n]*))", "possible",
          "模组注入失败，可能存在版本或兼容性问题", "Mod injection failed; possible compatibility issue",
          "查看 Mixin 配置所属模组及最内层 Caused by。优先核对游戏、加载器、模组版本；备份后暂时移出最近新增或更新的相关模组逐个验证。仅凭 Mixin 错误不能断定具体冲突对象。",
          "Inspect the mod owning the Mixin config and the innermost Caused by. Check game/loader/mod versions, then temporarily move recently changed related mods aside and test one at a time. A Mixin error alone does not identify the conflicting mod." },
        { "missing-class", R"((?:ClassNotFoundException:\s*[^\n]+|NoClassDefFoundError:\s*[^\n]+|Could not find or load main class[^\n]*))", "possible",
          "所需类无法加载", "A required class could not be loaded",
          "核对报错类属于游戏依赖、加载器还是模组；先运行文件校验并检查缺失依赖或版本不匹配。NoClassDefFoundError 也可能由更早的初始化异常引起，需要查看前面的 Caused by。",
          "Identify whether the class belongs to a game library, loader or mod. Verify files and check missing or incompatible dependencies. NoClassDefFoundError can also follow an earlier initialization failure; inspect preceding Caused by entries." },
        { "native-library", R"((?:UnsatisfiedLinkError:\s*[^\n]+))", "possible",
          "本地库加载失败", "Native library could not be loaded",
          "核对 Java 与本地库的位数和系统架构，运行依赖文件校验，并检查杀毒软件是否隔离了相关文件。按报错定位具体库，不要直接关闭系统安全防护。",
          "Check Java/native-library architecture, verify dependency files and inspect antivirus quarantine for the named library. Identify the affected file rather than disabling system security." },
        { "disk-full", R"((?:No space left on device|There is not enough space on the disk|磁盘空间不足))", "high",
          "磁盘空间不足", "Disk is full",
          "检查游戏目录、启动器缓存和系统临时目录所在磁盘的剩余空间，腾出空间后重试；不要删除存档来释放空间。",
          "Check free space on drives containing the game, launcher cache and system temporary directory. Free space and retry, preserving worlds." },
        { "access-denied", R"((?:AccessDeniedException:\s*[^\n]+|Permission denied[^\n]*|Access is denied[^\n]*))", "possible",
          "文件访问被拒绝", "File access denied",
          "检查报错路径的写入权限、文件是否被其他进程占用及安全软件隔离记录。优先使用当前用户可写目录，不要直接把管理员权限作为默认修复。",
          "Check permissions on the named path, file locks and security software quarantine. Prefer a directory writable by the current user rather than running as administrator by default." },
    };
    QJsonArray findings;
    for (const auto& rule : rules) {
        QJsonArray evidence;
        const QRegularExpression pattern(QString::fromUtf8(rule.pattern), QRegularExpression::CaseInsensitiveOption);
        for (const auto& source : sources) {
            const auto match = pattern.match(source.text);
            if (match.hasMatch()) evidence.append(QJsonObject{ { "source", source.name }, { "text", match.captured().left(800) } });
            if (evidence.size() >= 3) break;
        }
        if (evidence.isEmpty()) continue;
        findings.append(QJsonObject{ { "id", rule.id }, { "confidence", rule.confidence },
            { "title", QString::fromUtf8(english ? rule.titleEn : rule.titleZh) },
            { "advice", QString::fromUtf8(english ? rule.adviceEn : rule.adviceZh) }, { "evidence", evidence } });
    }
    return findings;
}
} // namespace LaunchDiagnostics
