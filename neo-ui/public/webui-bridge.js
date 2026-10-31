// SPDX-License-Identifier: MPL-2.0
// Injected by the native host into every local Web UI. No frontend dependency.
(() => {
  let tail = Promise.resolve();
  const control = new Set(['task.list', 'task.status', 'task.cancel', 'api.describe', 'event.poll',
    'event.subscriptions', 'event.unsubscribe', 'server.console.command', 'server.console.write', 'server.console.resize']);
  const invoke = (command, parameters) => window.__TAURI__.core.invoke(command, parameters);
  function identity(value) {
    return String(value && value.id || '');
  }
  function accountIdentity(value) {
    return String(value && (value.internalId || value.id || value.profileName) || '');
  }
  function createRuntime(host) {
    const state = {
      instances: [],
      accounts: [],
      icons: {},
      selectedInstanceId: '',
      selectedAccountId: '',
      loading: false,
      error: '',
      ready: false,
    };
    const listeners = new Set();
    let refreshPromise;
    let startPromise;
    let stopEvents;

    function snapshot() {
      return {
        instances: state.instances.slice(),
        accounts: state.accounts.slice(),
        icons: { ...state.icons },
        selectedInstanceId: state.selectedInstanceId,
        selectedAccountId: state.selectedAccountId,
        selected: state.instances.find(item => identity(item) === state.selectedInstanceId) || null,
        selectedAccount: state.accounts.find(item => accountIdentity(item) === state.selectedAccountId) || null,
        loading: state.loading,
        error: state.error,
        ready: state.ready,
      };
    }
    function notify() {
      const value = snapshot();
      for (const listener of listeners) {
        try { listener(value); } catch (error) { console.error('[luna.runtime] subscriber failed', error); }
      }
    }
    function replaceCollection(target, incoming, key) {
      const old = new Map(target.map(item => [key(item), item]));
      const next = (Array.isArray(incoming) ? incoming : []).map(item => {
        const existing = old.get(key(item));
        if (existing) { Object.assign(existing, item); return existing; }
        return item;
      });
      target.splice(0, target.length, ...next);
    }
    function keepSelection() {
      if (!state.instances.some(item => identity(item) === state.selectedInstanceId)) {
        state.selectedInstanceId = state.instances[0] ? identity(state.instances[0]) : '';
      }
      if (!state.accounts.some(item => accountIdentity(item) === state.selectedAccountId)) {
        const preferred = state.accounts.find(item => item && item.default) || state.accounts[0];
        state.selectedAccountId = preferred ? accountIdentity(preferred) : '';
      }
    }
    async function refresh() {
      if (refreshPromise) return refreshPromise;
      refreshPromise = (async () => {
        state.loading = true;
        notify();
        try {
          const [instanceResult, accountResult] = await Promise.all([
            host.execute('instance.list'),
            host.execute('account.list'),
          ]);
          if (!instanceResult.ok) throw new Error(instanceResult.error || 'instance.list');
          if (!accountResult.ok) throw new Error(accountResult.error || 'account.list');
          replaceCollection(state.instances, instanceResult.data, identity);
          replaceCollection(state.accounts, accountResult.data, accountIdentity);
          keepSelection();
          state.error = '';
          state.ready = true;
        } catch (error) {
          state.error = error && error.message ? error.message : String(error);
        } finally {
          state.loading = false;
          refreshPromise = undefined;
          notify();
        }
      })();
      return refreshPromise;
    }
    async function refreshIcons() {
      const result = await host.execute('instance.icons');
      if (!result.ok) throw new Error(result.error || 'instance.icons');
      for (const icon of Array.isArray(result.data && result.data.icons) ? result.data.icons : []) {
        if (icon && icon.id && icon.dataUrl) state.icons[icon.id] = icon.dataUrl;
      }
      notify();
      return snapshot().icons;
    }
    async function start() {
      if (startPromise) return startPromise;
      startPromise = (async () => {
        try {
          stopEvents = await host.onEvent(event => {
            const operation = String(event && (event.operation || event.data && event.data.operation) || '');
            const kind = String(event && event.kind || '');
            if (kind.includes('instance') || kind.includes('account') || operation.startsWith('instance.') || operation.startsWith('account.')) {
              void refresh();
            }
          });
        } catch { /* Older hosts may not expose event subscriptions. */ }
        await refresh();
      })();
      try { await startPromise; } finally { startPromise = undefined; }
    }
    function subscribe(listener) {
      listeners.add(listener);
      listener(snapshot());
      void start().catch(() => {});
      return () => listeners.delete(listener);
    }
    function selectInstance(value) {
      const id = typeof value === 'string' ? value : identity(value);
      state.selectedInstanceId = id;
      notify();
    }
    function selectAccount(value) {
      const id = typeof value === 'string' ? value : accountIdentity(value);
      state.selectedAccountId = id;
      notify();
    }
    function updateInstance(id, patch) {
      const item = state.instances.find(value => identity(value) === String(id));
      if (item) { Object.assign(item, patch || {}); notify(); }
    }
    function dispose() {
      if (stopEvents) { void stopEvents(); stopEvents = undefined; }
      listeners.clear();
    }
    return Object.freeze({ getSnapshot: snapshot, subscribe, start, refresh, refreshIcons, selectInstance, selectAccount, updateInstance, dispose });
  }

  const bridge = {
    apiVersion: 1,
    execute(operation, parameters = {}) {
      const request = () => invoke('launcher_request', { method: 'launcher/execute', parameters: { operation, parameters } });
      if (control.has(operation)) return request();
      const result = tail.then(request);
      tail = result.then(() => {}, () => {});
      return result;
    },
    catalog: () => invoke('launcher_request', { method: 'launcher/catalog', parameters: {} }),
    respond: parameters => invoke('launcher_request', { method: 'launcher/respond', parameters }),
    onEvent: handler => window.__TAURI__.event.listen('launcher-event', event => handler(event.payload)),
    onStream: handler => window.__TAURI__.event.listen('launcher-stream', event => handler(event.payload)),
    onExit: handler => window.__TAURI__.event.listen('launcher-exit', handler),
    openPath: path => invoke('plugin:opener|reveal_item_in_dir', { paths: [path] }),
    webUis: () => invoke('webui_list'),
    webUiSettings: () => invoke('webui_settings_get'),
    updateWebUiSettings: patch => invoke('webui_settings_update', { patch }),
    switchWebUi: id => invoke('webui_select', { id }),
    openWebUiManager: () => invoke('webui_manager'),
  };
  bridge.runtime = createRuntime(bridge);
  window.luna = Object.freeze(bridge);
  // Independent of the external app/router. The native menu accelerator also
  // opens this window when the page's own JavaScript fails to initialize.
  document.addEventListener('keydown', event => {
    if ((event.ctrlKey || event.metaKey) && event.shiftKey && event.code === 'KeyU') {
      event.preventDefault();
      void invoke('webui_manager');
    }
  }, true);
})();
