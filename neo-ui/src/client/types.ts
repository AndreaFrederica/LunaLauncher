// SPDX-License-Identifier: MPL-2.0

export type ApiResult<T = unknown> = {
  ok: boolean;
  data?: T;
  error?: string;
  operation?: string;
  exitCode?: number;
  apiVersion?: number;
};

/** Stable object-shaped instance contract used by Neo UI and Web UI adapters. */
export interface LauncherInstance {
  id: string;
  name: string;
  type: string;
  group: string;
  icon: string;
  notes: string;
  paths?: { root: string; game: string; mods: string };
  lifecycle?: { running: boolean; lastLaunch: string; playTimeSeconds: number };
  managed?: { enabled: boolean; type: string; projectId: string; versionId: string };
  runtime?: { minecraftVersion?: string };
  capabilities?: Record<string, boolean>;
  running?: boolean;
  managedType?: string;
  managedId?: string;
  managedVersionId?: string;
  lastLaunch?: string;
  playTimeSeconds?: number;
}

export interface LauncherSettingsEntry {
  key: string;
  type: string;
  value: unknown;
  default: unknown;
  isDefault: boolean;
  sensitive: boolean;
  redacted: boolean;
  category?: string;
  surface?: 'shared' | 'qt';
}

export interface LauncherProfile {
  runtime: Record<string, unknown>;
  settings: { scope: 'launcher'; entries: LauncherSettingsEntry[] };
  accounts: unknown[];
  capabilities: string[];
}

export type HostUnlisten = () => void;

/**
 * Thin transport boundary. The bundled UI uses the Tauri API package; the
 * injected Web UI bridge uses the global host object. Everything above this
 * interface is shared TypeScript.
 */
export interface HostTransport {
  invoke<T>(command: string, args?: Record<string, unknown>): Promise<T>;
  listen<T>(event: string, handler: (payload: T) => void): Promise<HostUnlisten>;
}

export interface ClientOptions {
  transport: HostTransport;
  isDesktop?: () => boolean;
  translate?: (key: string) => string;
  now?: () => number;
  onSlow?: (operation: string, elapsedMs: number) => void;
}

export type ClientPriority = 'high' | 'normal';

export interface ClientStat {
  operation: string;
  calls: number;
  cacheHits: number;
  averageMs: number;
  maxMs: number;
  slow: number;
  queued: number;
}

export interface LauncherEvent {
  kind?: string;
  operation?: string;
  data?: Record<string, any>;
  [field: string]: unknown;
}

export interface StreamBatch {
  subscriptionId?: string;
  events?: Array<Record<string, any>>;
  [field: string]: unknown;
}

export interface LunaClient {
  execute<T = unknown>(operation: string, parameters?: Record<string, unknown>): Promise<ApiResult<T>>;
  request<T = unknown>(method: string, parameters?: Record<string, unknown>): Promise<T>;
  respond(parameters: Record<string, unknown>): Promise<unknown>;
  invalidate(operation: string): void;
  getStats(): ClientStat[];
  onEvent(handler: (event: LauncherEvent) => void): Promise<HostUnlisten>;
  onStream(handler: (batch: StreamBatch) => void): Promise<HostUnlisten>;
  onExit(handler: (reason: { message?: string }) => void): Promise<HostUnlisten>;
}
