import { launcher } from '../../../boot/launcher-api';
import { emit } from '@tauri-apps/api/event';
import { isTauri } from '@tauri-apps/api/core';
import { storeToRefs } from 'pinia';
import { useLauncherSessionStore } from '../../../common/launcher-session';
export async function melodyExecute(operation: string, parameters: Record<string, unknown> = {}): Promise<any> {
  const result = await launcher.execute(operation, parameters);
  if (!result.ok) throw new Error(result.error || operation);
  return result.data;
}
export function notifyMelodyInstancesChanged() {
  window.dispatchEvent(new Event('melody-instances-changed'));
  if (isTauri()) void emit('launcher-session-changed');
}
export function useMelodyInstances() {
  const store = useLauncherSessionStore();
  const { instances, loading, error } = storeToRefs(store);
  return { instances, loading, error, refresh: store.refresh };
}
