# Launcher API boundary

This document defines the additive API boundary for alternate launcher user interfaces.
The boundary is intended to let a new UI use the launcher without linking against Qt
widgets or calling `MainWindow`, while keeping the existing GUI and upstream code
layout intact during the migration.

## Goals

- Expose every user-visible launcher operation through a stable, versioned command
  contract.
- Keep GUI classes, tasks, and upstream file layout in place while adapters are added
  next to them.
- Reuse the existing task and model implementations instead of duplicating launcher
  behavior in a second UI.
- Support MCP/CLI first and allow another transport later without changing the domain
  adapters.
- Report capabilities so a UI can adapt when an upstream build does not contain an
  optional integration.

The API boundary is a service boundary, not a second widget toolkit. It should own
command validation, operation lifecycle, progress, cancellation, interaction requests,
and serialization. It should not own layout, dialogs, text translation, or selection
widgets.

## Layers

```text
alternate UI
    |
transport adapter (MCP, CLI, future local HTTP or in-process adapter)
    |
Launcher API dispatcher and operation catalog
    |
domain adapters (instances, accounts, resources, worlds, servers, Java, settings,
               exports, integrations, diagnostics)
    |
existing launcher models and tasks
```

The first implementation can continue to use Qt internally. Qt-free embedding is a
separate later goal. A UI that launches `lunalauncher-cli --mcp` already avoids a Qt
Widgets dependency and does not require changes to the existing GUI.
The generic `--cli api OPERATION JSON_OBJECT` form can invoke any catalog entry while
transport-specific aliases are added.

## Command contract

Every operation uses the following logical envelope, regardless of transport:

```json
{
  "operation": "instance.create",
  "parameters": {},
  "requestId": "optional-client-id"
}
```

The result has a stable shape:

```json
{
  "ok": true,
  "apiVersion": 1,
  "data": {},
  "warnings": []
}
```

Failures include a machine-readable code and a human-readable message:

```json
{
  "ok": false,
  "apiVersion": 1,
  "error": {
    "code": "instance.not_found",
    "message": "The instance does not exist.",
    "details": {}
  }
}
```

Long-running operations report `started`, `progress`, `input`, `status`, `succeeded`,
`failed`, and `cancelled` events. User interaction is data, not a Qt dialog: the
adapter may request text, a secret, or a choice and the transport returns the answer.

The catalog must expose the operation name, API version, input schema, destructive
flag, supported instance kinds, and optional capability name. A UI must use the catalog
instead of guessing from the launcher version. The initial facade currently publishes
the legacy operation names and descriptions; schemas and the remaining domain
adapters are added incrementally.

## Operation groups

The catalog is complete only when each GUI action maps to one of these groups. The
existing `OperationService` operations are the first adapters; missing operations are
added without moving the existing implementation files. The table below is the target
catalog, not a claim that every operation is implemented in this change.

| Group | Required operations |
| --- | --- |
| Instances | list, info, create, import, rename, group, copy, delete, restore, shortcut, icon, notes, update, verify, export, open-folder, stop, kill |
| Minecraft components | list versions, select version, list/install/remove/reorder components, install loader, managed-pack actions |
| Resources | list, inspect, search, list versions, install, update, dependency check, enable, disable, remove, metadata, batch actions |
| Worlds and servers | list, inspect, create, rename, copy, delete, import/export, join, server properties, console, EULA, Java, loader, YAML, ops, whitelist, bans |
| Accounts | list, login, refresh, remove, default, reorder, profiles, skin library, skin upload/reset, capes |
| Java | scan, install, remove, select, refresh |
| Settings and appearance | list/get/set/reset, import/export, theme, language, layout, paths, proxy |
| Launcher services | update check, metadata, assets, external-tool probes, Aria2, Terracotta, Yukari, diagnostics |
| Logs and screenshots | list, read, tail/watch, clear, delete, upload, copy/open |

An operation may return `unsupported` when an optional provider is not built or
installed. That is different from an unknown operation and lets the UI display the same
capability state as the Qt GUI.

## Upstream-friendly migration

The migration deliberately avoids a large refactor:

1. Add API headers and adapters in a new `launcher/api/` directory.
2. Register the new files in the build source lists with small append-only changes.
3. Put one operation family in one adapter file. Do not move existing GUI or task files.
4. Make CLI and MCP call the dispatcher. Keep their existing wire formats and command
   names as compatibility aliases.
5. Migrate GUI actions one family at a time. A GUI slot may call the same adapter while
   its existing dialog remains responsible for presentation.
6. Add contract tests for the adapter and keep upstream task tests unchanged.

When upstream changes a task or page, the adapter is updated in its separate file. A
merge therefore adds a small adapter conflict only when the affected behavior changed;
the upstream GUI implementation remains easy to merge.

The facade currently waits for existing tasks and forwards status/input events through
`UserInteraction`, preserving CLI and MCP behavior. Before exposing a new long-running
GUI domain, add an API task bridge and task registry around the existing `Task` signals
so a future transport can track a request without blocking its event loop.

## Current baseline

The current headless interface covers basic instance/account/resource/settings
operations, imports, launches, instance creation and lifecycle control, shortcuts,
icons, notes, asset verification, Minecraft component editing, and zip export. It
also covers world listing and editing, account ordering/profile and local skin library
operations, skin upload/reset and cape selection, resource inspection/refresh, Java
scanning/installation/removal/selection, server properties and EULA files, server
operator/whitelist/ban lists, server console commands, bounded log reads, and
screenshot listing/deletion. Optional domains such as platform search, resource
dependency resolution and batch actions, server YAML/loader pages, and
Aria2/Terracotta/Yukari controls still need dedicated adapters. They should be added
beside the existing files and registered in the catalog so a client can distinguish
an unsupported optional provider from an unknown operation.
See [CLI-MCP.md](CLI-MCP.md) for the compatibility command list and limitations.
