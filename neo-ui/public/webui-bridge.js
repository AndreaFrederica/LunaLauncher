// SPDX-License-Identifier: GPL-3.0-only
// Injected by the native host into every local Web UI. No frontend dependency.
(() => {
  let tail = Promise.resolve();
  const control = new Set(['task.list', 'task.status', 'task.cancel', 'api.describe', 'event.poll',
    'event.subscriptions', 'event.unsubscribe', 'server.console.command', 'server.console.write', 'server.console.resize']);
  const invoke = (command, parameters) => window.__TAURI__.core.invoke(command, parameters);
  window.luna = Object.freeze({
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
  });
  // Independent of the external app/router. The native menu accelerator also
  // opens this window when the page's own JavaScript fails to initialize.
  document.addEventListener('keydown', event => {
    if ((event.ctrlKey || event.metaKey) && event.shiftKey && event.code === 'KeyU') {
      event.preventDefault();
      void invoke('webui_manager');
    }
  }, true);
})();
