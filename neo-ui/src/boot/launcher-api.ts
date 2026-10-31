// SPDX-License-Identifier: MPL-2.0
import { invoke, isTauri } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';
import { revealItemInDir } from '@tauri-apps/plugin-opener';
import { i18n } from '../i18n';
import { createLunaClient } from '../client/core';
import type {
  ApiResult,
  HostTransport,
  LauncherInstance,
  LauncherProfile,
  LauncherSettingsEntry,
} from '../client/types';

export type { ApiResult, LauncherInstance, LauncherProfile, LauncherSettingsEntry } from '../client/types';

const transport: HostTransport = {
  invoke: (command, args) => invoke(command, args),
  listen: <T>(event: string, handler: (payload: T) => void) => listen<T>(event, payload => handler(payload.payload)),
};

const client = createLunaClient({
  transport,
  isDesktop: () => isTauri(),
  translate: key => i18n.global.t(key),
  onSlow: (operation, elapsed) => console.warn(`[launcher-api] slow ${operation}: ${Math.round(elapsed)} ms`),
});

function requireDesktop(): void {
  if (!isTauri()) throw new Error(i18n.global.t('neo.extra.webPreview'));
}

export const launcher = {
  ...client,

  async openPath(path: string): Promise<void> {
    requireDesktop();
    await revealItemInDir(path);
  },
  async openWebUiManager(): Promise<void> {
    requireDesktop();
    await invoke('webui_manager');
  },
  async openInstanceEditor(id: string, name = ''): Promise<void> {
    requireDesktop();
    await invoke('open_instance_editor', { id, name });
  },
  async openResourceDownload(instance: string, kind: string, name = ''): Promise<void> {
    requireDesktop();
    await invoke('open_resource_download', { instance, kind, name });
  },
  async getInstance(reference: string): Promise<ApiResult<LauncherInstance>> {
    return client.execute<LauncherInstance>('instance.get', { instance: reference });
  },
  async listInstances(): Promise<ApiResult<LauncherInstance[]>> {
    return client.execute<LauncherInstance[]>('instance.list');
  },
  async getLauncher(): Promise<ApiResult<LauncherProfile>> {
    return client.execute<LauncherProfile>('launcher.get');
  },
};

if (typeof window !== 'undefined') {
  (window as any).__lunaApiStats = () => launcher.getStats();
}
