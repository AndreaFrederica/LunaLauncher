// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QStringList>

/** Load sandboxed provenance recognizers from the installed resource tree. */
class JSExternalLauncherRecognizerRuntime {
   public:
    static void initialize(const QStringList& directories);
};

