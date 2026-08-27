# Headless CLI, TUI, and MCP

The headless entry points initialize the normal launcher data, task, account, import, and launch services without showing a window. They do not create a daemon. The process exits after a CLI command, when the TUI is closed, after a detached game starts, or when MCP standard input closes.

## CLI

```text
lunalauncher --cli instance list [--json]
lunalauncher --cli account list [--json]
lunalauncher --cli account login microsoft [--minecraft-profile-name NAME]
lunalauncher --cli account login offline --username NAME
lunalauncher --cli account login yggdrasil --username USER --auth-url URL --session-url URL [--password-stdin]
lunalauncher --cli account login unified-pass --username USER --server-id ID [--password-stdin]
lunalauncher --cli import PATH_OR_URL [--name NAME]
lunalauncher --cli launch INSTANCE [--profile NAME | --offline NAME] [--server ADDRESS | --world WORLD] [--wait]
lunalauncher --cli settings list launcher [FILTER]
lunalauncher --cli settings get launcher KEY [--reveal-secrets]
lunalauncher --cli settings set launcher KEY [VALUE]
lunalauncher --cli settings reset launcher KEY
lunalauncher --cli settings list instance INSTANCE [FILTER]
lunalauncher --cli settings get instance INSTANCE KEY [--reveal-secrets]
lunalauncher --cli settings set instance INSTANCE KEY [VALUE]
lunalauncher --cli settings reset instance INSTANCE KEY
```

Authentication passwords and sensitive setting values are never accepted as command-line arguments. Use hidden terminal input or `--password-stdin`. `--non-interactive` makes missing input an error. Launches detach after the game starts unless `--wait` is present. Ctrl-C aborts the active task.

Normal archive URLs and local packs are handled by `InstanceImportTask`. Modrinth and CurseForge project file pages are resolved through their APIs. Restricted CurseForge files require an enabled external tool that reports `headless: true` during its protocol probe.

Settings commands expose every setting registered by the launcher or selected instance, including instance override gates. Values retain their existing type. CLI values may use JSON syntax for booleans, numbers, lists, and objects; omit `VALUE` to enter it interactively. Passwords, tokens, and API keys are redacted unless `--reveal-secrets` is explicitly used. Changes that configure process-wide services take full effect on the next headless invocation.

## TUI

Start the interactive terminal interface with:

```text
lunalauncher --tui
```

The numbered menus expose instance and account lists, all supported login types, local or URL pack imports, instance launches, launcher settings, and instance settings. The settings editors filter and edit every registered setting while preserving the existing setting type and inheritance behavior. Passwords use hidden terminal input, Microsoft device codes are printed in the terminal, and Ctrl-C aborts the active task. Launches detach by default unless the wait option is selected.

## MCP

Start the MCP server with:

```text
lunalauncher --mcp
```

Transport is JSON-RPC 2.0 over newline-delimited standard input/output. Launcher logs remain on standard error. The server supports `initialize`, `ping`, `tools/list`, `tools/call`, cancellation notifications, and empty resource/prompt lists.

Tools:

- `lunalauncher_instance_list`
- `lunalauncher_account_list`
- `lunalauncher_account_login`
- `lunalauncher_instance_import`
- `lunalauncher_instance_launch`
- `lunalauncher_settings_list`
- `lunalauncher_settings_get`
- `lunalauncher_settings_set`
- `lunalauncher_settings_reset`

Authentication and task status events use `notifications/progress` when the request supplies a progress token, and `notifications/message` otherwise. Microsoft authentication includes the verification URL, device code, and expiry in the notification payload. Settings tools use `scope: "launcher"` or `scope: "instance"`; instance scope also requires `instance`. Sensitive values are redacted unless `reveal: true` is explicitly supplied. MCP calls are serialized because launcher instance, account, and settings operations mutate shared on-disk state.
