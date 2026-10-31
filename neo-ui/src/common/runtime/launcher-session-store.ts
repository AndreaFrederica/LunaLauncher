import { computed, ref } from 'vue';
import { defineStore } from 'pinia';
import { isTauri } from '@tauri-apps/api/core';
import { emit, listen } from '@tauri-apps/api/event';
import { launcher, type LauncherInstance } from '../../boot/launcher-api';
import { neoUiPreferences, persistNeoUiPreferences } from '../../neo-ui-preferences';
import { recordRuntimeDebug } from './runtime-debug';

type LauncherAccount = Record<string, any>;

function keyOf(value: any): string {
  return String(value?.id ?? '');
}

function accountKeyOf(value: any): string {
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

/** Shared launcher data model used by every shell and every native window. */
export const useLauncherSessionStore = defineStore('launcher-session', () => {
  const instances = ref<LauncherInstance[]>([]);
  const accounts = ref<LauncherAccount[]>([]);
  const icons = ref<Record<string, string>>({});
  const selectedInstanceId = ref(String(neoUiPreferences.selectedInstanceId || ''));
  const selectedAccountId = ref('');
  const loading = ref(false);
  const error = ref('');
  let refreshRequest: Promise<void> | undefined;
  let eventsStarted = false;
  let stopLauncherEvent: (() => void) | undefined;
  let stopSessionEvent: (() => void) | undefined;
  let stopAccountSelectionEvent: (() => void) | undefined;

  const selected = computed<LauncherInstance | null>({
    get: () => instances.value.find(item => keyOf(item) === selectedInstanceId.value) || null,
    set: value => selectInstance(value),
  });
  const selectedAccount = computed(() => accounts.value.find(item => accountKeyOf(item) === selectedAccountId.value) || null);

  function persistSelection() {
    neoUiPreferences.selectedInstanceId = selectedInstanceId.value;
    persistNeoUiPreferences();
  }

  function selectInstance(value: string | LauncherInstance | null | undefined) {
    const id = typeof value === 'string' ? value : keyOf(value);
    selectedInstanceId.value = id;
    persistSelection();
  }

  function selectAccount(value: string | LauncherAccount | null | undefined) {
    const next = typeof value === 'string' ? value : accountKeyOf(value);
    if (selectedAccountId.value === next) return;
    selectedAccountId.value = next;
    // Native Melody preference/account windows are separate webviews. A
    // dedicated selection event keeps their runtime state in sync without
    // causing unrelated components to refresh or rebuild.
    if (isTauri()) void emit('launcher-account-selected', { accountId: next });
  }

  function replaceInstances(next: LauncherInstance[]) {
    const incoming = Array.isArray(next) ? next : [];
    const oldById = new Map(instances.value.map(item => [keyOf(item), item]));
    const incomingIds = new Set(incoming.map(keyOf));
    const changedEntities: Array<{ id: string; fields: string[] }> = [];
    const ordered = incoming.map(item => {
      const existing = oldById.get(keyOf(item));
      if (existing) {
        const fields = patchEntity(existing as any, item as any);
        if (fields.length) changedEntities.push({ id: keyOf(item), fields });
        return existing;
      }
      return item;
    });
    const orderChanged = ordered.length !== instances.value.length || ordered.some((item, index) => item !== instances.value[index]);
    if (orderChanged) instances.value.splice(0, instances.value.length, ...ordered);
    if (!selectedInstanceId.value || !incomingIds.has(selectedInstanceId.value)) {
      selectedInstanceId.value = incoming[0] ? keyOf(incoming[0]) : '';
      persistSelection();
    }
    if (orderChanged || changedEntities.length) recordRuntimeDebug('store', 'launcher-session.instances', orderChanged ? `instances list patched (${ordered.length})` : 'instance entities patched', { orderChanged, changedEntities });
  }

  function replaceAccounts(next: LauncherAccount[]) {
    const incoming = Array.isArray(next) ? next : [];
    const oldById = new Map(accounts.value.map(item => [accountKeyOf(item), item]));
    const incomingIds = new Set(incoming.map(accountKeyOf));
    const changedEntities: Array<{ id: string; fields: string[] }> = [];
    const ordered = incoming.map(item => {
      const existing = oldById.get(accountKeyOf(item));
      if (existing) {
        const fields = patchEntity(existing, item);
        if (fields.length) changedEntities.push({ id: accountKeyOf(item), fields });
        return existing;
      }
      return item;
    });
    const orderChanged = ordered.length !== accounts.value.length || ordered.some((item, index) => item !== accounts.value[index]);
    if (orderChanged) accounts.value.splice(0, accounts.value.length, ...ordered);
    if (!selectedAccountId.value || !incomingIds.has(selectedAccountId.value)) {
      const preferred = incoming.find(item => item.default) || incoming[0];
      selectedAccountId.value = accountKeyOf(preferred);
    }
    if (orderChanged || changedEntities.length) recordRuntimeDebug('store', 'launcher-session.accounts', orderChanged ? `accounts list patched (${ordered.length})` : 'account entities patched', { orderChanged, changedEntities });
  }

  function updateInstance(id: string, patch: Partial<LauncherInstance>) {
    const item = instances.value.find(value => keyOf(value) === String(id));
    if (item) Object.assign(item, patch);
  }

  async function refresh() {
    if (!isTauri()) return;
    if (refreshRequest) return refreshRequest;
    recordRuntimeDebug('refresh', 'launcher-session', 'refresh requested');
    refreshRequest = (async () => {
      loading.value = true;
      try {
        const [instanceResult, accountResult] = await Promise.all([
          launcher.listInstances(),
          launcher.execute<any[]>('account.list'),
        ]);
        if (!instanceResult.ok) throw new Error(instanceResult.error || 'instance.list');
        replaceInstances((instanceResult.data || []) as LauncherInstance[]);
        if (!accountResult.ok) throw new Error(accountResult.error || 'account.list');
        replaceAccounts((accountResult.data || []) as LauncherAccount[]);
        error.value = '';
        recordRuntimeDebug('refresh', 'launcher-session', 'refresh completed');
      } catch (cause) {
        error.value = String(cause);
        recordRuntimeDebug('error', 'launcher-session', 'refresh failed', String(cause));
      } finally {
        loading.value = false;
        refreshRequest = undefined;
      }
    })();
    return refreshRequest;
  }

  function invalidateAndRefresh() {
    launcher.invalidate('instance.list');
    launcher.invalidate('account.list');
    void refresh();
  }

  const sync = () => invalidateAndRefresh();
  async function startEventBridge() {
    if (eventsStarted || typeof window === 'undefined') return;
    eventsStarted = true;
    window.addEventListener('melody-instances-changed', sync);
    if (!isTauri()) return;
    try {
      stopSessionEvent = await listen('launcher-session-changed', sync);
      stopAccountSelectionEvent = await listen<{ accountId?: string }>('launcher-account-selected', event => {
        const next = String(event.payload?.accountId || '');
        if (next !== selectedAccountId.value) selectedAccountId.value = next;
      });
      stopLauncherEvent = await launcher.onEvent((raw: any) => {
        const operation = String(raw?.operation || raw?.data?.operation || '');
        const kind = String(raw?.kind || '');
        if (kind.includes('instance') || kind.includes('account') || operation.startsWith('instance.') || operation.startsWith('account.')) {
          recordRuntimeDebug('event', 'launcher-event', 'instance/account domain event received', { kind, operation });
          sync();
        }
      });
    } catch { /* Event bridge is optional for older native hosts. */ }
  }
  function dispose() {
    if (!eventsStarted || typeof window === 'undefined') return;
    window.removeEventListener('melody-instances-changed', sync);
    stopSessionEvent?.();
    stopAccountSelectionEvent?.();
    stopLauncherEvent?.();
    stopSessionEvent = undefined;
    stopAccountSelectionEvent = undefined;
    stopLauncherEvent = undefined;
    eventsStarted = false;
  }
  void startEventBridge();

  return { instances, accounts, icons, loading, error, selectedInstanceId, selectedAccountId, selected, selectedAccount, selectInstance, selectAccount, replaceInstances, replaceAccounts, updateInstance, refresh, invalidateAndRefresh, dispose };
});
