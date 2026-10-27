import { reactive, watch } from 'vue';
import { i18n } from '../../i18n';

const storageKey = 'luna-pcl-hidden-features';

export interface PclFeature {
  id: string;
  label: string;
  description?: string;
}

const feature = (id: string): PclFeature => ({ id, label: i18n.global.t(`pcl.features.${id}.label`), description: i18n.global.t(`pcl.features.${id}.description`) });
export const pclSetupFeatures: PclFeature[] = ['java', 'advanced', 'game', 'network', 'language', 'misc', 'download-settings', 'paths', 'console', 'advanced-settings', 'about', 'update', 'feedback', 'logs'].map(feature);

export const pclDownloadFeatures: PclFeature[] = ['mods', 'modpacks', 'datapacks', 'resourcepacks', 'shaders', 'worlds', 'favorites', 'forge', 'neoforge', 'cleanroom', 'fabric', 'liteloader', 'legacyfabric', 'labymod-production', 'labymod-snapshot'].map(feature);

function load(): Record<string, boolean> {
  if (typeof localStorage === 'undefined') return {};
  try {
    const value = JSON.parse(localStorage.getItem(storageKey) || '{}');
    return value && typeof value === 'object' ? value : {};
  } catch {
    return {};
  }
}

export const hiddenPclFeatures = reactive<Record<string, boolean>>(load());

watch(hiddenPclFeatures, value => {
  if (typeof localStorage === 'undefined') return;
  try {
    localStorage.setItem(storageKey, JSON.stringify(value));
  } catch {
    // Storage may be unavailable in a restricted webview.
  }
}, { deep: true });

export function isPclFeatureHidden(id: string): boolean {
  return Boolean(hiddenPclFeatures[id]);
}

export function setPclFeatureHidden(id: string, hidden: boolean) {
  if (hidden) hiddenPclFeatures[id] = true;
  else delete hiddenPclFeatures[id];
}

export function resetPclFeatures() {
  for (const key of Object.keys(hiddenPclFeatures)) delete hiddenPclFeatures[key];
}
