# XMCL compatibility object map

Neo UI uses object-shaped API snapshots so a frontend can keep a launcher
context and an instance context, while the existing operation names remain
available for mutations and long-running work.

## Core objects

| XMCL service boundary | Neo API object/operations | State model |
| --- | --- | --- |
| `BaseService` | `launcher.get`, `runtime.info`, `network.proxy.*` | launcher profile, paths, capabilities |
| `InstanceService` | `instance.list`, `instance.get`, `instance.create`, `instance.delete`, `instance.update` | instance identity, paths, runtime, lifecycle |
| `LaunchService` | `instance.launch`, `instance.stop`, `instance.kill`, `instance.console.subscribe` | process lifecycle and console stream |
| `UserService` / account services | `account.list`, `account.login`, `account.refresh`, `account.subscribe` | account profiles and auth state |
| `JavaService` | `java.list`, `java.refresh`, `java.install`, `java.select` | detected and managed Java installations |
| `VersionService` | `component.catalog`, `component.versions`, `instance.version.*` | local and remote Minecraft/loader versions |
| `InstanceResourcesService` | `resource.list`, `resource.install`, `resource.enable`, `resource.disable`, `resource.remove`, `resource.updates.*` | mods, packs, shaders and datapacks |
| `InstanceLogService` | `instance.log.subscribe`, `instance.log.list`, `instance.log.read` | launch history and log files |
| task services | `task.list`, `task.status`, `task.cancel` | progress, cancellation and completion |
| update/appearance services | `launcher.update.*`, `appearance.*` | update state and UI appearance |

## Compatibility rules

1. `instance.get` and `launcher.get` are read-only aggregate objects. They do
   not replace fine-grained operations.
2. Existing flat response fields remain supported for CLI/MCP clients.
3. Sensitive launcher settings are redacted unless a caller explicitly uses
   the existing reveal/import controls.
4. Long-running operations return task identities and stream progress through
   the existing event protocol.
5. New UI code should depend on these object contracts and call operations
   through an adapter. It should not depend on Qt classes or instance paths as
   object identity; `instance.id` is the stable identity.

## Remaining adapter work

The XMCL renderer also expects shared reactive stores, file/drag-drop bridges,
native dialogs, and detailed resource/save/screenshot services. Those should be
implemented as adapter methods over the existing operation groups before
copying XMCL views wholesale. The backend operation surface already contains
most of the required domains; the adapter is the compatibility boundary.
