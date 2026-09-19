import { invoke } from '@tauri-apps/api/core';
import { listen, type UnlistenFn } from '@tauri-apps/api/event';

export type ApiResult<T = unknown> = { ok: boolean; data?: T; error?: string; operation?: string };
export class LauncherApiClient {
  async execute<T = unknown>(operation: string, parameters: Record<string, unknown> = {}): Promise<ApiResult<T>> {
    return invoke<ApiResult<T>>('launcher_execute', { operation, parameters });
  }
  onEvent(handler: (event: unknown) => void): Promise<UnlistenFn> {
    return listen('launcher-event', event => handler(event.payload));
  }
}
export const launcher = new LauncherApiClient();
