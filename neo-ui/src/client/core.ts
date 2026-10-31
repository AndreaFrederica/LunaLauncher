// SPDX-License-Identifier: MPL-2.0
import type {
  ApiResult,
  ClientOptions,
  ClientPriority,
  ClientStat,
  LauncherEvent,
  LunaClient,
  StreamBatch,
} from './types';

type QueueEntry = {
  operation: string;
  parameters: Record<string, unknown>;
  priority: ClientPriority;
  resolve: (value: ApiResult<any>) => void;
  reject: (reason?: unknown) => void;
};

const READ_ONLY_TTL: Record<string, number> = {
  'api.describe': 60_000,
  'runtime.info': 5_000,
  'startup.status': 2_000,
  'runtime.memory': 1_500,
  'instance.list': 2_000,
  'instance.info': 2_000,
  'instance.icons': 5_000,
  'instance.icon-library': 30_000,
  'account.list': 2_000,
  'settings.list': 1_000,
  'language.list': 30_000,
  'launcher.update.status': 30_000,
  'component.catalog': 30_000,
  'authentication.helper.status': 5_000,
  'scheduler.status': 500,
};

const HIGH_PRIORITY = new Set([
  'runtime.info', 'startup.status', 'runtime.memory', 'instance.list', 'instance.info', 'instance.icons', 'instance.icon-library', 'account.list',
  'settings.list', 'task.list', 'api.describe', 'component.catalog', 'component.versions',
  'language.list', 'launcher.update.status', 'authentication.helper.status',
]);

const TASK_CONTROL = new Set([
  'task.list',
  'task.status',
  'task.cancel',
  'neo.task.start',
  'neo.task.status',
  'neo.task.list',
  'neo.task.cancel',
  'neo.task.respond',
  'neo.task.capabilities',
  'api.describe',
  'event.poll',
  'event.subscriptions',
  'event.unsubscribe',
  'server.console.command',
  'server.console.write',
  'server.console.resize',
]);

export function createLunaClient(options: ClientOptions): LunaClient {
  const { transport } = options;
  const now = options.now ?? (() => (typeof performance !== 'undefined' ? performance.now() : Date.now()));
  const queue: QueueEntry[] = [];
  const cache = new Map<string, { expires: number; value: ApiResult<any> }>();
  const inflight = new Map<string, Promise<ApiResult<any>>>();
  const stats = new Map<string, { calls: number; totalMs: number; maxMs: number; slow: number; cacheHits: number }>();
  let running = false;

  function requireDesktop(): void {
    if (options.isDesktop && !options.isDesktop()) {
      throw new Error(options.translate?.('neo.extra.webPreview') ?? 'This page is only available inside the launcher.');
    }
  }

  function request<T>(method: string, parameters: Record<string, unknown>): Promise<T> {
    return transport.invoke<T>('launcher_request', { method, parameters });
  }

  function cacheKey(operation: string, parameters: Record<string, unknown>): string {
    return `${operation}:${JSON.stringify(parameters)}`;
  }

  function record(operation: string, startedAt: number, cacheHit = false): void {
    const elapsed = now() - startedAt;
    const stat = stats.get(operation) || { calls: 0, totalMs: 0, maxMs: 0, slow: 0, cacheHits: 0 };
    stat.calls += 1;
    stat.totalMs += elapsed;
    stat.maxMs = Math.max(stat.maxMs, elapsed);
    if (!cacheHit && elapsed > 900) stat.slow += 1;
    if (cacheHit) stat.cacheHits += 1;
    stats.set(operation, stat);
    if (!cacheHit && elapsed > 900) options.onSlow?.(operation, elapsed);
  }

  function invalidate(operation: string): void {
    // Other native windows can mutate data without passing through this client.
    for (const key of cache.keys()) if (key.startsWith(`${operation}:`)) cache.delete(key);
  }

  function getStats(): ClientStat[] {
    return [...stats.entries()].map(([operation, stat]) => ({
      operation,
      calls: stat.calls,
      cacheHits: stat.cacheHits,
      averageMs: Math.round((stat.totalMs / Math.max(1, stat.calls)) * 10) / 10,
      maxMs: Math.round(stat.maxMs),
      slow: stat.slow,
      queued: queue.filter(item => item.operation === operation).length,
    })).sort((a, b) => b.maxMs - a.maxMs);
  }

  function pump(): void {
    if (running) return;
    const index = queue.findIndex(item => item.priority === 'high');
    const entry = index >= 0 ? queue.splice(index, 1)[0] : queue.shift();
    if (!entry) return;
    running = true;
    queueMicrotask(async () => {
      const startedAt = now();
      try {
        const result = await request<ApiResult<any>>('launcher/execute', {
          operation: entry.operation,
          parameters: entry.parameters,
        });
        record(entry.operation, startedAt);
        entry.resolve(result);
      } catch (error) {
        record(entry.operation, startedAt);
        entry.reject(error);
      } finally {
        running = false;
        pump();
      }
    });
  }

  function execute<T = unknown>(operation: string, parameters: Record<string, unknown> = {}): Promise<ApiResult<T>> {
    requireDesktop();
    if (TASK_CONTROL.has(operation)) {
      return request<ApiResult<T>>('launcher/execute', { operation, parameters });
    }

    const key = cacheKey(operation, parameters);
    const ttl = READ_ONLY_TTL[operation] || 0;
    const cached = cache.get(key);
    if (ttl > 0 && cached && cached.expires > Date.now()) {
      record(operation, now(), true);
      return Promise.resolve(cached.value as ApiResult<T>);
    }
    if (ttl > 0) {
      const pending = inflight.get(key);
      if (pending) {
        record(operation, now(), true);
        return pending as Promise<ApiResult<T>>;
      }
    }

    const promise = new Promise<ApiResult<T>>((resolve, reject) => {
      queue.push({
        operation,
        parameters,
        priority: HIGH_PRIORITY.has(operation) ? 'high' : 'normal',
        resolve: value => {
          resolve(value as ApiResult<T>);
          if (ttl > 0 && value.ok) cache.set(key, { expires: Date.now() + ttl, value });
          if (ttl === 0) cache.clear();
        },
        reject,
      });
      pump();
    });
    if (ttl > 0) {
      inflight.set(key, promise as Promise<ApiResult<any>>);
      void promise.finally(() => inflight.delete(key)).catch(() => {});
    }
    return promise;
  }

  function respond(parameters: Record<string, unknown>): Promise<unknown> {
    return request('launcher/respond', parameters);
  }

  function onEvent(handler: (event: LauncherEvent) => void): Promise<() => void> {
    requireDesktop();
    return transport.listen<LauncherEvent>('launcher-event', handler);
  }

  function onStream(handler: (batch: StreamBatch) => void): Promise<() => void> {
    requireDesktop();
    return transport.listen<StreamBatch>('launcher-stream', handler);
  }

  function onExit(handler: (reason: { message?: string }) => void): Promise<() => void> {
    requireDesktop();
    return transport.listen<{ message?: string }>('launcher-exit', handler);
  }

  return {
    execute,
    request: (method, parameters = {}) => request(method, parameters),
    respond,
    invalidate,
    getStats,
    onEvent,
    onStream,
    onExit,
  };
}
