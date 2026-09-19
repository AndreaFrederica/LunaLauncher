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
separate later goal. A UI that launches `lunalauncher-cli --mcp` needs no Qt linkage of its own.
The sidecar still links Qt, including Widgets, and must ship with its runtime libraries.
The generic `--cli api OPERATION JSON_OBJECT` form can invoke any catalog entry while
transport-specific aliases are added.

## Command contract

The CLI uses `--cli --json api OPERATION JSON_OBJECT`. Persistent clients use
`launcher/execute` on the existing `--mcp` JSON-RPC transport:

```json
{"jsonrpc":"2.0","id":1,"method":"launcher/execute","params":{"operation":"instance.list","parameters":{}}}
```

The JSON-RPC `result` (or CLI output) is the API envelope:

```json
{"ok":true,"apiVersion":1,"operation":"instance.list","exitCode":0,"data":[]}
```

Failures preserve the existing string error format:

```json
{"ok":false,"apiVersion":1,"operation":"instance.info","exitCode":2,"error":"Instance not found."}
```

Do not parse translated error text for control flow. `exitCode` distinguishes the
existing broad error categories; domain-specific machine-readable error codes remain
future work. JSON-RPC protocol errors are separate, including `-32000` for a busy
backend and `-32602` for an invalid interaction reply.

`api.describe` and `launcher/catalog` expose the operation name, API version, input
schema, destructive flag, and optional capability group. A capability group is not
proof that a helper is installed: query `integration.status` for runtime availability.
The dispatcher checks declared required fields, scalar/array types, and numeric bounds;
handlers validate operation-specific constraints. It does not implement all JSON Schema
keywords or reject every extra property. Use the catalog rather than guessing from the
launcher version. MCP tools are generated from the same catalog.

Native transport notifications use `launcher/event`, correlated by `requestId`:
`status`, `device_code`, `task`, and `input`. Final completion is the response to
`launcher/execute`. Input replies use `launcher/respond`. Task snapshots contain
numeric progress, cancellation support, warnings and steps. `task.list`, `task.status`,
`task.cancel`, catalog calls, and interaction replies remain available during a long
operation; other operations are serialized. History retains up to 64 task records
without evicting active tasks. See [NEO-UI-API.md](NEO-UI-API.md) for the wire protocol,
client example, packaging requirements and lifecycle details.

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

## Current coverage and remaining gaps

The implementation exposes 120 operations and reuses existing models and tasks in additive domain adapters:

| Domain | Implemented boundary | Remaining work |
| --- | --- | --- |
| Instances | list/info/create/import/copy/delete/restore, rename/group/icon/notes, launch/stop/kill, verify/update, shortcuts/folders, ZIP export | specialized managed-pack workflows, other export formats; server force-kill semantics still use the existing stop implementation |
| Components | component catalog and version lists, installed list, version selection, enable/remove/reorder/customize/revert; install local component files, jar mods, replacement jars and Java agents | richer drag/drop validation and provider-specific component installers |
| Resources | installed inspection/refresh, local/URL install, enable/disable/remove; remote search/project/version/dependency lookup, indexed installation; recursive required/optional dependency installation with bounds; batch upgrade check/select/apply with stale-file validation and disabled-state preservation | Hangar update metadata; restricted CurseForge files with no direct URL are not exposed by the common version list |
| Servers | properties, EULA, operator/whitelist/ban lists, start, terminal subscription/input/resize, YAML revisions, compatibility configuration; local mods/plugins CRUD; staged HTTP(S) server distribution installation | provider-specific automatic distribution catalogs and installer arguments beyond the supplied URL |
| Worlds | list, rename/delete, reset icon, import directory/zip and export world zip | world creation from an empty template and advanced copy/replace policies |
| Accounts | login/refresh/remove/default/order/profiles; skin library/upload/reset/cape | live profile changes outside current selection flow |
| Java | scan/list/install/remove/select | platform-specific diagnostics beyond current core tasks |
| Settings | every registered launcher/instance setting via list/get/set/reset/import/export; theme/icon/background catalogs, refresh and selection; backend language catalog/selection/refresh | frontend rendering and localization; some services require a sidecar restart after settings changes |
| Integrations | Aria2/Terracotta/Yukari status/install/start/stop; room host/join/leave/state/log and Yukari retry; Aria2 queue/cancel/clear/remove | broader launcher update and proxy diagnostics; not every helper version/platform has been exercised |
| Logs/screenshots | bounded reads, file lists, clear/delete/upload for logs, Imgur screenshot upload/deletion; client console and server PTY subscriptions, log file following with bounded buffers and cursor polling | copy/open actions remain presentation-owned |

`resource.update` refreshes installed resource metadata; it is **not** a remote version
upgrade. To upgrade a known project, call `resource.versions`, inspect dependencies, and
use `resource.install-version` with the selected version. Its result explicitly reports
`dependenciesInstalled: false`. It uses the existing indexed downloader and checksum
validation; an existing same-name file requires `replace: true`. The existing downloader
updates index metadata before the file download finishes, so interrupted replacements
may require a metadata refresh. `resource.install` remains the local/direct-URL path.

For batch upgrades, `resource.updates.check` creates an immutable session plan from up
to 256 indexed Modrinth/CurseForge resources. It defaults to releases and the instance's
Minecraft and applicable loader filters; explicit filters and releaseTypes override
these defaults. Only strictly newer dated versions are selected. Missing current
versions, unindexed files and unsupported metadata are reported as skipped. Plans expire
after ten minutes (maximum 16 active plans); `resource.updates.discard` releases one.

`resource.updates.apply` requires planId and an items array of selected item IDs. It
preflights SHA-256 fingerprints of all selected files and indexes, rejects filename
collisions, and consumes the plan once execution begins. Each download uses the existing
task in a temporary directory, then rechecks the originals before replacing the file and
atomically saving the new index. Disabled resources remain disabled. A failed replacement
attempts restoration; if restoration fails, recoveryDirectory contains original.backup.
This does not guarantee crash/power-loss recovery or batch rollback. Inspect complete
and each updated/failed/cancelled result even when the operation returns ok=true.
Cancellation retains completed items and skips the remainder. Dependencies remain a
separate selection and installation step.

`instance.console.subscribe` and `instance.log.subscribe` create session subscriptions.
`launcher/stream` notifications and `event.poll` have independent cursors into bounded
history; event.subscriptions/unsubscribe manage their lifetime. Client console lines use
the existing censored log model; server PTY output and file chunks are base64 bytes.
See [Neo UI integration](NEO-UI-API.md) for event shapes, buffer limits, rotation behavior,
terminal controls and frontend responsibilities for appearance/language APIs.

Provider search uses the existing provider filters and page size (25). `mayHaveMore` is
a page-size heuristic, not a provider total. Hangar does not filter Minecraft versions
in project search; its version list is filtered after retrieval. The provider catalog
reports these differences. Dependency lookup resolves one dependency, not a complete
graph. Provider timeouts, credentials, mirror settings and version-list limits still
follow the existing core implementations.

`server.yaml.write` edits only `bukkit.yml` and `spigot.yml`, preserves raw UTF-8 text,
limits content to 1 MiB, and uses an atomic save. An optional `ifRevision` prevents
stale writes. YAML syntax is not parsed, matching the current GUI editor. Server loader
configuration changes compatibility filters and does not install server software.

`component.catalog` and `component.versions` accept `offline: true` to read existing
metadata files without fetching missing entries. New instance metadata loads are tracked
so the same cancellation mechanism covers the version-loading phase.

## Validation

Build and install with the repository's Meson/Pixi workflow (including helper modules
and runtime deployment). Use a separate install prefix for tests. Then run:

```text
python tests/api_sidecar_smoke.py --exe install-api-test/lunalauncher-cli.exe
python tests/api_sidecar_smoke.py --exe install-api-test/lunalauncher-cli.exe --online
node --experimental-strip-types --test tests/launcher_api_client.test.mjs
```

The sidecar test uses a temporary profile and a local HTTP fixture. It covers fragmented
UTF-8 and combined pipe frames, malformed/oversized requests, input replies/cancellation,
busy requests, task control during a pending download, MCP compatibility, offline
metadata, YAML revision checks, loader configuration, log boundaries and integration
status, theme/language validation, server mods/plugins CRUD, disposable server PTY
roundtrips/restarts, log streams during downloads, rotation and history overflow. The
optional online test installs an older Modrinth file into a disposable instance and
upgrades it while disabled, checks hashes and rejects stale files/indexes and destination
collisions. It does not launch Minecraft or authenticate
real accounts. Live multiplayer rooms, Microsoft account mutations and every external
provider are not validated by these tests.
