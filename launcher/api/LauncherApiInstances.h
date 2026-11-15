// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <QJsonArray>
#include <QJsonObject>

class LauncherApi;
class InstanceTask;
class UserInteraction;

/** Register instance lifecycle and shell integration operations. */
void registerInstanceApiOperations(LauncherApi& api);

/** Install a generated/imported instance into a selected Store.
 *
 * The helper is shared with the modpack API so every installation path applies
 * the same Store format rules and target selection policy.
 */
QJsonObject installInstanceTaskToStore(LauncherApi& api,
                                       InstanceTask* task,
                                       const QJsonObject& parameters,
                                       UserInteraction& interaction,
                                       QJsonArray warnings = {});
