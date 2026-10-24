// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class MinecraftInstance;
class LauncherApi;

/** Register Legacy Fabric and LabyMod provider/install operations. */
void registerLauncherApiSpecialOperations(LauncherApi& api);

namespace SpecialLoader {

/** Fetch the official version choices used by the native C++ UI. */
QJsonArray versions(const QString& loader, const QString& minecraftVersion, QString* error = nullptr);

/** Build the normalized Prism component profile from the official provider. */
QJsonObject profile(const QString& loader,
                    const QString& minecraftVersion,
                    const QString& loaderVersion = {},
                    const QString& commitRef = {},
                    QString* error = nullptr);

/** Install the normalized profile and download its libraries into an instance. */
bool install(MinecraftInstance* instance,
             const QString& loader,
             const QString& minecraftVersion,
             const QString& loaderVersion = {},
             const QString& commitRef = {},
             QString* error = nullptr);

}  // namespace SpecialLoader
