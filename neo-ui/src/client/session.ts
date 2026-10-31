// SPDX-License-Identifier: MPL-2.0
import type { ApiResult, LauncherEvent, LauncherInstance, LunaClient } from './types';

export type LauncherAccount = Record<string, any>;

export interface LauncherSessionState {
  instances: LauncherInstance[];
  accounts: LauncherAccount[];
  icons: Record<string, string>;
  selectedInstanceId: string;
  selectedAccountId: string;
  loading: boolean;
  error: string;
  ready: boolean;
}

export interface LauncherSessionSnapshot extends LauncherSessionState {
  selected: LauncherInstance | null;
  selectedAccount: LauncherAccount | null;
}

export interface SessionOptions {
  state?: LauncherSessionState;
  enabled?: () => boolean;
  startOnSubscribe?: boolean;
  persistInstanceSelection?: (id: string) => void;
  onAccountSelected?: (id: string) => void;
  onDebug?: (kind: string, source: string, message: string, data?: unknown) => void;
}

export interface LunaSession {
  getSnapshot(): LauncherSessionSnapshot;
  subscribe(listener: (snapshot: LauncherSessionSnapshot) => void): () => void;
  start(): Promise<void>;
  refresh(): Promise<void>;
  refreshIcons(): Promise<Record<string, string>>;
  selectInstance(value: string | LauncherInstance | null | undefined): void;
  selectAccount(value: string | LauncherAccount | null | undefined): void;
  setSelectedAccount(value: string | LauncherAccount | null | undefined): void;
  updateInstance(id: string, patch: Partial<LauncherInstance>): void;
  replaceInstances(next: LauncherInstance[]): void;
  replaceAccounts(next: LauncherAccount[]): void;
  dispose(): void;
}

export function instanceKey(value: any): string {
  return String(value?.id ?? '');
}

export function accountKey(value: any): string {
  return String(value?.internalId ?? value?.id ?? value?.profileName ?? '');
}

function sameValue(left: unknown, right: unknown): boolean {
  if (Object.is(left, right)) return true;
  if (left === null || right === null || typeof left !== 'object' || typeof right !== 'object') return false;
  try { return JSON.stringify(left) === JSON.stringify(right); } catch { return false; }
}

function patchEntity(target: Record<string, any>, incoming: Record<string, any>): string[] {
  const changed: string[] = [];
  for (const key of Object.keys(incoming)) {
    if (sameValue(target[key], incoming[key])) continue;
    target[key] = incoming[key];
    changed.push(key);
  }
  return changed;
}

function initialState(): LauncherSessionState {
  return {
    instances: [],
    accounts: [],
    icons: {},
    selectedInstanceId: '',
    selectedAccountId: '',
    loading: false,
    error: '',
    ready: false,
  };
}

export function createLunaSession(client: Pick<LunaClient, 'execute' | 'onEvent'>, options: SessionOptions = {}): LunaSession {
  const state = options.state ?? initialState();
  const listeners = new Set<(snapshot: LauncherSessionSnapshot) => void>();
  let refreshPromise: Promise<void> | undefined;
  let startPromise: Promise<void> | undefined;
  let stopEvents: (() => void) | undefined;
  let iconRequest = 0;

  const debug = (kind: string, source: string, message: string, data?: unknown) => options.onDebug?.(kind, source, message, data);

  function selectedInstance(): LauncherInstance | null {
    return state.instances.find(item => instanceKey(item) === state.selectedInstanceId) || null;
  }

  function selectedAccount(): LauncherAccount | null {
    return state.accounts.find(item => accountKey(item) === state.selectedAccountId) || null;
  }

  function getSnapshot(): LauncherSessionSnapshot {
    return {
      instances: state.instances.slice(),
      accounts: state.accounts.slice(),
      icons: { ...state.icons },
      selectedInstanceId: state.selectedInstanceId,
      selectedAccountId: state.selectedAccountId,
      loading: state.loading,
      error: state.error,
      ready: state.ready,
      selected: selectedInstance(),
      selectedAccount: selectedAccount(),
    };
  }

  function notify(): void {
    const snapshot = getSnapshot();
    for (const listener of listeners) {
      try { listener(snapshot); } catch (error) { console.error('[luna.session] subscriber failed', error); }
    }
  }

  function replaceInstances(next: LauncherInstance[]): void {
    const incoming = Array.isArray(next) ? next : [];
    const oldById = new Map(state.instances.map(item => [instanceKey(item), item]));
    const incomingIds = new Set(incoming.map(instanceKey));
    const changedEntities: Array<{ id: string; fields: string[] }> = [];
    const ordered = incoming.map(item => {
      const existing = oldById.get(instanceKey(item));
      if (existing) {
        const fields = patchEntity(existing as any, item as any);
        if (fields.length) changedEntities.push({ id: instanceKey(item), fields });
        return existing;
      }
      return item;
    });
    const orderChanged = ordered.length !== state.instances.length || ordered.some((item, index) => item !== state.instances[index]);
    if (orderChanged) state.instances.splice(0, state.instances.length, ...ordered);
    for (const id of Object.keys(state.icons)) {
      if (!incomingIds.has(id)) delete state.icons[id];
    }
    let selectionChanged = false;
    if (!state.selectedInstanceId || !incomingIds.has(state.selectedInstanceId)) {
      const next = incoming[0] ? instanceKey(incoming[0]) : '';
      if (state.selectedInstanceId !== next) selectionChanged = true;
      state.selectedInstanceId = next;
      options.persistInstanceSelection?.(next);
    }
    if (orderChanged || changedEntities.length || selectionChanged) {
      debug('store', 'launcher-session.instances', orderChanged ? `instances list patched (${ordered.length})` : 'instance entities patched', { orderChanged, changedEntities });
      notify();
    }
  }

  function replaceAccounts(next: LauncherAccount[]): void {
    const incoming = Array.isArray(next) ? next : [];
    const oldById = new Map(state.accounts.map(item => [accountKey(item), item]));
    const incomingIds = new Set(incoming.map(accountKey));
    const changedEntities: Array<{ id: string; fields: string[] }> = [];
    const ordered = incoming.map(item => {
      const existing = oldById.get(accountKey(item));
      if (existing) {
        const fields = patchEntity(existing, item);
        if (fields.length) changedEntities.push({ id: accountKey(item), fields });
        return existing;
      }
      return item;
    });
    const orderChanged = ordered.length !== state.accounts.length || ordered.some((item, index) => item !== state.accounts[index]);
    if (orderChanged) state.accounts.splice(0, state.accounts.length, ...ordered);
    let selectionChanged = false;
    if (!state.selectedAccountId || !incomingIds.has(state.selectedAccountId)) {
      const preferred = incoming.find(item => item.default) || incoming[0];
      const next = accountKey(preferred);
      if (state.selectedAccountId !== next) selectionChanged = true;
      state.selectedAccountId = next;
    }
    if (orderChanged || changedEntities.length || selectionChanged) {
      debug('store', 'launcher-session.accounts', orderChanged ? `accounts list patched (${ordered.length})` : 'account entities patched', { orderChanged, changedEntities });
      notify();
    }
  }

  function updateInstance(id: string, patch: Partial<LauncherInstance>): void {
    const item = state.instances.find(value => instanceKey(value) === String(id));
    if (!item) return;
    Object.assign(item, patch);
    notify();
  }

  function selectInstance(value: string | LauncherInstance | null | undefined): void {
    const id = typeof value === 'string' ? value : instanceKey(value);
    state.selectedInstanceId = id;
    options.persistInstanceSelection?.(id);
    notify();
  }

  function setSelectedAccount(value: string | LauncherAccount | null | undefined): void {
    const id = typeof value === 'string' ? value : accountKey(value);
    if (state.selectedAccountId === id) return;
    state.selectedAccountId = id;
    notify();
  }

  function selectAccount(value: string | LauncherAccount | null | undefined): void {
    const id = typeof value === 'string' ? value : accountKey(value);
    if (state.selectedAccountId === id) return;
    state.selectedAccountId = id;
    // Native Melody windows are separate webviews; let the shell mirror explicit
    // account selection without turning inbound sync events into a loop.
    options.onAccountSelected?.(id);
    notify();
  }

  async function refresh(): Promise<void> {
    if (options.enabled && !options.enabled()) return;
    if (refreshPromise) return refreshPromise;
    debug('refresh', 'launcher-session', 'refresh requested');
    refreshPromise = (async () => {
      state.loading = true;
      notify();
      try {
        const [instanceResult, accountResult] = await Promise.all([
          client.execute<LauncherInstance[]>('instance.list'),
          client.execute<LauncherAccount[]>('account.list'),
        ]);
        if (!instanceResult.ok) throw new Error(instanceResult.error || 'instance.list');
        if (!accountResult.ok) throw new Error(accountResult.error || 'account.list');
        replaceInstances((instanceResult.data || []) as LauncherInstance[]);
        replaceAccounts((accountResult.data || []) as LauncherAccount[]);
        state.error = '';
        state.ready = true;
        debug('refresh', 'launcher-session', 'refresh completed');
      } catch (cause) {
        state.error = String(cause);
        debug('error', 'launcher-session', 'refresh failed', String(cause));
      } finally {
        state.loading = false;
        refreshPromise = undefined;
        notify();
      }
    })();
    return refreshPromise;
  }

  async function refreshIcons(): Promise<Record<string, string>> {
    const request = ++iconRequest;
    const fallback = async () => {
      await Promise.all(state.instances.map(async instance => {
        try {
          const result = await client.execute<{ dataUrl?: string }>('instance.icon', { instance: instanceKey(instance) });
          if (request === iconRequest && result.ok && result.data?.dataUrl) state.icons[instanceKey(instance)] = result.data.dataUrl;
        } catch { /* Keep the dashboard fallback when an old backend has no icon API. */ }
      }));
    };
    try {
      const result = await client.execute<{ icons?: Array<{ id?: string; dataUrl?: string }> }>('instance.icons');
      if (request === iconRequest) {
        if (result.ok && Array.isArray(result.data?.icons)) {
          for (const icon of result.data!.icons!) {
            if (icon?.id && icon?.dataUrl) state.icons[icon.id] = icon.dataUrl;
          }
        } else {
          await fallback();
        }
      }
    } catch {
      if (request === iconRequest) await fallback();
    }
    if (request === iconRequest) {
      const ids = new Set(state.instances.map(instanceKey));
      for (const id of Object.keys(state.icons)) {
        if (!ids.has(id)) delete state.icons[id];
      }
      notify();
    }
    return { ...state.icons };
  }

  async function start(): Promise<void> {
    if (stopEvents) return;
    if (startPromise) return startPromise;
    startPromise = (async () => {
      try {
        stopEvents = await client.onEvent((event: LauncherEvent) => {
          const operation = String(event?.operation || event?.data?.operation || '');
          const kind = String(event?.kind || '');
          if (kind.includes('instance') || kind.includes('account') || operation.startsWith('instance.') || operation.startsWith('account.')) {
            debug('event', 'launcher-event', 'instance/account domain event received', { kind, operation });
            void refresh();
          }
        });
      } catch { /* Event bridge is optional for older native hosts. */ }
    })();
    try { await startPromise; } finally { startPromise = undefined; }
  }

  function subscribe(listener: (snapshot: LauncherSessionSnapshot) => void): () => void {
    listeners.add(listener);
    listener(getSnapshot());
    if (options.startOnSubscribe !== false) {
      void start().then(() => refresh()).catch(() => {});
    }
    return () => { listeners.delete(listener); };
  }

  function dispose(): void {
    stopEvents?.();
    stopEvents = undefined;
    listeners.clear();
  }

  return {
    getSnapshot,
    subscribe,
    start,
    refresh,
    refreshIcons,
    selectInstance,
    selectAccount,
    setSelectedAccount,
    updateInstance,
    replaceInstances,
    replaceAccounts,
    dispose,
  };
}
