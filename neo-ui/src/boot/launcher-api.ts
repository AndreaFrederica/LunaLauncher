export type ApiResult<T = unknown> = { ok: boolean; data?: T; error?: string; operation?: string };
export class LauncherApiClient {
  async execute<T = unknown>(operation: string, parameters: Record<string, unknown> = {}): Promise<ApiResult<T>> {
    const bridge = (globalThis as any).__LUNA_LAUNCHER__;
    if (!bridge?.execute) throw new Error('Launcher sidecar bridge is unavailable');
    return bridge.execute(operation, parameters) as Promise<ApiResult<T>>;
  }
}
export const launcher = new LauncherApiClient();
