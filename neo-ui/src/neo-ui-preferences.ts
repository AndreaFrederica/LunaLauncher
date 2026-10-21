import { computed, reactive, watch } from 'vue';

export type NeoTheme = 'dark' | 'light' | 'system';
export type NeoDensity = 'comfortable' | 'compact';
export type NeoBackground = 'solid' | 'gradient';

// SPDX-License-Identifier: MPL-2.0
export interface NeoUiPreferences {
  theme: NeoTheme;
  accent: string;
  density: NeoDensity;
  background: NeoBackground;
  animations: boolean;
}

const storageKey = 'luna-neo-ui.preferences';
const defaults: NeoUiPreferences = {
  theme: 'dark',
  accent: '#818cf8',
  density: 'comfortable',
  background: 'gradient',
  animations: true,
};

function load(): NeoUiPreferences {
  if (typeof localStorage === 'undefined') return { ...defaults };
  try {
    const value = JSON.parse(localStorage.getItem(storageKey) || '{}');
    return { ...defaults, ...value };
  } catch {
    return { ...defaults };
  }
}

export const neoUiPreferences = reactive<NeoUiPreferences>(load());
export const neoUiIsDark = computed(() => {
  if (neoUiPreferences.theme === 'dark') return true;
  if (neoUiPreferences.theme === 'light') return false;
  return typeof window === 'undefined' || window.matchMedia('(prefers-color-scheme: dark)').matches;
});

export function applyNeoUiPreferences() {
  if (typeof document === 'undefined') return;
  const root = document.documentElement;
  root.dataset.neoTheme = neoUiIsDark.value ? 'dark' : 'light';
  root.dataset.neoDensity = neoUiPreferences.density;
  root.dataset.neoBackground = neoUiPreferences.background;
  root.dataset.neoAnimations = neoUiPreferences.animations ? 'on' : 'off';
  root.style.setProperty('--neo-primary', neoUiPreferences.accent);
}

watch(neoUiPreferences, () => {
  if (typeof localStorage !== 'undefined') localStorage.setItem(storageKey, JSON.stringify(neoUiPreferences));
  applyNeoUiPreferences();
}, { deep: true, immediate: true });

export function resetNeoUiPreferences() {
  Object.assign(neoUiPreferences, defaults);
}
