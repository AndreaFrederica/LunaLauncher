import { reactive } from 'vue';
import { defineStore } from 'pinia';
import { emit, listen, type UnlistenFn } from '@tauri-apps/api/event';
import { isTauri } from '@tauri-apps/api/core';

export type RuntimeDebugKind = 'event' | 'store' | 'refresh' | 'lifecycle' | 'timer' | 'error';

export interface RuntimeDebugEntry {
  id: number;
  at: number;
  kind: RuntimeDebugKind;
  source: string;
  message: string;
  data?: unknown;
}

const maxEntries = 1000;
let nextId = 1;
export const runtimeDebugState = reactive({ entries: [] as RuntimeDebugEntry[], paused: false, enabled: true });

function append(entry: Omit<RuntimeDebugEntry, 'id' | 'at'> & Partial<Pick<RuntimeDebugEntry, 'at' | 'id'>>) {
  if (!runtimeDebugState.enabled) return;
  const value: RuntimeDebugEntry = { id: entry.id || nextId++, at: entry.at || Date.now(), kind: entry.kind, source: entry.source, message: entry.message, data: entry.data };
  if (!runtimeDebugState.paused) {
    runtimeDebugState.entries.push(value);
    if (runtimeDebugState.entries.length > maxEntries) runtimeDebugState.entries.splice(0, runtimeDebugState.entries.length - maxEntries);
  }
  if (typeof window !== 'undefined') window.dispatchEvent(new CustomEvent<RuntimeDebugEntry>('neo-runtime-debug', { detail: value }));
  if (isTauri()) void emit('neo-runtime-debug', value).catch(() => {});
}

export function recordRuntimeDebug(kind: RuntimeDebugKind, source: string, message: string, data?: unknown) {
  append({ kind, source, message, data });
}

export function clearRuntimeDebug() { runtimeDebugState.entries.splice(0); }

/**
 * A single Pinia definition shared by every component in this webview.
 * Defining the store inside the composable creates a new definition on every
 * call and makes devtools/debug consumers observe different store instances.
 */
export const useRuntimeDebugStore = defineStore('runtime-debug', () => runtimeDebugState);

export async function listenRuntimeDebug(handler: (entry: RuntimeDebugEntry) => void): Promise<UnlistenFn | (() => void)> {
  const local = (event: Event) => handler((event as CustomEvent<RuntimeDebugEntry>).detail);
  if (typeof window !== 'undefined') window.addEventListener('neo-runtime-debug', local);
  let unlisten: UnlistenFn | undefined;
  if (isTauri()) {
    try { unlisten = await listen<RuntimeDebugEntry>('neo-runtime-debug', event => handler(event.payload)); } catch { /* Browser preview has no Tauri event bridge. */ }
  }
  return () => { if (typeof window !== 'undefined') window.removeEventListener('neo-runtime-debug', local); unlisten?.(); };
}
