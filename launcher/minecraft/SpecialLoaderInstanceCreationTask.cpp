// SPDX-License-Identifier: GPL-3.0-only

#include "SpecialLoaderInstanceCreationTask.h"

#include <utility>

#include "FileSystem.h"
#include "api/LauncherApiSpecial.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/INISettingsObject.h"

SpecialLoaderInstanceCreationTask::SpecialLoaderInstanceCreationTask(BaseVersion::Ptr minecraftVersion,
                                                                     QString loader,
                                                                     QString loaderVersion,
                                                                     QString commitRef)
    : m_minecraftVersion(std::move(minecraftVersion)),
      m_loader(std::move(loader)),
      m_loaderVersion(std::move(loaderVersion)),
      m_commitRef(std::move(commitRef))
{}

std::unique_ptr<BaseInstance> SpecialLoaderInstanceCreationTask::createInstance()
{
    if (!m_minecraftVersion) {
        setError(tr("Minecraft version is unavailable."));
        return nullptr;
    }
    setStatus(tr("Creating instance with %1").arg(m_loader));
    auto instance = std::make_unique<MinecraftInstance>(m_globalSettings,
                                                        std::make_unique<INISettingsObject>(FS::PathCombine(m_stagingPath, "instance.cfg")),
                                                        m_stagingPath);
    SettingsObject::Lock lock(instance->settings());
    auto profile = instance->getPackProfile();
    profile->buildingFromScratch();
    profile->setComponentVersion("net.minecraft", m_minecraftVersion->descriptor(), true);

    QString error;
    if (!SpecialLoader::install(instance.get(), m_loader, m_minecraftVersion->descriptor(), m_loaderVersion, m_commitRef, &error)) {
        setError(error);
        return nullptr;
    }
    instance->setName(name());
    instance->setIconKey(m_instIcon);
    return instance;
}

