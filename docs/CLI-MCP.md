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
```

Passwords are never accepted as command-line arguments. Use hidden terminal input or `--password-stdin`. `--non-interactive` makes missing input an error. Launches detach after the game starts unless `--wait` is present. Ctrl-C aborts the active task.

Normal archive URLs and local packs are handled by `InstanceImportTask`. Modrinth and CurseForge project file pages are resolved through their APIs. Restricted CurseForge files require an enabled external tool that reports `headless: true` during its protocol probe.

## TUI

Start the interactive terminal interface with:

```text
lunalauncher --tui
```

The numbered menus expose instance and account lists, all supported login types, local or URL pack imports, and instance launches. Passwords use hidden terminal input, Microsoft device codes are printed in the terminal, and Ctrl-C aborts the active task. Launches detach by default unless the wait option is selected.

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

Authentication and task status events use `notifications/progress` when the request supplies a progress token, and `notifications/message` otherwise. Microsoft authentication includes the verification URL, device code, and expiry in the notification payload. MCP calls are serialized because launcher instance and account tasks mutate shared on-disk state.
