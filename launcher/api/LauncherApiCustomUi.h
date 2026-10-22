// SPDX-License-Identifier: GPL-3.0-only

#pragma once

class LauncherApi;

/** Expose the per-instance Custom UI JSON/JS panel runtime through the headless API. */
void registerLauncherApiCustomUiOperations(LauncherApi& api);
