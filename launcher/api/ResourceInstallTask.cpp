// SPDX-License-Identifier: GPL-3.0-only

#include "ResourceInstallTask.h"

#include <QDir>
#include <QFileInfo>
#include <QVector>

#include <optional>

#include "Application.h"
#include "ResourceDownloadTask.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "minecraft/mod/DataPackFolderModel.h"
#include "modplatform/ResourceType.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/modrinth/ModrinthAPI.h"
#include "tasks/SequentialTask.h"

namespace {

ModPlatform::ModLoaderTypes loaderFlags(const QStringList& loaders)
{
    ModPlatform::ModLoaderTypes flags;
    for (const auto& raw : loaders) {
        const auto loader = raw.toLower();
        if (loader == "forge") flags |= ModPlatform::Forge;
        else if (loader == "neoforge") flags |= ModPlatform::NeoForge;
        else if (loader == "fabric") flags |= ModPlatform::Fabric;
        else if (loader == "quilt") flags |= ModPlatform::Quilt;
        else if (loader == "cleanroom") flags |= ModPlatform::Cleanroom;
        else if (loader == "liteloader") flags |= ModPlatform::LiteLoader;
    }
    return flags;
}

}  // namespace

ResourceInstallTask::ResourceInstallTask(BaseInstance* instance,
                                         QString kind,
                                         QString provider,
                                         QString projectId,
                                         QString versionId,
                                         bool includeOptional,
                                         QString minecraftVersion,
                                         QStringList loaders)
    : m_instance(instance),
      m_minecraft(dynamic_cast<MinecraftInstance*>(instance)),
      m_kind(std::move(kind)),
      m_provider(std::move(provider).toLower()),
      m_projectId(std::move(projectId)),
      m_versionId(std::move(versionId)),
      m_minecraftVersion(std::move(minecraftVersion)),
      m_loaders(std::move(loaders)),
      m_includeOptional(includeOptional)
{
    setStatus(tr("正在准备资源安装"));
}

ModPlatform::IndexedPack::Ptr ResourceInstallTask::makePack(const QVariant& addonId, const QString& name) const
{
    auto pack = std::make_shared<ModPlatform::IndexedPack>();
    pack->addonId = addonId;
    pack->name = name;
    pack->provider = m_provider == "curseforge" ? ModPlatform::ResourceProvider::FLAME : ModPlatform::ResourceProvider::MODRINTH;
    return pack;
}

ModPlatform::ResourceType ResourceInstallTask::resourceType() const
{
    if (m_kind == "mods" || m_kind == "coremods" || m_kind == "nilmods") return ModPlatform::ResourceType::Mod;
    if (m_kind == "resourcepacks" || m_kind == "texturepacks") return ModPlatform::ResourceType::ResourcePack;
    if (m_kind == "shaderpacks") return ModPlatform::ResourceType::ShaderPack;
    if (m_kind == "datapacks") return ModPlatform::ResourceType::DataPack;
    if (m_kind == "plugins") return ModPlatform::ResourceType::Plugin;
    return ModPlatform::ResourceType::Unknown;
}

ModPlatform::ModLoaderTypes ResourceInstallTask::loaderFlags() const
{
    return ::loaderFlags(m_loaders);
}

void ResourceInstallTask::executeTask()
{
    if (!m_minecraft) {
        fail(tr("Resource installation requires a Minecraft instance."));
        return;
    }
    if (m_kind == "mods") m_model = m_minecraft->loaderModList().get();
    else if (m_kind == "resourcepacks") m_model = m_minecraft->resourcePackList().get();
    else if (m_kind == "shaderpacks") m_model = m_minecraft->shaderPackList().get();
    else if (m_kind == "datapacks") m_model = m_minecraft->dataPackList().get();
    else {
        fail(tr("This resource kind cannot be installed with dependency resolution."));
        return;
    }
    if (!m_model) {
        fail(tr("The resource folder is not available."));
        return;
    }
    if (m_provider != "modrinth" && m_provider != "curseforge") {
        fail(tr("Dependency installation currently supports Modrinth and CurseForge."));
        return;
    }
    if (m_minecraftVersion.isEmpty()) {
        m_minecraftVersion = m_minecraft->getPackProfile()->getComponentVersion("net.minecraft");
    }
    if (m_minecraftVersion.isEmpty()) {
        fail(tr("The Minecraft version is required for dependency resolution."));
        return;
    }
    m_api = m_provider == "curseforge" ? std::unique_ptr<ResourceAPI>(new FlameAPI()) : std::unique_ptr<ResourceAPI>(new ModrinthAPI());
    m_rootPack = makePack(m_provider == "curseforge" ? QVariant(m_projectId.toLongLong()) : QVariant(m_projectId), m_projectId);
    fetchProject();
}

void ResourceInstallTask::fetchProject()
{
    auto* raw = m_api.get();
    auto task = raw->getProjectInfo({ m_rootPack }, {
        [this](ModPlatform::IndexedPack::Ptr& pack) {
            if (m_aborted) return;
            m_rootPack = pack;
            fetchVersions();
        },
        [this](const QString& reason, int) { fail(reason); },
        [this] { fail(tr("资源信息请求已取消。")); },
    });
    m_current = task;
    if (!task) {
        fail(tr("The provider did not create a project request."));
        return;
    }
    task->start();
}

void ResourceInstallTask::fetchVersions()
{
    if (m_aborted) return;
    setStatus(tr("正在解析资源版本"));
    ResourceAPI::VersionSearchArgs args;
    args.pack = m_rootPack;
    args.resourceType = resourceType();
    args.includeRestricted = true;
    if (!m_minecraftVersion.isEmpty()) args.mcVersions = std::list<Version>{ Version(m_minecraftVersion) };
    if (loaderFlags()) args.loaders = loaderFlags();
    auto task = m_api->getProjectVersions(std::move(args), {
        [this](QVector<ModPlatform::IndexedVersion>& versions) {
            if (m_aborted) return;
            std::optional<ModPlatform::IndexedVersion> selected;
            for (const auto& version : versions) {
                if (version.fileId.toString() == m_versionId) { selected = version; break; }
            }
            if (!selected) {
                fail(tr("The selected resource version was not found."));
                return;
            }
            m_rootVersion = *selected;
            if (m_rootVersion.downloadUrl.isEmpty()) {
                fail(tr("The provider did not supply a downloadable resource file."));
                return;
            }
            m_visited.insert(m_rootPack->addonId.toString());
            m_install.append({ m_rootPack, m_rootVersion });
            for (const auto& dependency : m_rootVersion.dependencies) {
                if (dependency.type == ModPlatform::DependencyType::Required ||
                    (m_includeOptional && dependency.type == ModPlatform::DependencyType::Optional)) {
                    m_dependencies.enqueue(dependency);
                }
            }
            resolveNextDependency();
        },
        [this](const QString& reason, int) { fail(reason); },
        [this] { fail(tr("版本请求已取消。")); },
    });
    m_current = task;
    if (!task) {
        fail(tr("The provider did not create a version request."));
        return;
    }
    task->start();
}

void ResourceInstallTask::resolveNextDependency()
{
    if (m_aborted) return;
    while (!m_dependencies.isEmpty()) {
        const auto dependency = m_dependencies.dequeue();
        const auto key = dependency.addonId.toString();
        if (key.isEmpty() || m_visited.contains(key)) continue;
        m_visited.insert(key);
        setStatus(tr("正在解析依赖 %1").arg(key));
        ResourceAPI::DependencySearchArgs args;
        args.dependency = dependency;
        args.mcVersion = Version(m_minecraftVersion);
        args.loader = loaderFlags();
        auto task = m_api->getDependencyVersion(std::move(args), {
            [this, addonId = dependency.addonId, type = dependency.type](ModPlatform::IndexedVersion& version) {
                if (m_aborted) return;
                if (version.fileId.toString().isEmpty() || version.downloadUrl.isEmpty()) {
                    fail(tr("A required dependency has no downloadable file: %1").arg(addonId.toString()));
                    return;
                }
                auto pack = makePack(addonId, addonId.toString());
                m_install.append({ pack, version });
                for (const auto& child : version.dependencies) {
                    if (child.type == ModPlatform::DependencyType::Required ||
                        (m_includeOptional && child.type == ModPlatform::DependencyType::Optional)) {
                        m_dependencies.enqueue(child);
                    }
                }
                Q_UNUSED(type);
                resolveNextDependency();
            },
            [this](const QString& reason, int) { fail(reason); },
            [this] { fail(tr("依赖请求已取消。")); },
        });
        m_current = task;
        if (!task) {
            fail(tr("The provider did not create a dependency request."));
            return;
        }
        task->start();
        return;
    }
    installVersions();
}

void ResourceInstallTask::installVersions()
{
    if (m_aborted) return;
    if (m_install.isEmpty()) {
        fail(tr("No installable resources were resolved."));
        return;
    }
    setStatus(tr("正在下载 %1 个资源").arg(m_install.size()));
    auto sequence = makeShared<SequentialTask>(tr("安装资源及依赖"));
    for (const auto& [pack, version] : m_install) {
        sequence->addTask(makeShared<ResourceDownloadTask>(pack, version, m_model));
    }
    m_current = sequence;
    connect(sequence.get(), &Task::status, this, &Task::setStatus);
    connect(sequence.get(), &Task::progress, this, &Task::setProgress);
    connect(sequence.get(), &Task::finished, this, [this, sequence]() {
        if (m_aborted) return;
        if (!sequence->wasSuccessful()) {
            emitFailed(sequence->failReason().isEmpty() ? tr("资源安装失败。") : sequence->failReason());
            return;
        }
        setStatus(tr("资源安装完成"));
        emitSucceeded();
    });
    sequence->start();
}

void ResourceInstallTask::fail(const QString& message)
{
    if (m_aborted) return;
    emitFailed(message);
}

bool ResourceInstallTask::abort()
{
    m_aborted = true;
    if (m_current) m_current->abort();
    emitAborted();
    return true;
}
