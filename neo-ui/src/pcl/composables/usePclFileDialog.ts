import { invoke } from '@tauri-apps/api/core';

export interface PclFileFilter {
  name: string;
  extensions: string[];
}

function isDesktop(): boolean {
  return Boolean((window as any).__TAURI__);
}

async function showNativePicker(kind: 'file' | 'directory' | 'save', title?: string, defaultPath?: string): Promise<string> {
  if (!isDesktop()) {
    return window.prompt(title || (kind === 'directory' ? '输入目录完整路径' : kind === 'save' ? '输入保存路径' : '输入文件完整路径'), defaultPath || '') || '';
  }
  try {
    return await invoke<string>('pick_path_dialog', { kind, title: title || null, default_path: defaultPath || null });
  } catch {
    return '';
  }
}

export async function pickPclFile(options: {
  title?: string;
  filters?: PclFileFilter[];
  multiple?: boolean;
} = {}): Promise<string> {
  return showNativePicker('file', options.title);
}

export async function pickPclDirectory(options: { title?: string } = {}): Promise<string> {
  return showNativePicker('directory', options.title);
}

export async function pickPclSavePath(options: {
  title?: string;
  defaultPath?: string;
  filters?: PclFileFilter[];
} = {}): Promise<string> {
  return showNativePicker('save', options.title, options.defaultPath);
}
