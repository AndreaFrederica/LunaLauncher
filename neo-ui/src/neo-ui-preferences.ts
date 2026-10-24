import { computed, reactive, watch } from 'vue';

export type NeoTheme = 'dark' | 'light' | 'system';
export type NeoDensity = 'comfortable' | 'compact';
export type NeoBackground = 'solid' | 'gradient';
export type NeoShell = 'neo' | 'pcl';
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
  pclAccentColor: '#d65c9a',
  pclBackgroundImage: '',
  pclBackgroundImageName: '',
  pclBackgroundOpacity: 0.45,
  pclBackgroundMusicName: '',
  pclBackgroundMusicVolume: 0.35,
  pclBackgroundMusicEnabled: false,
  pclHomeCards: [
    { id: 'welcome', title: '欢迎回来', content: '在 设置 → 个性化 → 主页卡片 中编辑这张卡片。' },
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

function load(): NeoUiPreferences {
  if (typeof localStorage === 'undefined') return { ...defaults };
  try {
    const value = JSON.parse(localStorage.getItem(storageKey) || '{}');
    const merged = { ...defaults, ...value };
    if (!['luna-spinner', 'pcl-pickaxe', 'pcl-bar'].includes(merged.pclLoadingAnimation)) {
      merged.pclLoadingAnimation = defaults.pclLoadingAnimation;
    }
    if (typeof merged.pclAutoRefreshOnPageOpen !== 'boolean') {
      merged.pclAutoRefreshOnPageOpen = defaults.pclAutoRefreshOnPageOpen;
    }
    if (typeof merged.pclLaunchLogAutoScroll !== 'boolean') {
      merged.pclLaunchLogAutoScroll = defaults.pclLaunchLogAutoScroll;
    }

    merged.pclHomeCardOrder = Array.isArray(merged.pclHomeCardOrder)
      ? merged.pclHomeCardOrder.map(String).filter(Boolean)
      : ['hint', 'log', 'instances', 'servers'];
    merged.pclHomeInstanceOrder = Array.isArray(merged.pclHomeInstanceOrder)
      ? merged.pclHomeInstanceOrder.map(String).filter(Boolean)
      : [];
    merged.pclHomeCards = Array.isArray(merged.pclHomeCards)
      ? merged.pclHomeCards.filter((card: any) => card && typeof card === 'object').map((card: any) => ({
        id: String(card.id || `${Date.now()}-${Math.random().toString(36).slice(2, 7)}`),
        title: String(card.title || ''),
        content: String(card.content || ''),
      }))
      : [];
    return merged;
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

/** Persist immediately when a preference changes at an interaction boundary. */
export function persistNeoUiPreferences() {
  if (typeof localStorage === 'undefined') return;
  try {
    localStorage.setItem(storageKey, JSON.stringify(neoUiPreferences));
  } catch {
    // Storage may be unavailable in a restricted webview.
  }
}

export const PCL_ACCENT_PRESETS = [
  { name: '猫蓝', value: '#2a80e3' },
  { name: '薄荷', value: '#2fa58a' },
  { name: '翠绿', value: '#3f9b55' },
  { name: '琥珀', value: '#d68a2e' },
  { name: '绯红', value: '#d0525a' },
  { name: '紫罗兰', value: '#8a5cf6' },
  { name: '樱花', value: '#d65c9a' },
  { name: '石墨', value: '#5c738a' },
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
  root.dataset.neoTheme = neoUiIsDark.value ? 'dark' : 'light';
  root.dataset.neoShell = neoUiPreferences.shell;
  root.dataset.neoDensity = neoUiPreferences.density;
  root.dataset.neoBackground = neoUiPreferences.background;
  root.dataset.neoAnimations = neoUiPreferences.animations ? 'on' : 'off';
  root.dataset.pclTitleLogo = neoUiPreferences.pclTitleLogo ? 'on' : 'off';
  root.dataset.pclTitleBadge = neoUiPreferences.pclTitleBadge ? 'on' : 'off';
  root.style.setProperty('--neo-primary', neoUiPreferences.accent);
  const pclAccent = normalizePclAccent(neoUiPreferences.pclAccentColor);
  root.style.setProperty('--pcl-c3', pclAccent);
  root.style.setProperty('--pcl-c2', mixHex(pclAccent, '#ffffff', 0.4));
  root.style.setProperty('--pcl-c4', mixHex(pclAccent, '#ffffff', 0.2));
  root.style.setProperty('--pcl-c5', mixHex(pclAccent, '#000000', 0.32));
  root.style.setProperty('--pcl-font', pclFontStacks[neoUiPreferences.pclFont] || pclFontStacks.system);
  root.style.setProperty('--pcl-font-scale', String(Math.max(9, Math.min(20, Number(neoUiPreferences.pclBaseFontSize) || 13)) / 13));
  root.dataset.pclLaunchAnimation = neoUiPreferences.pclLaunchAnimation ? 'on' : 'off';
  root.dataset.pclBackground = neoUiPreferences.pclBackgroundImage ? 'on' : 'off';
  root.style.setProperty('--pcl-bg-image', neoUiPreferences.pclBackgroundImage ? `url("${neoUiPreferences.pclBackgroundImage}")` : 'none');
  root.style.setProperty('--pcl-bg-overlay', String(Math.max(0, Math.min(1, 1 - neoUiPreferences.pclBackgroundOpacity))));
}

watch(neoUiPreferences, () => {
  persistNeoUiPreferences();
  applyNeoUiPreferences();
}, { deep: true, immediate: true });

export function resetNeoUiPreferences() {
  Object.assign(neoUiPreferences, defaults);
}
