// SPDX-License-Identifier: GPL-3.0-only
// Copy into your Web UI's TypeScript sources. The host injects the implementation.
export {};

type Unlisten = () => void;
type Parameters = Record<string, unknown>;
type ApiResult<T> = {
  apiVersion: number;
  operation: string;
  exitCode: number;
} & ({ ok: true; data: T } | { ok: false; error: string });

interface Operation {
  name: string;
  description: string;
  apiVersion: number;
  inputSchema: Parameters;
  destructive: boolean;
  capability?: string;
}

interface LauncherEvent {
  kind: string;
  requestId?: string | number;
  data?: unknown;
  interactionId?: string;
  prompt?: string;
  secret?: boolean;
  choices?: unknown[];
  expiresIn?: number;
  [field: string]: unknown;
}

interface StreamBatch {
  subscriptionId: string;
  events: Array<Parameters & { kind: string; sequence: number }>;
  nextCursor: number;
  dropped: number;
  [field: string]: unknown;
}

interface WebUiEntry {
  id: string;
  name: string;
  version: string;
  description: string;
  source: 'builtin' | 'install' | 'data' | `custom:${string}`;
  path: string | null;
  error: string | null;
  nativeFrame: boolean;
}

interface WebUiSettings {
  rememberLastUi: boolean;
  startupUi: string;
  scanInstallDirectory: boolean;
  scanDataDirectory: boolean;
  additionalDirectories: string[];
  showManagerOnStart: boolean;
}

interface LunaBridge {
  readonly apiVersion: 1;
  execute<T = unknown>(operation: string, parameters?: Parameters): Promise<ApiResult<T>>;
  catalog(): Promise<{ apiVersion: number; operations: Operation[] }>;
  respond(parameters: { interactionId: string; value: string | number; cancel?: false } |
    { interactionId: string; cancel: true }): Promise<{ accepted: boolean }>;
  onEvent(handler: (event: LauncherEvent) => void): Promise<Unlisten>;
  onStream(handler: (batch: StreamBatch) => void): Promise<Unlisten>;
  onExit(handler: () => void): Promise<Unlisten>;
  openPath(path: string): Promise<void>;
  webUis(): Promise<{
    entries: WebUiEntry[];
    roots: Array<{ source: 'install' | 'data' | `custom:${string}`; path: string }>;
    active: string;
    warning: string | null;
    settingsPath: string;
    startupOverride: string | null;
  }>;
  webUiSettings(): Promise<WebUiSettings>;
  updateWebUiSettings(patch: Partial<WebUiSettings>): Promise<WebUiSettings>;
  switchWebUi(id: string): Promise<void>;
  openWebUiManager(): Promise<void>;
}

declare global {
  interface Window {
    readonly luna?: LunaBridge;
  }
}
