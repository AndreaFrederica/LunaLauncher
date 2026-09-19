export type ApiResult<T = unknown> = { ok: boolean; data?: T; error?: string; operation?: string };
export class LauncherApiClient {
  async execute<T = unknown>(operation: string, parameters: Record<string, unknown> = {}): Promise<ApiResult<T>> {
    return invoke<ApiResult<T>>('launcher_execute', { operation, parameters });
  }
}
export const launcher = new LauncherApiClient();
import { invoke } from '@tauri-apps/api/core';
