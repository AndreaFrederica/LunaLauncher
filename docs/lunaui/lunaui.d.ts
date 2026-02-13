// LunaUI type definitions for custom panel scripts.
// Path reference example (inside a lunaui/*.js file):
// /// <reference path="../../docs/lunaui/lunaui.d.ts" />

export {};

declare global {
    type JsonPrimitive = string | number | boolean | null;
    type JsonValue = JsonPrimitive | JsonObject | JsonValue[];
    interface JsonObject {
        [key: string]: JsonValue;
    }

    interface LunaContext {
        languageId: string;
        instanceRoot: string;
        gameRoot: string;
    }

    interface LunaModState {
        query: string;
        normalized: string;
        found: boolean;
        enabled?: boolean;
        fileName?: string;
        path?: string;
    }

    interface LunaManagedMod {
        scope: "mods" | "coremods" | "nilmods" | string;
        fileName: string;
        normalized: string;
        enabled: boolean;
        path: string;
    }

    interface LunaFsDirEntry {
        name: string;
        path: string;
        isFile: boolean;
        isDir: boolean;
        size: number;
    }

    interface LunaFsApi {
        exists(path: string): boolean;
        readFile(path: string): string | undefined;
        writeFile(path: string, data: string): boolean;
        readdir(path: string): LunaFsDirEntry[];
        mkdir(path: string, recursive?: boolean): boolean;
        rm(path: string, recursive?: boolean): boolean;
    }

    interface LunaLauncherApi {
        setModEnabled(mod: string, enabled: boolean): boolean;
        activateVariant(group: string, option: string): boolean;
        getModState(mod: string): LunaModState;
        isModEnabled(mod: string): boolean | null;
        listMods(filter?: string): LunaManagedMod[];
        getLanguageId(): string;
        setState(key: string, value: JsonValue): boolean;
        getState(key: string): JsonValue | undefined;
        saveState(): boolean;
        fs: LunaFsApi;
    }

    interface LunaI18nMap {
        [languageId: string]: string;
    }

    type LunaLocalizedText = string | LunaI18nMap;

    interface LunaBaseControl {
        id?: string;
        type: string;
        label?: LunaLocalizedText;
        text?: LunaLocalizedText;
        title?: LunaLocalizedText;
        tooltip?: LunaLocalizedText;
        action?: LunaActionValue;
        onChange?: string;
        onClick?: string;
        saveOnChange?: boolean;
        [key: string]: JsonValue | undefined;
    }

    interface LunaTextControl extends LunaBaseControl {
        type: "text" | "label";
    }

    interface LunaSeparatorControl extends LunaBaseControl {
        type: "separator";
    }

    interface LunaToggleControl extends LunaBaseControl {
        type: "toggle" | "checkbox";
        default?: boolean;
        mod?: string;
    }

    interface LunaSelectOptionObject {
        label?: LunaLocalizedText;
        value: string | number | boolean;
        description?: LunaLocalizedText;
        [key: string]: JsonValue | undefined;
    }

    interface LunaSelectControl extends LunaBaseControl {
        type: "select" | "combo";
        options: Array<LunaSelectOptionObject | string>;
        default?: string | number | boolean;
        variantGroup?: string;
    }

    interface LunaButtonControl extends LunaBaseControl {
        type: "button";
    }

    interface LunaModDetailsItem {
        mod?: string;
        name?: LunaLocalizedText;
        label?: LunaLocalizedText;
        description?: LunaLocalizedText;
        warning?: LunaLocalizedText;
        homepage?: string;
        authors?: LunaLocalizedText;
        license?: string;
        issueTracker?: string;
        showFile?: boolean;
        [key: string]: JsonValue | undefined;
    }

    interface LunaModDetailsControl extends LunaBaseControl {
        type: "moddetails" | "moddetail" | "modinfo" | "mod-info";
        items?: LunaModDetailsItem[];
        mods?: string | string[];
    }

    type LunaControl =
        | LunaTextControl
        | LunaSeparatorControl
        | LunaToggleControl
        | LunaSelectControl
        | LunaButtonControl
        | LunaModDetailsControl
        | LunaBaseControl;

    interface LunaActionSetModEnabled {
        action: "setModEnabled";
        mod: string;
        enabled?: boolean;
    }

    interface LunaActionActivateVariant {
        action: "activateVariant";
        group: string;
        value?: string;
    }

    interface LunaActionSetState {
        action: "setState";
        key?: string;
        value?: JsonValue;
    }

    interface LunaActionSaveState {
        action: "saveState";
    }

    interface LunaActionReloadTabs {
        action: "reloadTabs" | "refreshTabs";
    }

    interface LunaActionRunHandler {
        action: "runHandler" | "callHandler";
        handler: string;
    }

    type LunaActionObject =
        | LunaActionSetModEnabled
        | LunaActionActivateVariant
        | LunaActionSetState
        | LunaActionSaveState
        | LunaActionReloadTabs
        | LunaActionRunHandler;

    type LunaActionValue = LunaActionObject | LunaActionObject[] | string;

    interface LunaVariantGroupEntryObject {
        mods?: string | string[];
        [key: string]: JsonValue | undefined;
    }

    type LunaVariantGroupEntry = string | string[] | LunaVariantGroupEntryObject;
    type LunaVariantGroup = Record<string, LunaVariantGroupEntry>;
    type LunaVariantGroups = Record<string, LunaVariantGroup>;

    interface LunaTabDefinition {
        title?: LunaLocalizedText;
        name?: LunaLocalizedText;
        controls: LunaControl[];
        variantGroups?: LunaVariantGroups;
        [key: string]: JsonValue | undefined;
    }

    interface LunaPageMetadata {
        name?: LunaLocalizedText;
        displayName?: LunaLocalizedText;
        name_i18n?: LunaI18nMap;
        displayName_i18n?: LunaI18nMap;
        [key: string]: JsonValue | undefined;
    }

    interface LunaPageDefinition {
        panel?: LunaPageMetadata;
        panelName?: LunaLocalizedText;
        pageName?: LunaLocalizedText;
        title?: LunaLocalizedText;
        controls?: LunaControl[];
        tabs?: LunaTabDefinition[];
        variantGroups?: LunaVariantGroups;
        [key: string]: JsonValue | undefined;
    }

    type LunaTabsReturn = LunaPageDefinition | LunaTabDefinition[] | LunaTabDefinition;

    const launcher: LunaLauncherApi;

    // Either define getTabs(...)
    function getTabs(state: JsonObject, ctx: LunaContext): LunaTabsReturn;

    // ...or expose a global tabs value.
    const tabs: LunaTabsReturn | undefined;
}
