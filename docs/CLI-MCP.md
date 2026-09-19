# Headless CLI, TUI, and MCP

The headless entry points initialize the normal launcher data, task, account, import, and launch services without showing a window. They do not create a daemon. The process exits after a CLI command, when the TUI is closed, after a detached game starts, or when MCP standard input closes.

On Windows, use `lunalauncher-cli.exe` for all three headless modes. It is built as a console application so PowerShell and Command Prompt wait for it and keep terminal input attached. `lunalauncher.exe` remains a GUI application so normal launcher use does not open a console window. On other platforms, use `lunalauncher`.

## CLI

```text
lunalauncher-cli --cli instance list [--json]
lunalauncher-cli --cli account list [--json]
lunalauncher-cli --cli account login microsoft [--minecraft-profile-name NAME]
lunalauncher-cli --cli account login offline --username NAME
lunalauncher-cli --cli account login yggdrasil --username USER --auth-url URL --session-url URL [--password-stdin]
lunalauncher-cli --cli account login unified-pass --username USER --server-id ID [--password-stdin]
lunalauncher-cli --cli account default ACCOUNT|-
lunalauncher-cli --cli account refresh ACCOUNT
lunalauncher-cli --cli account remove ACCOUNT --yes
lunalauncher-cli --cli instance info INSTANCE
lunalauncher-cli --cli instance rename INSTANCE NAME
lunalauncher-cli --cli instance group INSTANCE [GROUP]
lunalauncher-cli --cli instance copy INSTANCE NAME
lunalauncher-cli --cli instance update INSTANCE
lunalauncher-cli --cli instance delete INSTANCE --yes [--permanent] [--force]
lunalauncher-cli --cli instance undo-delete
lunalauncher-cli --cli import PATH_OR_URL [--name NAME]
lunalauncher-cli --cli launch INSTANCE [--profile NAME | --offline NAME] [--server ADDRESS | --world WORLD] [--wait]
lunalauncher-cli --cli resource list INSTANCE KIND
lunalauncher-cli --cli resource install INSTANCE KIND PATH_OR_DIRECT_URL
lunalauncher-cli --cli resource enable INSTANCE KIND RESOURCE
lunalauncher-cli --cli resource disable INSTANCE KIND RESOURCE
lunalauncher-cli --cli resource remove INSTANCE KIND RESOURCE --yes
lunalauncher-cli --cli java list
lunalauncher-cli --cli settings list launcher [FILTER]
lunalauncher-cli --cli settings get launcher KEY [--reveal-secrets]
lunalauncher-cli --cli settings set launcher KEY [VALUE]
lunalauncher-cli --cli settings reset launcher KEY
lunalauncher-cli --cli settings list instance INSTANCE [FILTER]
lunalauncher-cli --cli settings get instance INSTANCE KEY [--reveal-secrets]
lunalauncher-cli --cli settings set instance INSTANCE KEY [VALUE]
lunalauncher-cli --cli settings reset instance INSTANCE KEY
lunalauncher-cli --cli api OPERATION [JSON_OBJECT]
```

Authentication passwords and sensitive setting values are never accepted as command-line arguments. Use hidden terminal input or `--password-stdin`. `--non-interactive` makes missing input an error. Launches detach after the game starts unless `--wait` is present. Ctrl-C aborts the active task.

Normal archive URLs and local packs are handled by `InstanceImportTask`. Modrinth and CurseForge project file pages are resolved through their APIs. Restricted CurseForge files require an enabled external tool that reports `headless: true` during its protocol probe. Resource kinds are `mods`, `coremods`, `nilmods`, `resourcepacks`, `texturepacks`, `shaderpacks`, `datapacks`, `schematics`, `customplayermodels`, and `yesstevemodels`.

Settings commands expose every setting registered by the launcher or selected instance, including instance override gates. Values retain their existing type. CLI values may use JSON syntax for booleans, numbers, lists, and objects; omit `VALUE` to enter it interactively. Passwords, tokens, and API keys are redacted unless `--reveal-secrets` is explicitly used. Changes that configure process-wide services take full effect on the next headless invocation.

The generic API form accepts an operation from `api.describe` and an optional JSON
object, for example:

```text
lunalauncher-cli --cli --json api runtime.info
lunalauncher-cli --cli --json api instance.world.list {"instance":"INSTANCE"}
```

The result includes `apiVersion` and `operation` fields. `api.describe` returns the
operation catalog and JSON Schemas, including optional capability names and destructive
flags, so alternate UIs do not need to hard-code the GUI command set.

## TUI

Start the interactive terminal interface with:

```text
lunalauncher-cli --tui
```

The numbered menus expose instance and account lists, all supported login types, local or URL pack imports, instance launches, instance management, account management, resource management, Java installations, launcher settings, and instance settings. The settings editors filter and edit every registered setting while preserving the existing setting type and inheritance behavior. Passwords use hidden terminal input, Microsoft device codes are printed in the terminal, and Ctrl-C aborts the active task. Launches detach by default unless the wait option is selected.

## MCP

Start the MCP server with:

```text
lunalauncher-cli --mcp
```

Transport is JSON-RPC 2.0 over newline-delimited standard input/output. Launcher logs remain on standard error. The server supports `initialize`, `ping`, `tools/list`, `tools/call`, cancellation notifications, and empty resource/prompt lists.

The original compatibility tools remain available. `tools/list` also generates a
`lunalauncher_<operation>` tool for every entry in the API catalog, with its schema
copied from `api.describe`. This includes instance lifecycle and export, Minecraft
components, worlds, servers, Java management, resource inspection/update, logs and
screenshots, account profiles and skin/cape operations, runtime information, and task
status/cancellation.

Compatibility tools:

- `lunalauncher_instance_list`
- `lunalauncher_instance_info`
- `lunalauncher_instance_rename`
- `lunalauncher_instance_group`
- `lunalauncher_instance_copy`
- `lunalauncher_instance_update`
- `lunalauncher_instance_delete`
- `lunalauncher_instance_undo_delete`
- `lunalauncher_account_list`
- `lunalauncher_account_login`
- `lunalauncher_account_set_default`
- `lunalauncher_account_refresh`
- `lunalauncher_account_remove`
- `lunalauncher_instance_import`
- `lunalauncher_instance_launch`
- `lunalauncher_resource_list`
- `lunalauncher_resource_install`
- `lunalauncher_resource_enable`
- `lunalauncher_resource_disable`
- `lunalauncher_resource_remove`
- `lunalauncher_java_list`
- `lunalauncher_settings_list`
- `lunalauncher_settings_get`
- `lunalauncher_settings_set`
- `lunalauncher_settings_reset`

Authentication and task status events use `notifications/progress` when the request supplies a progress token, and `notifications/message` otherwise. Microsoft authentication includes the verification URL, device code, and expiry in the notification payload. Settings tools use `scope: "launcher"` or `scope: "instance"`; instance scope also requires `instance`. Sensitive values are redacted unless `reveal: true` is explicitly supplied. MCP calls are serialized because launcher instance, account, and settings operations mutate shared on-disk state.

Account, instance, resource, Java, import, launch, and settings operations share the same `OperationService` in CLI, TUI, and MCP. Operations that delete user data require `confirm: true` where declared by their schema; compatibility CLI deletion commands require `--yes` and the TUI asks interactively. The catalog's `destructive` flag describes mutations and does not itself introduce a universal confirmation parameter.

The API also exposes remote resource search, project/version/dependency lookup and
selected-version installation, component version catalogs, server YAML/loader
configuration, and optional integration controls. Resource installation from a local
path/direct URL remains available as `resource.install`; use
`resource.install-version` for indexed provider downloads. Dependencies are returned for
client selection and are not recursively installed.

For a Tauri/Neo UI sidecar, the same process additionally accepts `launcher/catalog`,
`launcher/execute` and `launcher/respond`, with correlated `launcher/event`
notifications. Task controls are accepted while a long operation is active. Cancellation
notifications must name that operation's JSON-RPC request ID. See
[NEO-UI-API.md](NEO-UI-API.md) for the protocol and TypeScript example and
[LAUNCHER-API.md](LAUNCHER-API.md) for the current coverage and remaining gaps.
