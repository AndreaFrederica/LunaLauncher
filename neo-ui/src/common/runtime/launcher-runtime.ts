import { useLauncherSessionStore as useStore } from './launcher-session-store';
import type { LauncherInstance } from '../../boot/launcher-api';

export type LauncherSessionInstance = LauncherInstance;
export type LauncherSessionAccount = Record<string, any>;

/**
 * Runtime boundary shared by PCL, XMCL and Melody.
 *
 * Keeping this adapter deliberately small prevents themes from creating a
 * second source of truth. It is also the place for runtime-only coordination
 * such as refresh coalescing and cross-window event delivery.
 */
export function useLauncherSessionStore() {
  return useStore();
}
