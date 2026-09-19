// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "UserInteraction.h"
#include <QScopedValueRollback>
#include <functional>

// Existing main-thread tasks can request input without depending on a GUI page.
// Nested dispatcher calls restore the outer interaction on return.
inline thread_local UserInteraction* activeUserInteraction = nullptr;
inline thread_local std::function<bool()> activeInteractionCancellation;
class ScopedUserInteraction {
    QScopedValueRollback<UserInteraction*> scope;
    QScopedValueRollback<std::function<bool()>> cancellation;
public:
    explicit ScopedUserInteraction(UserInteraction& interaction, std::function<bool()> cancelled = {})
        : scope(activeUserInteraction, &interaction), cancellation(activeInteractionCancellation, cancelled ? std::move(cancelled) : activeInteractionCancellation) {}
};
inline bool headlessConfirm(const QString& prompt)
{
    if (!activeUserInteraction) return false;
    const auto selected = activeUserInteraction->select(prompt, QJsonArray{ "Cancel", "Continue" });
    return selected && *selected == 1;
}
