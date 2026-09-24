import { invoke, isTauri } from '@tauri-apps/api/core';
import { listen, type UnlistenFn } from '@tauri-apps/api/event';
import { revealItemInDir } from '@tauri-apps/plugin-opener';

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
  category?: string;
  surface?: 'shared' | 'qt';
}

export interface LauncherProfile {
  runtime: Record<string, unknown>;
  settings: { scope: 'launcher'; entries: LauncherSettingsEntry[] };
  accounts: unknown[];
  capabilities: string[];
}

export class LauncherApiClient {
  // The MCP sidecar executes normal launcher operations one at a time.  A
  // number of XMCL views intentionally load independent panels together, so
  // serialize ordinary calls here instead of letting the sidecar reject the
  // second request with "Another launcher operation is already running".
  private operationTail: Promise<void> = Promise.resolve();

  private isTaskControl(operation: string): boolean {
    return new Set([
      'task.list',
      'task.status',
      'task.cancel',
      'api.describe',
      'event.poll',
      'event.subscriptions',
      'event.unsubscribe',
      'server.console.command',
      'server.console.write',
      'server.console.resize',
    ]).has(operation);
  }

  private requireDesktop(): void {
    if (!isTauri()) throw new Error('当前为网页预览。请启动安装目录中的 luna-neo-ui.exe，以连接启动器并读取实例、账户和设置。');
  }
  async execute<T = unknown>(operation: string, parameters: Record<string, unknown> = {}): Promise<ApiResult<T>> {
    this.requireDesktop();
    const request = () => invoke<ApiResult<T>>('launcher_request', { method: 'launcher/execute', parameters: { operation, parameters } });
    if (this.isTaskControl(operation)) return request();

    const previous = this.operationTail;
    let release!: () => void;
    this.operationTail = new Promise<void>(resolve => { release = resolve; });
    await previous;
    try {
      return await request();
    } finally {
      release();
    }
  }
  respond(parameters: Record<string, unknown>): Promise<unknown> {
    return invoke('launcher_request', { method: 'launcher/respond', parameters });
  }
  async openPath(path: string): Promise<void> {
    this.requireDesktop();
    // Directories must use the plugin's Explorer/reveal operation. `openPath`
    // delegates to the default file association and can reopen Documents on
    // Windows when Explorer is already running.
    await revealItemInDir(path);
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
