// SPDX-License-Identifier: MPL-2.0
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

type Priority = 'high' | 'normal';
type QueueEntry = {
  operation: string;
  parameters: Record<string, unknown>;
  priority: Priority;
  queuedAt: number;
  resolve: (value: ApiResult<any>) => void;
  reject: (reason?: unknown) => void;
};

const readOnlyTtl: Record<string, number> = {
  'api.describe': 60_000,
  'runtime.info': 5_000,
  'runtime.memory': 1_500,
  'instance.list': 2_000,
  'instance.info': 2_000,
  'instance.icons': 5_000,
  'account.list': 2_000,
  'settings.list': 1_000,
  'language.list': 30_000,
  'launcher.update.status': 30_000,
  'component.catalog': 30_000,
  'authentication.helper.status': 5_000,
  'scheduler.status': 500,
};

const highPriority = new Set([
  'runtime.info', 'runtime.memory', 'instance.list', 'instance.info', 'instance.icons', 'account.list',
  'settings.list', 'task.list', 'api.describe', 'component.catalog', 'component.versions',
  'language.list', 'launcher.update.status', 'authentication.helper.status',
]);

export class LauncherApiClient {
  // The sidecar still owns a single active operation; this scheduler keeps the
  // bridge fair so short UI reads are not stuck behind background mutations.
  private queue: QueueEntry[] = [];
  private running = false;
  private cache = new Map<string, { expires: number; value: ApiResult<any> }>();
  private inflight = new Map<string, Promise<ApiResult<any>>>();
  private stats = new Map<string, { calls: number; totalMs: number; maxMs: number; slow: number; cacheHits: number }>();

  private isTaskControl(operation: string): boolean {
    return new Set([
      'task.list',
      'task.status',
      'task.cancel',
      'neo.task.start',
      'neo.task.status',
      'neo.task.list',
      'neo.task.cancel',
      'neo.task.capabilities',
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

  private cacheKey(operation: string, parameters: Record<string, unknown>): string {
    return `${operation}:${JSON.stringify(parameters)}`;
  }

  private record(operation: string, startedAt: number, cacheHit = false) {
    const elapsed = performance.now() - startedAt;
    const stat = this.stats.get(operation) || { calls: 0, totalMs: 0, maxMs: 0, slow: 0, cacheHits: 0 };
    stat.calls += 1;
    stat.totalMs += elapsed;
    stat.maxMs = Math.max(stat.maxMs, elapsed);
    if (!cacheHit && elapsed > 900) stat.slow += 1;
    if (cacheHit) stat.cacheHits += 1;
    this.stats.set(operation, stat);
    if (!cacheHit && elapsed > 900) console.warn(`[launcher-api] slow ${operation}: ${Math.round(elapsed)} ms`);
  }

  /** Snapshot for the task/performance panel and console diagnostics. */
  getStats() {
    return [...this.stats.entries()].map(([operation, stat]) => ({
      operation, calls: stat.calls, cacheHits: stat.cacheHits,
      averageMs: Math.round((stat.totalMs / Math.max(1, stat.calls)) * 10) / 10,
      maxMs: Math.round(stat.maxMs), slow: stat.slow,
      queued: this.queue.filter(item => item.operation === operation).length,
    })).sort((a, b) => b.maxMs - a.maxMs);
  }

  async execute<T = unknown>(operation: string, parameters: Record<string, unknown> = {}): Promise<ApiResult<T>> {
    this.requireDesktop();
    const request = () => invoke<ApiResult<T>>('launcher_request', { method: 'launcher/execute', parameters: { operation, parameters } });
    if (this.isTaskControl(operation)) return request();

    const key = this.cacheKey(operation, parameters);
    const ttl = readOnlyTtl[operation] || 0;
    const cached = this.cache.get(key);
    if (ttl > 0 && cached && cached.expires > Date.now()) {
      this.record(operation, performance.now(), true);
      return Promise.resolve(cached.value as ApiResult<T>);
    }
    if (ttl > 0) {
      const pending = this.inflight.get(key);
      if (pending) {
        this.record(operation, performance.now(), true);
        return pending as Promise<ApiResult<T>>;
      }
    }

    const promise = new Promise<ApiResult<T>>((resolve, reject) => {
      this.queue.push({
        operation, parameters,
        priority: highPriority.has(operation) ? 'high' : 'normal',
        queuedAt: performance.now(),
        resolve: value => {
          resolve(value as ApiResult<T>);
          if (ttl > 0 && value.ok) this.cache.set(key, { expires: Date.now() + ttl, value });
          if (ttl === 0) this.cache.clear();
        },
        reject,
      });
      this.pump();
    });
    if (ttl > 0) {
      this.inflight.set(key, promise as Promise<ApiResult<any>>);
      void promise.finally(() => this.inflight.delete(key)).catch(() => {});
    }
    return promise;
  }

  private pump() {
    if (this.running) return;
    const index = this.queue.findIndex(item => item.priority === 'high');
    const entry = index >= 0 ? this.queue.splice(index, 1)[0] : this.queue.shift();
    if (!entry) return;
    this.running = true;
    queueMicrotask(async () => {
      const startedAt = performance.now();
      try {
        const result = await invoke<ApiResult<any>>('launcher_request', {
          method: 'launcher/execute',
          parameters: { operation: entry.operation, parameters: entry.parameters },
        });
        this.record(entry.operation, startedAt);
        entry.resolve(result);
      } catch (error) {
        this.record(entry.operation, startedAt);
        entry.reject(error);
      } finally {
        this.running = false;
        this.pump();
      }
    });
  }

  respond(parameters: Record<string, unknown>): Promise<unknown> {
    return invoke('launcher_request', { method: 'launcher/respond', parameters });
  }
  async openPath(path: string): Promise<void> {
    this.requireDesktop();
    await revealItemInDir(path);
  }
  async openWebUiManager(): Promise<void> {
    this.requireDesktop();
    await invoke('webui_manager');
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
  onExit(handler: (reason: { message?: string }) => void): Promise<UnlistenFn> {
    this.requireDesktop();
    return listen('launcher-exit', event => handler(event.payload));
  }
}

export const launcher = new LauncherApiClient();
if (typeof window !== 'undefined') {
  (window as any).__lunaApiStats = () => launcher.getStats();
}
