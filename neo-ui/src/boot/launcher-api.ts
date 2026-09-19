import { invoke, isTauri } from '@tauri-apps/api/core';
import { listen, type UnlistenFn } from '@tauri-apps/api/event';

export type ApiResult<T = unknown> = { ok: boolean; data?: T; error?: string; operation?: string };
export class LauncherApiClient {
  private requireDesktop(): void {
    if (!isTauri()) throw new Error('当前为网页预览。请启动安装目录中的 luna-neo-ui.exe，以连接启动器并读取实例、账户和设置。');
  }
  async execute<T = unknown>(operation: string, parameters: Record<string, unknown> = {}): Promise<ApiResult<T>> {
    this.requireDesktop();
    return invoke<ApiResult<T>>('launcher_request', { method: 'launcher/execute', parameters: { operation, parameters } });
  }
  respond(parameters: Record<string, unknown>): Promise<unknown> {
    return invoke('launcher_request', { method: 'launcher/respond', parameters });
  }
  onEvent(handler: (event: unknown) => void): Promise<UnlistenFn> {
    this.requireDesktop();
    return listen('launcher-event', event => handler(event.payload));
  }
  onStream(handler: (batch: unknown) => void): Promise<UnlistenFn> {
    this.requireDesktop();
    return listen('launcher-stream', event => handler(event.payload));
  }
  onExit(handler: () => void): Promise<UnlistenFn> {
    this.requireDesktop();
    return listen('launcher-exit', handler);
  }
}
export const launcher = new LauncherApiClient();
