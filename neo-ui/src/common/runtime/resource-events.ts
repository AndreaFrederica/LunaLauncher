// SPDX-License-Identifier: MPL-2.0
import { emit as emitTauri, listen as listenTauri, type UnlistenFn } from '@tauri-apps/api/event';

const RESOURCE_CHANGED_EVENT = 'luna://resource-changed';
const RESOURCE_CHANGED_STORAGE_KEY = 'luna-resource-changed';

export type ResourceChangedPayload = {
  instanceId?: string;
  kind?: string;
  source?: string;
  at?: number;
};

/**
 * Cross-window notification used after an install task finishes. The backend
 * filesystem watcher is useful but can miss rapid writes on Windows, so the
 * downloading window also broadcasts an explicit signal to open editors.
 */
export async function notifyResourceChanged(payload: ResourceChangedPayload = {}) {
  const detail = { ...payload, at: Date.now() };
  try {
    await emitTauri(RESOURCE_CHANGED_EVENT, detail);
  } catch {
    // Browser preview and non-Tauri hosts do not expose the event bridge.
  }
  try {
    localStorage.setItem(RESOURCE_CHANGED_STORAGE_KEY, JSON.stringify(detail));
  } catch {
    // Storage is optional; the DOM event below still covers this window.
  }
  window.dispatchEvent(new CustomEvent(RESOURCE_CHANGED_EVENT, { detail }));
}

export function onResourceChanged(callback: (payload: ResourceChangedPayload) => void): () => void {
  let disposed = false;
  let unlistenTauri: UnlistenFn | undefined;

  const handleDetail = (detail: ResourceChangedPayload | undefined) => {
    if (!disposed) callback(detail || {});
  };
  const handleDomEvent = (event: Event) => {
    handleDetail((event as CustomEvent<ResourceChangedPayload>).detail);
  };
  const handleStorage = (event: StorageEvent) => {
    if (event.key !== RESOURCE_CHANGED_STORAGE_KEY || !event.newValue) return;
    try {
      handleDetail(JSON.parse(event.newValue));
    } catch {
      handleDetail({});
    }
  };

  window.addEventListener(RESOURCE_CHANGED_EVENT, handleDomEvent);
  window.addEventListener('storage', handleStorage);
  void listenTauri<ResourceChangedPayload>(RESOURCE_CHANGED_EVENT, event => {
    handleDetail(event.payload);
  }).then(unlisten => {
    if (disposed) unlisten();
    else unlistenTauri = unlisten;
  }).catch(() => {
    // The listener is an enhancement for desktop windows.
  });

  return () => {
    disposed = true;
    window.removeEventListener(RESOURCE_CHANGED_EVENT, handleDomEvent);
    window.removeEventListener('storage', handleStorage);
    unlistenTauri?.();
  };
}
