// SPDX-License-Identifier: GPL-3.0-only

#pragma once

class LauncherApi;

/** New asynchronous Neo UI task API, independent of OperationService. */
void registerLauncherApiNeoOperations(LauncherApi& api);
