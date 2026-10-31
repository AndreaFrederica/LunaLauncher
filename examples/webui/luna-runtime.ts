// SPDX-License-Identifier: GPL-3.0-only
// Copy this adapter into a Vue + Pinia external Web UI. The native host owns
// the runtime model; this file only exposes it as the same store shape used by
// the bundled Neo UI.
import { computed, ref } from 'vue';
import { defineStore } from 'pinia';

type RuntimeSnapshot = NonNullable<Window['luna']>['runtime'] extends {
  getSnapshot(): infer Snapshot;
} ? Snapshot : never;

const emptySnapshot = (): RuntimeSnapshot => ({
  instances: [],
  accounts: [],
  icons: {},
  selectedInstanceId: '',
  selectedAccountId: '',
  selected: null,
  selectedAccount: null,
  loading: false,
  error: '',
  ready: false,
} as RuntimeSnapshot);

export const useLauncherSessionStore = defineStore('launcher-session', () => {
  const runtime = typeof window !== 'undefined' ? window.luna?.runtime : undefined;
  const snapshot = ref<RuntimeSnapshot>(runtime?.getSnapshot() || emptySnapshot());
  const started = ref(false);
  let unsubscribe: (() => void) | undefined;

  const instances = computed(() => snapshot.value.instances);
  const accounts = computed(() => snapshot.value.accounts);
  const icons = computed(() => snapshot.value.icons);
  const selectedInstanceId = computed(() => snapshot.value.selectedInstanceId);
  const selectedAccountId = computed(() => snapshot.value.selectedAccountId);
  const selected = computed(() => snapshot.value.selected);
  const selectedAccount = computed(() => snapshot.value.selectedAccount);
  const loading = computed(() => snapshot.value.loading);
  const error = computed(() => snapshot.value.error);

  async function start() {
    if (!runtime) throw new Error('请在 Luna Neo UI 中打开此界面。');
    if (!unsubscribe) unsubscribe = runtime.subscribe(value => { snapshot.value = value; });
    await runtime.start();
    started.value = true;
  }
  function selectInstance(value: string | Record<string, unknown> | null) { runtime?.selectInstance(value); }
  function selectAccount(value: string | Record<string, unknown> | null) { runtime?.selectAccount(value); }
  async function refresh() { if (!runtime) return; await runtime.refresh(); }
  async function refreshIcons() { if (!runtime) return; await runtime.refreshIcons(); }
  function dispose() { unsubscribe?.(); unsubscribe = undefined; runtime?.dispose(); started.value = false; }

  return {
    instances, accounts, icons, selectedInstanceId, selectedAccountId, selected,
    selectedAccount, loading, error, started, start, refresh, refreshIcons,
    selectInstance, selectAccount, dispose,
  };
});
