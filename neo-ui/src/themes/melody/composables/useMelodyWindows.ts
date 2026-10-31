import { nextTick, onUnmounted, ref } from 'vue';
import { invoke, isTauri } from '@tauri-apps/api/core';
import { getCurrentWindow } from '@tauri-apps/api/window';
import { windows, type MelodyWindowEntry, type MelodyWindowKind } from '../types';
import { messageWidth } from '../services/message-layout';

export function useMelodyWindows() {
  const native = isTauri();
  const query = new URLSearchParams(location.search);
  const requested = query.get('melodyWindow') || 'launcher';
  const initialKind = Object.hasOwn(windows, requested) ? requested as MelodyWindowKind : 'launcher';
  const entries = ref<MelodyWindowEntry[]>([]);
  const returnFocus = new Map<string, HTMLElement>();
  let layer = 0;
  let disposed = false;
  onUnmounted(() => { disposed = true; });
  function position(kind: MelodyWindowKind, width = windows[kind].width) {
    return native ? { x: 0, y: 0 } : {
      x: Math.max(12, (Math.max(1084, window.innerWidth) - width) / 2),
      y: Math.max(12, (Math.max(700, window.innerHeight) - windows[kind].height) / 2),
    };
  }
  function activate(id: string) {
    const child = entries.value.find(entry => entry.parent === id && entry.kind === 'message');
    const entry = child || entries.value.find(entry => entry.id === id);
    if (entry) entry.z = ++layer;
  }
  async function open(kind: MelodyWindowKind, parent?: string, message?: string, title?: string, instanceId?: string, componentUid?: string) {
    const width = kind === 'message' ? messageWidth(message || '', title) : windows[kind].width;
    if (native) {
      await invoke('open_melody_window', { kind, message: message || '', title: title || '', messageWidth: width, instanceId, componentUid });
      return;
    }
    const id = kind === 'message' ? `message-${parent || 'launcher'}` : kind === 'client' || (kind === 'games' && (instanceId || componentUid)) ? `${kind}-${encodeURIComponent(JSON.stringify([instanceId || '', componentUid || '']))}` : kind;
    const existing = entries.value.find(entry => entry.id === id);
    if (existing) {
      if (kind === 'message') {
        existing.message = message; existing.title = title; existing.width = width;
        existing.x = Math.min(existing.x, Math.max(1084, window.innerWidth) - width);
      }
      activate(id);
      await nextTick();
      document.querySelector<HTMLElement>(`[data-window="${CSS.escape(id)}"]`)?.focus();
      return;
    }
    if (document.activeElement instanceof HTMLElement) returnFocus.set(id, document.activeElement);
    entries.value.push({ id, kind, parent: kind === 'message' ? parent : undefined, ...position(kind, width), width, z: ++layer, message, title, instanceId, componentUid });
  }
  async function close(id: string) {
    if (id === 'message-error') { entries.value = entries.value.filter(entry => entry.id !== id); return; }
    if (native) { await getCurrentWindow().close(); return; }
    const childIds = entries.value.filter(entry => entry.parent === id).map(entry => entry.id);
    entries.value = entries.value.filter(entry => entry.id !== id && entry.parent !== id);
    for (const child of childIds) returnFocus.delete(child);
    const target = returnFocus.get(id);
    returnFocus.delete(id);
    await nextTick();
    if (target?.isConnected) target.focus({ preventScroll: true });
  }
  function move(entry: MelodyWindowEntry, x: number, y: number) {
    entry.x = Math.max(0, Math.min(Math.max(1084, window.innerWidth) - (entry.width ?? windows[entry.kind].width), x));
    entry.y = Math.max(0, Math.min(Math.max(700, window.innerHeight) - 38, y));
  }
  function blocked(id: string) { return entries.value.some(entry => entry.parent === id && entry.kind === 'message'); }
  const initialMessage = query.get('message') || '';
  const initialTitle = query.get('title') || undefined;
  const initialWidth = initialKind === 'message' ? messageWidth(initialMessage, initialTitle) : windows[initialKind].width;
  entries.value.push({ id: initialKind, kind: initialKind, ...position(initialKind, initialWidth), width: initialWidth, z: ++layer, message: initialMessage, title: initialTitle, instanceId: query.get('instance') || undefined, componentUid: query.get('component') || undefined });
  // Native message windows receive new content when their owner reuses them.
  if (native && initialKind === 'message') {
    void import('@tauri-apps/api/event').then(async ({ listen }) => {
      const unlisten = await listen<{ message: string; title: string }>('melody-message', event => {
        const entry = entries.value[0];
        if (entry) { entry.message = event.payload.message; entry.title = event.payload.title; entry.width = messageWidth(event.payload.message, event.payload.title); }
      });
      if (disposed) unlisten(); else cleanup = unlisten;
    });
  }
  let cleanup: (() => void) | undefined;
  onUnmounted(() => cleanup?.());
  return { native, entries, initialKind, open, close, activate, move, blocked };
}
