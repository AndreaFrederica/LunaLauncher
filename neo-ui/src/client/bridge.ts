// SPDX-License-Identifier: MPL-2.0
import { createLunaClient } from './core';
import { createLunaSession } from './session';
import type { HostTransport } from './types';

interface TauriEvent<T> {
  payload: T;
}

interface TauriGlobal {
  core: {
    invoke<T>(command: string, args?: Record<string, unknown>): Promise<T>;
  };
  event: {
    listen<T>(event: string, handler: (event: TauriEvent<T>) => void): Promise<() => void>;
  };
}

declare global {
  interface Window {
    __TAURI__?: TauriGlobal;
    luna?: unknown;
  }
}

const host = typeof window !== 'undefined' ? window.__TAURI__ : undefined;

if (!host) {
  console.error('[luna] Native host bridge is unavailable. Open this UI inside Luna Neo UI.');
} else {
  const transport: HostTransport = {
    invoke: (command, args) => host.core.invoke(command, args),
    listen: <T>(event: string, handler: (payload: T) => void) => host.event.listen<T>(event, payload => handler(payload.payload)),
  };

  const client = createLunaClient({
    transport,
    onSlow: (operation, elapsed) => console.warn(`[launcher-api] slow ${operation}: ${Math.round(elapsed)} ms`),
  });
  const session = createLunaSession(client, { startOnSubscribe: true });

  const runtime = Object.freeze({
    getSnapshot: session.getSnapshot,
    subscribe: session.subscribe,
    start: async () => { await session.start(); await session.refresh(); },
    refresh: session.refresh,
    refreshIcons: session.refreshIcons,
    selectInstance: session.selectInstance,
    selectAccount: session.selectAccount,
    updateInstance: session.updateInstance,
    dispose: session.dispose,
  });

  const invokeHost = <T = unknown>(command: string, args?: Record<string, unknown>) => host.core.invoke<T>(command, args);

  const bridge = {
    apiVersion: 1,
    execute: client.execute,
    catalog: () => client.request('launcher/catalog', {}),
    respond: client.respond,
    onEvent: client.onEvent,
    onStream: client.onStream,
    onExit: client.onExit,
    openPath: (path: string) => invokeHost('plugin:opener|reveal_item_in_dir', { paths: [path] }),
    webUis: () => invokeHost('webui_list'),
    webUiSettings: () => invokeHost('webui_settings_get'),
    updateWebUiSettings: (patch: Record<string, unknown>) => invokeHost('webui_settings_update', { patch }),
    switchWebUi: (id: string) => invokeHost('webui_select', { id }),
    openWebUiManager: () => invokeHost('webui_manager'),
    runtime,
  };

  window.luna = Object.freeze(bridge);

  // Independent of the external app/router. The native menu accelerator also
  // opens this window when the page's own JavaScript fails to initialize.
  document.addEventListener('keydown', (event: KeyboardEvent) => {
    if ((event.ctrlKey || event.metaKey) && event.shiftKey && event.code === 'KeyU') {
      event.preventDefault();
      void bridge.openWebUiManager();
    }
  }, true);
}
