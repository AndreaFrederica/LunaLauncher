# LunaUI Custom Panel Development

This document describes the `lunaui` custom panel system used by Luna Launcher instances.

## Overview

- Location: `<instance-root>/lunaui`
- Loader behavior:
  - Recursively scans `*.json` and `*.js`
  - Skips `state.json`
  - Executes each JS file in QuickJS
- Panel data sources:
  - JS: define `getTabs(state, ctx)` or global `tabs`
  - JSON: direct tab/page definition

## Safety Model

- Scripts run in a sandboxed QuickJS runtime with memory, stack, and timeout limits.
- `launcher.fs` is restricted to the current instance root (cannot access arbitrary system paths).
- JS in `lunaui` is executable content. Treat imported packs/scripts as trusted code.

## TypeScript/JSDoc IntelliSense

Use the provided type definitions:

- File: `docs/lunaui/lunaui.d.ts`

In your `lunaui/*.js` script:

```js
/// <reference path="../../docs/lunaui/lunaui.d.ts" />
```

For external editors/workspaces, copy or symlink `lunaui.d.ts` near your scripts.

## Runtime Context

`getTabs(state, ctx)` receives:

- `state`: persisted state from `lunaui/state.json`
- `ctx.languageId`: current launcher language id
- `ctx.instanceRoot`: instance root path
- `ctx.gameRoot`: game directory path

## Global API

Available global object:

- `launcher.setModEnabled(mod, enabled): boolean`
- `launcher.activateVariant(group, option): boolean`
- `launcher.getModState(mod): { found, enabled, fileName, path, ... }`
- `launcher.isModEnabled(mod): boolean | null`
- `launcher.listMods(filter?): LunaManagedMod[]`
- `launcher.getLanguageId(): string`
- `launcher.setState(key, value): boolean`
- `launcher.getState(key): JsonValue | undefined`
- `launcher.saveState(): boolean`
- `launcher.fs.*`:
  - `exists(path)`
  - `readFile(path)`
  - `writeFile(path, data)`
  - `readdir(path)`
  - `mkdir(path, recursive?)`
  - `rm(path, recursive?)`

## Action Objects

Controls support:

- `setModEnabled`
- `activateVariant`
- `setState`
- `saveState`
- `reloadTabs` / `refreshTabs`
- `runHandler` / `callHandler`

`action` can be:

- one object
- array of objects
- string handler name

## Control Types

Common control `type` values:

- `text` / `label`
- `separator`
- `toggle` / `checkbox`
- `select` / `combo`
- `button`
- `moddetails` (`moddetail` / `modinfo` / `mod-info` aliases)

## `moddetails` Notes

- If `mod` is provided and file exists, launcher parses real mod metadata and renders details like the mod list panel.
- JSON fields (`name`, `description`, `license`, `warning`, etc.) are fallback/augmentation.

## Minimal Example

```js
/// <reference path="../../docs/lunaui/lunaui.d.ts" />

function getTabs(state, ctx) {
  const enabled = launcher.isModEnabled("ExampleMod.jar") === true;

  return {
    panel: {
      name_i18n: {
        en_US: "Client Settings",
        zh_CN: "客户端设置"
      }
    },
    title: "Example",
    controls: [
      {
        id: "example_mod_enabled",
        type: "toggle",
        label: "Enable ExampleMod",
        default: enabled,
        action: [
          { action: "setModEnabled", mod: "ExampleMod.jar" },
          { action: "saveState" },
          { action: "reloadTabs" }
        ]
      },
      {
        type: "moddetails",
        items: [{ mod: "ExampleMod.jar" }]
      }
    ]
  };
}
```
