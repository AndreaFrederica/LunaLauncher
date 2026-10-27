import { isTauri } from '@tauri-apps/api/core';
import { open, save } from '@tauri-apps/plugin-dialog';
import { i18n } from '../../i18n';

export interface PclFileFilter {
  name: string;
  extensions: string[];
}

async function showNativePicker(kind: 'file' | 'directory' | 'save', title?: string, defaultPath?: string, filters?: PclFileFilter[], multiple = false): Promise<string> {
  if (!isTauri()) {
    return window.prompt(title || (kind === 'directory' ? i18n.global.t('pcl.fileDialog.directoryPath') : kind === 'save' ? i18n.global.t('pcl.fileDialog.savePath') : i18n.global.t('pcl.fileDialog.filePath')), defaultPath || '') || '';
  }
  try {
    const dialogFilters = filters?.filter(filter => filter.extensions.length).map(filter => ({ name: filter.name, extensions: filter.extensions }));
    if (kind === 'save') return (await save({ title, defaultPath, filters: dialogFilters })) || '';
    const selected = await open({ title, defaultPath, directory: kind === 'directory', multiple, filters: dialogFilters });
    return Array.isArray(selected) ? (selected[0] || '') : (selected || '');
  } catch {
    return '';
  }
}

export async function pickPclFile(options: {
  title?: string;
  filters?: PclFileFilter[];
  multiple?: boolean;
} = {}): Promise<string> {
  return showNativePicker('file', options.title, undefined, options.filters, options.multiple);
}

export async function pickPclDirectory(options: { title?: string } = {}): Promise<string> {
  return showNativePicker('directory', options.title);
}

export async function pickPclSavePath(options: {
  title?: string;
  defaultPath?: string;
  filters?: PclFileFilter[];
} = {}): Promise<string> {
  return showNativePicker('save', options.title, options.defaultPath, options.filters);
}
