import { computed, reactive, watch } from 'vue';
import { defineStore } from 'pinia';
import { emit, listen, type UnlistenFn } from '@tauri-apps/api/event';
import { invoke, isTauri } from '@tauri-apps/api/core';
import { i18n } from './i18n';
import { recordRuntimeDebug } from './common/runtime/runtime-debug';

export type NeoTheme = 'dark' | 'light' | 'system';
export type NeoDensity = 'comfortable' | 'compact';
export type NeoBackground = 'solid' | 'gradient';
export type NeoShell = 'neo' | 'pcl' | 'melody';
export type PclFont = 'system' | 'yahei' | 'dengxian' | 'simsun';
export type PclStylePreset = 'luna-neo' | 'pcl';
export type PclLoadingAnimation = 'luna-spinner' | 'pcl-pickaxe' | 'pcl-bar';

export interface PclHomeCard {
  id: string;
  title: string;
  content: string;
}

// SPDX-License-Identifier: MPL-2.0
export interface NeoUiPreferences {
  shell: NeoShell;
  theme: NeoTheme;
  accent: string;
  density: NeoDensity;
  background: NeoBackground;
  animations: boolean;
  /** Font stack used by the PCL-inspired shell only. */
  pclFont: PclFont;
  /** Selected sub-preset inside the PCL Style theme. */
  pclStylePreset: PclStylePreset;
  /** Base font size in px for the PCL shell. */
  pclBaseFontSize: number;
  /** Loading indicator used by PCL Style pages and dialogs. */
  pclLoadingAnimation: PclLoadingAnimation;
  /** Refresh remote resource catalogs when their page is opened. */
  pclAutoRefreshOnPageOpen: boolean;
  /** PCL launch-page navigation layout. */
  /** Shared width of PCL page navigation columns, in CSS pixels. */
  pclPageNavWidth: number;
  pclNavLayout: 'pcl' | 'mmc-classic';
  pclClassicSide: 'left' | 'right';
  pclClassicIconSize: number;
  /** Enable the PCL launch-page transition and launching panel animation. */
  pclLaunchAnimation: boolean;
  /** Optional floating quick-action buttons on the PCL launch page. */
  pclFloatTasks: boolean;
  pclFloatLog: boolean;
  pclFloatRefresh: boolean;
  pclFloatMusic: boolean;
  /** Open an independent editor window even for the currently selected instance. */
  pclEditCurrentInWindow: boolean;
  /** Clear accumulated launch console output when a new launch starts. */
  pclClearLaunchLogOnStart: boolean;
  /** Keep startup log views pinned to the newest output. */
  pclLaunchLogAutoScroll: boolean;
  pclTitleLogo: boolean;
  pclTitleBadge: boolean;
  /** Show the Luna mark before the PCL title-bar brand. */
  pclTitleIcon: boolean;
  /** Accent color for the PCL shell (buttons, links, selected rows). */
  pclAccentColor: string;
  /** Data URL for the PCL page background; empty means no image. */
  pclBackgroundImage: string;
  pclBackgroundImageName: string;
  /** 0..1 opacity of the background image. */
  pclBackgroundOpacity: number;
  /** Background music stored in IndexedDB; localStorage only keeps metadata. */
  pclBackgroundMusicName: string;
  pclBackgroundMusicVolume: number;
  pclBackgroundMusicEnabled: boolean;
  /** Custom cards shown at the top of the PCL launch page. */
  pclHomeCards: PclHomeCard[];
  /** Optional PCL launch-page widgets. */
  pclHomeShowInstances: boolean;
  pclHomeShowServers: boolean;
  pclHomeSwitchOnQuickLaunch: boolean;
  pclHomeInstanceLayout: 'grid' | 'list';
  pclHomeInstanceColumns: number | 'auto';
  pclHomeInstanceRows: number;
  pclHomeInstanceWidth: number;
  pclHomeInstanceFontSize: number;
  pclHomeInstanceTextPosition: 'beside' | 'below';
  pclHomeInstanceGrouping: 'flat' | 'grouped';
  pclHomeInstanceSort: 'manual' | 'name' | 'version';
  pclHomeInstanceOrder: string[];
  /** Group identifiers collapsed in the PCL instance lists. */
  pclHomeInstanceCollapsedGroups: string[];
  pclHomeCardOrder: string[];
  /** The last instance selected in the classic launcher shell. */
  selectedInstanceId: string;
}

const storageKey = 'luna-neo-ui.preferences';
const defaults: NeoUiPreferences = {
  // The PCL-CE inspired shell is now the default appearance.
  shell: 'pcl',
  theme: 'dark',
  accent: '#818cf8',
  density: 'comfortable',
  background: 'gradient',
  animations: true,
  pclFont: 'system',
  // PCL Style is the shell; its initial visual preset follows Luna Neo.
  // Users can still switch to the classic PCL preset in personalization.
  pclStylePreset: 'luna-neo',
  pclLoadingAnimation: 'luna-spinner',
  pclAutoRefreshOnPageOpen: true,
  pclBaseFontSize: 14,
  pclPageNavWidth: 220,
  pclNavLayout: 'mmc-classic',
  pclClassicSide: 'right',
  pclClassicIconSize: 96,
  pclLaunchAnimation: false,
  pclFloatTasks: false,
  pclFloatLog: false,
  pclFloatRefresh: false,
  pclFloatMusic: false,
  pclEditCurrentInWindow: true,
  pclClearLaunchLogOnStart: true,
  pclLaunchLogAutoScroll: true,
  pclTitleLogo: true,
  pclTitleBadge: true,
  pclTitleIcon: true,
  pclAccentColor: '#d65c9a',
  pclBackgroundImage: '',
  pclBackgroundImageName: '',
  pclBackgroundOpacity: 0.45,
  pclBackgroundMusicName: '',
  pclBackgroundMusicVolume: 0.35,
  pclBackgroundMusicEnabled: false,
  pclHomeCards: [
    { id: 'welcome', title: i18n.global.t('neo.extra.welcomeTitle'), content: i18n.global.t('neo.extra.welcomeContent') },
  ],
  pclHomeShowInstances: true,
  pclHomeShowServers: true,
  pclHomeSwitchOnQuickLaunch: false,
  pclHomeInstanceLayout: 'grid',
  pclHomeInstanceColumns: 'auto',
  pclHomeInstanceRows: 4,
  pclHomeInstanceWidth: 100,
  pclHomeInstanceFontSize: 14,
  pclHomeInstanceTextPosition: 'below',
  pclHomeInstanceGrouping: 'grouped',
  pclHomeInstanceSort: 'name',
  pclHomeInstanceOrder: [],
  pclHomeInstanceCollapsedGroups: [],
  pclHomeCardOrder: ['instances', 'log', 'hint', 'custom:welcome', 'servers'],
  selectedInstanceId: '',
};

export const pclStylePresetLabels: Record<PclStylePreset, string> = {
  'luna-neo': 'Luna Neo',
  pcl: 'PCL',
};

const PCL_STYLE_PRESETS: Record<PclStylePreset, Partial<NeoUiPreferences>> = {
  'luna-neo': {
    pclLoadingAnimation: 'luna-spinner',
    pclAutoRefreshOnPageOpen: true,
    pclFont: 'system',
    pclBaseFontSize: 14,
    pclNavLayout: 'mmc-classic',
    pclClassicSide: 'right',
    pclClassicIconSize: 96,
    pclLaunchAnimation: false,
    pclLaunchLogAutoScroll: true,
      pclFloatTasks: false,
    pclFloatLog: false,
    pclFloatRefresh: false,
    pclFloatMusic: false,
    pclTitleLogo: true,
    pclTitleBadge: true,
    pclTitleIcon: true,
    pclAccentColor: '#d65c9a',
    pclHomeShowInstances: true,
    pclHomeShowServers: true,
    pclHomeSwitchOnQuickLaunch: false,
    pclHomeInstanceLayout: 'grid',
    pclHomeInstanceColumns: 'auto',
    pclHomeInstanceRows: 4,
    pclHomeInstanceWidth: 100,
    pclHomeInstanceFontSize: 14,
    pclHomeInstanceTextPosition: 'below',
    pclHomeInstanceGrouping: 'grouped',
    pclHomeInstanceSort: 'name',
    pclHomeCardOrder: ['instances', 'log', 'hint', 'custom:welcome', 'servers'],
  },
  pcl: {
    pclLoadingAnimation: 'pcl-pickaxe',
    pclAutoRefreshOnPageOpen: true,
    pclFont: 'system',
    pclBaseFontSize: 13,
    pclNavLayout: 'pcl',
    pclClassicSide: 'right',
    pclClassicIconSize: 96,
    pclLaunchAnimation: true,
    pclLaunchLogAutoScroll: true,
    pclFloatTasks: true,
    pclFloatLog: true,
    pclFloatRefresh: true,
    pclFloatMusic: true,
    pclTitleLogo: true,
    pclTitleBadge: true,
    pclTitleIcon: true,
    pclAccentColor: '#2a80e3',
    pclHomeShowInstances: false,
    pclHomeShowServers: false,
    pclHomeSwitchOnQuickLaunch: false,
    pclHomeInstanceLayout: 'grid',
    pclHomeInstanceColumns: 2,
    pclHomeInstanceRows: 4,
    pclHomeInstanceWidth: 100,
    pclHomeInstanceFontSize: 13,
    pclHomeInstanceTextPosition: 'beside',
    pclHomeInstanceGrouping: 'flat',
    pclHomeInstanceSort: 'manual',
    pclHomeCardOrder: ['hint', 'custom:welcome', 'log', 'instances', 'servers'],
  },
};

export function applyPclStylePreset(preset: PclStylePreset) {
  const values = PCL_STYLE_PRESETS[preset];
  if (!values) return;
  Object.assign(neoUiPreferences, values, { pclStylePreset: preset });
  persistNeoUiPreferences();
}

const pclFontStacks: Record<PclFont, string> = {
  system: '"Segoe UI", "Microsoft YaHei UI", "Microsoft YaHei", "PCL English", sans-serif',
  yahei: '"Microsoft YaHei UI", "Microsoft YaHei", "Segoe UI", sans-serif',
  dengxian: '"DengXian", "Microsoft YaHei UI", "Segoe UI", sans-serif',
  simsun: 'SimSun, "Songti SC", "Microsoft YaHei", serif',
};

function normalizeLoaded(value: any): NeoUiPreferences {
    const merged = { ...defaults, ...(value && typeof value === 'object' ? value : {}) };
    const pageNavWidth = Number(merged.pclPageNavWidth);
    merged.pclPageNavWidth = Number.isFinite(pageNavWidth)
      ? Math.max(140, Math.min(360, Math.round(pageNavWidth)))
      : defaults.pclPageNavWidth;
    if (!['luna-spinner', 'pcl-pickaxe', 'pcl-bar'].includes(merged.pclLoadingAnimation)) {
      merged.pclLoadingAnimation = defaults.pclLoadingAnimation;
    }
    if (typeof merged.pclAutoRefreshOnPageOpen !== 'boolean') {
      merged.pclAutoRefreshOnPageOpen = defaults.pclAutoRefreshOnPageOpen;
    }
    if (typeof merged.pclLaunchLogAutoScroll !== 'boolean') {
      merged.pclLaunchLogAutoScroll = defaults.pclLaunchLogAutoScroll;
    }
    if (typeof merged.pclTitleIcon !== 'boolean') {
      merged.pclTitleIcon = defaults.pclTitleIcon;
    }

    merged.pclHomeCardOrder = Array.isArray(merged.pclHomeCardOrder)
      ? merged.pclHomeCardOrder.map(String).filter(Boolean)
      : ['hint', 'log', 'instances', 'servers'];
    merged.pclHomeInstanceOrder = Array.isArray(merged.pclHomeInstanceOrder)
      ? merged.pclHomeInstanceOrder.map(String).filter(Boolean)
      : [];
    merged.pclHomeInstanceCollapsedGroups = Array.isArray(merged.pclHomeInstanceCollapsedGroups)
      ? [...new Set(merged.pclHomeInstanceCollapsedGroups.map(String).filter(Boolean))]
      : [];
    merged.pclHomeCards = Array.isArray(merged.pclHomeCards)
      ? merged.pclHomeCards.filter((card: any) => card && typeof card === 'object').map((card: any) => ({
        id: String(card.id || `${Date.now()}-${Math.random().toString(36).slice(2, 7)}`),
        title: String(card.title || ''),
        content: String(card.content || ''),
      }))
      : [];
    return merged;
}

function samePreferenceValue(left: unknown, right: unknown): boolean {
  if (Object.is(left, right)) return true;
  if (left === null || right === null || typeof left !== 'object' || typeof right !== 'object') return false;
  try { return JSON.stringify(left) === JSON.stringify(right); } catch { return false; }
}

/** Apply a cross-window snapshot without writing unchanged fields into Vue's
 * reactive graph. Components that do not use a changed preference therefore
 * receive no update at all. */
function patchPreferences(value: Partial<NeoUiPreferences>) {
  const next = normalizeLoaded(value);
  const changed: string[] = [];
  for (const key of Object.keys(next) as (keyof NeoUiPreferences)[]) {
    if (samePreferenceValue(neoUiPreferences[key], next[key])) continue;
    (neoUiPreferences[key] as any) = next[key] as any;
    changed.push(key);
  }
  if (changed.length) recordRuntimeDebug('store', 'neo-ui-preferences', 'preference snapshot patched', { fields: changed });
  return changed.length > 0;
}

function load(): NeoUiPreferences {
  if (isTauri() || typeof localStorage === 'undefined') return { ...defaults };
  try { return normalizeLoaded(JSON.parse(localStorage.getItem(storageKey) || '{}')); }
  catch { return { ...defaults }; }
}

export const neoUiPreferences = reactive<NeoUiPreferences>(load());
let hydrated = !isTauri();
let applyingRemote = false;
let bridgePromise: Promise<void> | undefined;
let bridgeUnlisten: UnlistenFn | undefined;

function startNeoPreferencesBridge() {
  if (bridgePromise) return bridgePromise;
  if (!isTauri()) {
    window.addEventListener('storage', event => {
      if (event.key !== storageKey || !event.newValue) return;
      applyingRemote = true;
      try { patchPreferences(JSON.parse(event.newValue)); }
      catch { /* Ignore malformed browser preview state. */ }
      finally { queueMicrotask(() => { applyingRemote = false; }); }
    });
    bridgePromise = Promise.resolve();
    return bridgePromise;
  }
  bridgePromise = listen<Partial<NeoUiPreferences>>('neo-ui-preferences-changed', event => {
    applyingRemote = true;
    patchPreferences(event.payload);
    queueMicrotask(() => { applyingRemote = false; });
  }).then(unlisten => { bridgeUnlisten = unlisten; }).catch(error => { console.error('Unable to bridge Neo UI preferences', error); });
  return bridgePromise;
}

export async function hydrateNeoUiPreferences() {
  if (hydrated) { startNeoPreferencesBridge(); return; }
  try {
    const stored = await invoke<Partial<NeoUiPreferences> | null>('get_neo_ui_preferences');
    if (stored) patchPreferences(stored);
  } catch (error) { console.error('Unable to load Neo UI preferences', error); }
  hydrated = true;
  startNeoPreferencesBridge();
  applyNeoUiPreferences();
}

export const useNeoUiPreferencesStore = defineStore('neo-ui-preferences', {
  state: () => ({ preferences: neoUiPreferences }),
  actions: {
    hydrate: hydrateNeoUiPreferences,
    persist: persistNeoUiPreferences,
  },
});
export const neoUiIsDark = computed(() => {
  if (neoUiPreferences.theme === 'dark') return true;
  if (neoUiPreferences.theme === 'light') return false;
  return typeof window === 'undefined' || window.matchMedia('(prefers-color-scheme: dark)').matches;
});

/** Persist immediately when a preference changes at an interaction boundary. */
export function persistNeoUiPreferences() {
  if (!hydrated) return;
  const value = JSON.parse(JSON.stringify(neoUiPreferences));
  if (isTauri()) {
    void invoke('set_neo_ui_preferences', { preferences: value }).then(() => emit('neo-ui-preferences-changed', value)).catch(error => console.error('Unable to persist Neo UI preferences', error));
  } else if (typeof localStorage !== 'undefined') {
    try { localStorage.setItem(storageKey, JSON.stringify(value)); }
    catch { /* Browser preview storage may be unavailable. */ }
  }
}

export const PCL_ACCENT_PRESETS = [
  { name: i18n.global.t('neo.extra.accentCatBlue'), value: '#2a80e3' },
  { name: i18n.global.t('neo.extra.accentMint'), value: '#2fa58a' },
  { name: i18n.global.t('neo.extra.accentGreen'), value: '#3f9b55' },
  { name: i18n.global.t('neo.extra.accentAmber'), value: '#d68a2e' },
  { name: i18n.global.t('neo.extra.accentRed'), value: '#d0525a' },
  { name: i18n.global.t('neo.extra.accentViolet'), value: '#8a5cf6' },
  { name: i18n.global.t('neo.extra.accentPink'), value: '#d65c9a' },
  { name: i18n.global.t('neo.extra.accentGraphite'), value: '#5c738a' },
] as const;

export function normalizePclAccent(value: string): string {
  const match = String(value || '').trim().match(/^#?([0-9a-f]{6})$/i);
  return match ? `#${match[1].toLowerCase()}` : '#2a80e3';
}

function mixHex(color: string, target: string, amount: number): string {
  const source = normalizePclAccent(color).slice(1);
  const destination = normalizePclAccent(target).slice(1);
  const channel = (offset: number) => {
    const from = parseInt(source.slice(offset, offset + 2), 16);
    const to = parseInt(destination.slice(offset, offset + 2), 16);
    return Math.round(from + (to - from) * amount).toString(16).padStart(2, '0');
  };
  return `#${channel(0)}${channel(2)}${channel(4)}`;
}

export function applyNeoUiPreferences() {
  if (typeof document === 'undefined') return;
  const root = document.documentElement;
  const isDark = neoUiIsDark.value;
  root.dataset.neoTheme = isDark ? 'dark' : 'light';
  root.dataset.neoShell = neoUiPreferences.shell;
  root.dataset.neoDensity = neoUiPreferences.density;
  root.dataset.neoBackground = neoUiPreferences.background;
  root.dataset.neoAnimations = neoUiPreferences.animations ? 'on' : 'off';
  root.dataset.pclTitleLogo = neoUiPreferences.pclTitleLogo ? 'on' : 'off';
  root.dataset.pclTitleBadge = neoUiPreferences.pclTitleBadge ? 'on' : 'off';
  root.dataset.pclTitleIcon = neoUiPreferences.pclTitleIcon ? 'on' : 'off';
  root.style.setProperty('--neo-primary', neoUiPreferences.accent);
  const pclAccent = normalizePclAccent(neoUiPreferences.pclAccentColor);
  root.style.setProperty('--pcl-c3', pclAccent);
  root.style.setProperty('--pcl-c2', isDark ? mixHex(pclAccent, '#ffffff', 0.4) : mixHex(pclAccent, '#000000', 0.2));
  root.style.setProperty('--pcl-c4', mixHex(pclAccent, '#ffffff', 0.2));
  root.style.setProperty('--pcl-c5', isDark ? mixHex(pclAccent, '#000000', 0.32) : mixHex(pclAccent, '#ffffff', 0.6));
  root.style.setProperty('--pcl-font', pclFontStacks[neoUiPreferences.pclFont] || pclFontStacks.system);
  root.style.setProperty('--pcl-font-scale', String(Math.max(9, Math.min(20, Number(neoUiPreferences.pclBaseFontSize) || 13)) / 13));
  root.style.setProperty('--pcl-page-nav-width', `${Math.max(140, Math.min(360, Number(neoUiPreferences.pclPageNavWidth) || defaults.pclPageNavWidth))}px`);
  root.dataset.pclLaunchAnimation = neoUiPreferences.pclLaunchAnimation ? 'on' : 'off';
  root.dataset.pclBackground = neoUiPreferences.pclBackgroundImage ? 'on' : 'off';
  root.style.setProperty('--pcl-bg-image', neoUiPreferences.pclBackgroundImage ? `url("${neoUiPreferences.pclBackgroundImage}")` : 'none');
  root.style.setProperty('--pcl-bg-overlay', String(Math.max(0, Math.min(1, 1 - neoUiPreferences.pclBackgroundOpacity))));
}

watch(neoUiPreferences, () => {
  if (!applyingRemote) persistNeoUiPreferences();
  applyNeoUiPreferences();
}, { deep: true, immediate: true });

export function resetNeoUiPreferences() {
  Object.assign(neoUiPreferences, defaults);
}
