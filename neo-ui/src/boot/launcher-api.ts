import { invoke, isTauri } from '@tauri-apps/api/core';
import { listen, type UnlistenFn } from '@tauri-apps/api/event';

export type ApiResult<T = unknown> = { ok: boolean; data?: T; error?: string; operation?: string };

/** Stable object-shaped instance contract used by Neo UI and future UI adapters. */
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
}

export interface LauncherProfile {
  runtime: Record<string, unknown>;
  settings: { scope: 'launcher'; entries: LauncherSettingsEntry[] };
  accounts: unknown[];
  capabilities: string[];
}

export class LauncherApiClient {
  private requireDesktop(): void {
    if (!isTauri()) throw new Error('当前为网页预览。请启动安装目录中的 luna-neo-ui.exe，以连接启动器并读取实例、账户和设置。');
  }
  async execute<T = unknown>(operation: string, parameters: Record<string, unknown> = {}): Promise<ApiResult<T>> {
    this.requireDesktop();
    return invoke<ApiResult<T>>('launcher_request', { method: 'launcher/execute', parameters: { operation, parameters } });
  }
  respond(parameters: Record<string, unknown>): Promise<unknown> {
    return invoke('launcher_request', { method: 'launcher/respond', parameters });
  }
  async openPath(path: string): Promise<void> {
    this.requireDesktop();
    await invoke('open_path', { path });
  }
  onEvent(handler: (event: unknown) => void): Promise<UnlistenFn> {
    this.requireDesktop();
    return listen('launcher-event', event => handler(event.payload));
  }
  async getInstance(reference: string): Promise<ApiResult<LauncherInstance>> {
    return this.execute<LauncherInstance>('instance.get', { instance: reference });
  }
  async listInstances(): Promise<ApiResult<LauncherInstance[]>> {
    return this.execute<LauncherInstance[]>('instance.list');
  }
  async getLauncher(): Promise<ApiResult<LauncherProfile>> {
    return this.execute<LauncherProfile>('launcher.get');
  }
  onStream(handler: (batch: unknown) => void): Promise<UnlistenFn> {
    this.requireDesktop();
    return listen('launcher-stream', event => handler(event.payload));
  }
  onExit(handler: () => void): Promise<UnlistenFn> {
    this.requireDesktop();
    return listen('launcher-exit', handler);
  }
}
export const launcher = new LauncherApiClient();
