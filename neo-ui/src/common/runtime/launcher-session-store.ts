import { computed, reactive, toRef } from 'vue';
import { defineStore } from 'pinia';
import { isTauri } from '@tauri-apps/api/core';
import { emit, listen } from '@tauri-apps/api/event';
import { launcher } from '../../boot/launcher-api';
import { neoUiPreferences, persistNeoUiPreferences } from '../../neo-ui-preferences';
import { recordRuntimeDebug } from './runtime-debug';
import {
  accountKey,
  createLunaSession,
  instanceKey,
  type LauncherSessionState,
} from '../../client/session';
import type { LauncherInstance } from '../../client/types';

/** Shared launcher data model used by every shell and every native window. */
export const useLauncherSessionStore = defineStore('launcher-session', () => {
  const state = reactive<LauncherSessionState>({
    instances: [],
    accounts: [],
    icons: {},
    selectedInstanceId: String(neoUiPreferences.selectedInstanceId || ''),
    selectedAccountId: '',
    loading: false,
    error: '',
    ready: false,
  });

  const session = createLunaSession(launcher, {
    state,
    enabled: () => isTauri(),
    startOnSubscribe: false,
    persistInstanceSelection: id => {
      neoUiPreferences.selectedInstanceId = id;
      persistNeoUiPreferences();
    },
    onAccountSelected: id => {
      // Native Melody preference/account windows are separate webviews. A
      // dedicated selection event keeps their runtime state in sync without
      // causing unrelated components to refresh or rebuild.
      if (isTauri()) void emit('launcher-account-selected', { accountId: id });
    },
    onDebug: (kind, source, message, data) => recordRuntimeDebug(kind as any, source, message, data),
  });

  const selected = computed<LauncherInstance | null>({
    get: () => state.instances.find(item => instanceKey(item) === state.selectedInstanceId) || null,
    set: value => session.selectInstance(value),
  });
  const selectedAccount = computed(() => state.accounts.find(item => accountKey(item) === state.selectedAccountId) || null);

  let eventsStarted = false;
  let stopSessionEvent: (() => void) | undefined;
  let stopAccountSelectionEvent: (() => void) | undefined;

  function sync() {
    launcher.invalidate('instance.list');
    launcher.invalidate('account.list');
    void session.refresh();
  }

  async function startEventBridge() {
    if (eventsStarted || typeof window === 'undefined') return;
    eventsStarted = true;
    window.addEventListener('melody-instances-changed', sync);
    if (!isTauri()) return;
    try {
      stopSessionEvent = await listen('launcher-session-changed', sync);
      stopAccountSelectionEvent = await listen<{ accountId?: string }>('launcher-account-selected', event => {
        session.setSelectedAccount(String(event.payload?.accountId || ''));
      });
      await session.start();
    } catch { /* Event bridge is optional for older native hosts. */ }
  }
  void startEventBridge();

  function dispose() {
    if (eventsStarted && typeof window !== 'undefined') window.removeEventListener('melody-instances-changed', sync);
    stopSessionEvent?.();
    stopAccountSelectionEvent?.();
    stopSessionEvent = undefined;
    stopAccountSelectionEvent = undefined;
    eventsStarted = false;
    session.dispose();
  }

  return {
    instances: toRef(state, 'instances'),
    accounts: toRef(state, 'accounts'),
    icons: toRef(state, 'icons'),
    loading: toRef(state, 'loading'),
    error: toRef(state, 'error'),
    selectedInstanceId: toRef(state, 'selectedInstanceId'),
    selectedAccountId: toRef(state, 'selectedAccountId'),
    selected,
    selectedAccount,
    selectInstance: session.selectInstance,
    selectAccount: session.selectAccount,
    replaceInstances: session.replaceInstances,
    replaceAccounts: session.replaceAccounts,
    updateInstance: session.updateInstance,
    refresh: session.refresh,
    refreshIcons: session.refreshIcons,
    invalidateAndRefresh: sync,
    dispose,
  };
});
