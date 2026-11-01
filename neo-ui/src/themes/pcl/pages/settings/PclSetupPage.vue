<template>
  <section class="pcl-page pcl-setup-page" :class="{ 'is-embedded': embedded }">
    <PclPageNav v-if="!embedded" v-model="section" :groups="groups" @refresh="refreshSection" />

    <main class="pcl-page-main">
      <div class="pcl-page-view pcl-setup-stack">
          <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>
          <PclHint v-if="notice" theme="blue" closable @close="notice = ''">{{ notice }}</PclHint>

          <PclCard v-if="isInstance && sectionOverrideGroups.length" :title="t('pcl.auto2.kc5133a8b25')">
            <p class="pcl-muted">{{ t('pcl.auto2.k068fc09151') }}</p>
            <div class="pcl-check-grid">
              <PclCheckbox v-for="item in sectionOverrideGroups" :key="item.key" :model-value="bool(item.key)"
                :label="item.label" @update:model-value="save(item.key, $event)" />
            </div>
          </PclCard>

          <div v-show="section === 'accounts'" class="pcl-settings-section">
            <PclAccountPage :accounts="accounts || []" :busy="busy" :execute="execute" embedded @changed="emit('changed')" />
          </div>

          <PclLaunchSettings v-show="section === 'launch'" :active="section === 'launch'" :t="t" :window-mode="windowMode" :window-options="windowOptions" :drafts="drafts" :val="val" :bool="bool" :num="num" :set-window-mode="setWindowMode" :save="save" />

          <PclMemorySettings v-show="section === 'memory'" :active="section === 'memory'" :t="t" :max-mem="maxMem" :memory-mode="memoryMode" :memory-used-percent="memoryUsedPercent" :memory-alloc-after-percent="memoryAllocAfterPercent" :memory-used-gib="memoryUsedGiB" :memory-total-gib="memoryTotalGiB" :drafts="drafts" :draft-number="draftNumber" :format-mib="formatMib" :on-memory-mode="onMemoryMode" :save="save" />

                    <PclAdvancedSettings v-show="section === 'advanced'" :t="t" :drafts="drafts" :draft-value="draftValue" :bool="bool" :save="save" />

                    <PclDownloadSettings v-show="section === 'download-settings'" />

                    <PclPathsSettings v-show="section === 'paths'" :t="t" :drafts="drafts" :draft-value="draftValue" :bool="bool" :save="save" :busy="busy" :path-settings="pathSettings" :tool-path-settings="toolPathSettings" :choose-directory="chooseDirectory" :choose-file-path="chooseFilePath" />

                    <PclConsoleSettings v-show="section === 'console'" :t="t" :drafts="drafts" :draft-value="draftValue" :draft-number="draftNumber" :bool="bool" :save="save" :save-paste-url="savePasteUrl" :console-color-options="consoleColorOptions" :console-font-options="consoleFontOptions" :paste-options="[{ label: 'mclo.gs', value: 3 }, { label: '0x0.st', value: 0 }, { label: 'paste.gg', value: 2 }, { label: 'hastebin', value: 1 }]" :is-instance="isInstance" :busy="busy" />

                    <PclAdvancedRuntimeSettings v-show="section === 'advanced-settings'" />

                    <PclGameSettings v-show="section === 'game'" />

                    <PclNetworkSettings v-show="section === 'network'" />

                    <PclCacheSettings v-show="section === 'cache'" />

          <div v-show="section === 'java'" class="pcl-settings-section">
            <PclJavaPanel :scope="scope" :settings="settings" :busy="busy" :execute="execute" @changed="emit('changed')" />
          </div>

                    <PclUiSettings v-show="section === 'ui'" />

                    <PclLanguageSettings v-show="section === 'language'" />

                    <PclMiscSettings v-show="section === 'misc'" />

                    <PclAboutSettings v-show="section === 'about'" :t="t" :runtime-info="runtimeInfo" :application-version="applicationVersion" :credits="credits" :contributors="contributors" :open-url="openUrl" :project-url="projectUrl" :license-url="licenseUrl" :docs-url="docsUrl" :bmcl-api-url="bmclApiUrl" :mcim-url="mcimUrl" />

                    <PclUpdateSettings v-show="section === 'update'" :t="t" :update-state="updateState" :update-check="updateCheck" :update-checked="updateChecked" :update-checking="updateChecking" :busy="busy" :configure-update="configureUpdate" :check-update="checkUpdate" :apply-update="applyUpdate" :open-update-log="openUpdateLog" />

                    <PclFeedbackSettings v-show="section === 'feedback'" :t="t" :project-url="projectUrl" :open-url="openUrl" />

                    <PclLogsSettings v-show="section === 'logs'" :t="t" :update-state="updateState" :busy="busy" :open-logs="openLogs" :open-update-log="openUpdateLog" />
        </div>
    </main>
  </section>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, provide, reactive, ref, toRef, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { launcher } from '../../../../boot/launcher-api';
import lunaLogo from '../../../../assets/luna-launcher.png';
import { localeFromLauncherLanguage, setNeoLocale } from '../../../../i18n';
import PclPageNav from '../../components/PclPageNav.vue';
import PclCard from '../../components/PclCard.vue';
import PclButton from '../../components/PclButton.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import { pickPclDirectory, pickPclFile } from '../../composables/usePclFileDialog';
import PclHint from '../../components/PclHint.vue';
import PclInput from '../../components/PclInput.vue';
import PclNumberInput from '../../components/PclNumberInput.vue';
import PclSecretInput from '../../components/PclSecretInput.vue';
import PclSelect from '../../components/PclSelect.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclRadio from '../../components/PclRadio.vue';
import PclSlider from '../../components/PclSlider.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclMemorySettings from './PclMemorySettings.vue';
import PclLaunchSettings from './PclLaunchSettings.vue';
import PclAdvancedSettings from './PclAdvancedSettings.vue';
import PclPathsSettings from './PclPathsSettings.vue';
import PclConsoleSettings from './PclConsoleSettings.vue';
import PclAboutSettings from './PclAboutSettings.vue';
import PclUpdateSettings from './PclUpdateSettings.vue';
import PclFeedbackSettings from './PclFeedbackSettings.vue';
import PclLogsSettings from './PclLogsSettings.vue';
import PclDownloadSettings from './PclDownloadSettings.vue';
import PclAdvancedRuntimeSettings from './PclAdvancedRuntimeSettings.vue';
import PclGameSettings from './PclGameSettings.vue';
import PclNetworkSettings from './PclNetworkSettings.vue';
import PclCacheSettings from './PclCacheSettings.vue';
import PclUiSettings from './PclUiSettings.vue';
import PclLanguageSettings from './PclLanguageSettings.vue';
import PclMiscSettings from './PclMiscSettings.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclAccountPage from '../accounts/PclAccountPage.vue';
import PclJavaPanel from './PclJavaPanel.vue';
import PclAppearancePanel from './PclAppearancePanel.vue';
import type { PclNavGroup } from '../../types';
import { hiddenPclFeatures, isPclFeatureHidden } from '../../composables/usePclFeatures';
import { useRuntimeLifecycle } from '../../../../common/runtime/lifecycle-debug';
import { useThemeResource } from '../../../../common/runtime/theme-runtime';
import { PclSetupContextKey } from './PclSetupContext';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
type ApiSetting = {
  key: string;
  value: unknown;
  default?: unknown;
  type?: string;
  category?: string;
  surface?: string;
  sensitive?: boolean;
  redacted?: boolean;
  isDefault?: boolean;
};

const props = defineProps<{ settings: ApiSetting[]; busy: boolean; scope: Record<string, unknown>; execute: Execute; accounts?: any[]; embedded?: boolean; initialSection?: string; active?: boolean }>();
const pageActive = computed(() => props.active !== false);
useRuntimeLifecycle('theme:pcl.settings-page', pageActive);
const emit = defineEmits<{ changed: [] }>();
const { t } = useI18n();

const section = ref('launch');
const error = ref(''), notice = ref('');
const drafts = reactive<Record<string, any>>({});
const languages = ref<any[]>([]), languageId = ref(''), useSystemLocale = ref(false), languagesLoading = ref(false);
const runtimeInfo = ref<any>({});
const updateState = ref<any>({}), updateCheck = ref<any>({}), updateChecked = ref(false), updateChecking = ref(false);
const proxy = reactive({ type: 'None', host: '', port: 7890, username: '', password: '' });
const proxyTestUrl = ref('https://api.minecraftservices.com/'), proxyTestResult = ref('');
const cacheState = ref<any>({}), cacheRoutes = ref<any[]>([]), cacheBusy = ref(false), cacheError = ref('');
const onlineBackends = ref<any[]>([]), onlineBusy = ref(false);

const isInstance = computed(() => props.scope.scope === 'instance');
watch(() => props.initialSection, value => { if (value && value !== section.value) section.value = value; }, { immediate: true });
const launcherGroups: PclNavGroup[] = [
  { label: t('pcl.auto2.k2305ee3ca3'), items: [
    { id: 'launch', label: t('pcl.auto2.kebd26da421'), icon: 'rocket', refresh: true },
    { id: 'java', label: 'Java', icon: 'coffee', refresh: true },
    { id: 'game', label: t('pcl.auto2.kd4cbc9e6a2'), icon: 'book-marked', refresh: true },
    { id: 'advanced', label: t('pcl.auto2.kb85853c3fc'), icon: 'tool-case' },
  ] },
  { label: t('pcl.auto2.ka72ef18d9a'), items: [
    { id: 'network', label: t('pcl.auto2.k1d5e43605d'), icon: 'bubbles', refresh: true },
  ] },
  { label: t('pcl.auto2.ka992a7f81a'), items: [
    { id: 'accounts', label: t('pcl.auto2.k9823d89dbc'), icon: 'user', refresh: true },
    { id: 'ui', label: t('pcl.auto2.kcd4994e503'), icon: 'palette', refresh: true },
    { id: 'language', label: t('pcl.auto2.kcd99b225a0'), icon: 'earth', refresh: true },
    { id: 'misc', label: t('pcl.auto2.kb046fe277d'), icon: 'monitor-cog', refresh: true },
    { id: 'download-settings', label: t('pcl.auto2.kbcf7016f51'), icon: 'download', refresh: true },
    { id: 'cache', label: t('pcl.auto2.kc15668b86b'), icon: 'layers', refresh: true },
    { id: 'paths', label: t('pcl.auto2.kf9b678c1d7'), icon: 'folder-cog', refresh: true },
    { id: 'console', label: t('pcl.auto2.k5bd086d22a'), icon: 'terminal', refresh: true },
    { id: 'advanced-settings', label: t('pcl.auto2.kd501b653be'), icon: 'settings', refresh: true },
  ] },
  { label: t('pcl.auto2.kbed172efc9'), items: [
    { id: 'about', label: t('pcl.auto2.k638d2df6e1'), icon: 'info' },
    { id: 'update', label: t('pcl.auto2.k7e5ce811e8'), icon: 'refresh-cw' },
    { id: 'feedback', label: t('pcl.auto2.k42a36e9497'), icon: 'message-circle', refresh: true },
    { id: 'logs', label: t('pcl.auto2.kb923b26d33'), icon: 'scroll-text' },
  ] },
];
const instanceGroups: PclNavGroup[] = [
  { label: t('pcl.auto2.k1836ba7e88'), items: [
    { id: 'launch', label: t('pcl.auto2.k6c4942326d'), icon: 'rocket', refresh: true },
    { id: 'java', label: 'Java', icon: 'coffee', refresh: true },
    { id: 'memory', label: t('pcl.auto2.kd014ab7703'), icon: 'gauge', refresh: true },
    { id: 'network', label: t('pcl.auto2.k6aca62f330'), icon: 'server', refresh: true },
    { id: 'advanced', label: t('pcl.auto2.kc009d0ab82'), icon: 'tool-case' },
    { id: 'advanced-settings', label: t('pcl.auto2.ke362c495b7'), icon: 'settings', refresh: true },
  ] },
];
const groups = computed(() => {
  const source = isInstance.value ? instanceGroups : launcherGroups;
  return source
    .map(group => ({
      ...group,
      items: group.items.filter(item => item.id === 'launch' || !isPclFeatureHidden(`setup.${item.id}`)),
    }))
    .filter(group => group.items.length);
});

const windowOptions = [
  { label: t('pcl.auto2.kc8d09cf955'), value: 'default' },
  { label: t('pcl.auto2.kda2d806e5f'), value: 'maximized' },
  { label: t('pcl.auto2.k1387e89231'), value: 'custom' },
];
const mirrorOptions = [
  { label: t('pcl.auto2.k212e81e6d0'), value: 0 },
  { label: t('pcl.auto2.kf0aba8a42d'), value: 1 },
  { label: t('pcl.auto2.kfe7276cd17'), value: 2 },
];
const downloadBackendOptions = [
  { label: t('pcl.auto2.ka626fe7700'), value: 0 },
  { label: 'Aria2', value: 1 },
  { label: t('pcl.auto2.k3ab6cb62c4'), value: 2 },
];
const modApiMirrorOptions = [
  { label: t('pcl.auto2.k212e81e6d0'), value: 0 },
  { label: t('pcl.auto2.kc0fa397b56'), value: 1 },
  { label: t('pcl.auto2.kfe7276cd17'), value: 2 },
];
const proxyTypeOptions = [
  { label: t('pcl.auto2.k4f187022e0'), value: 'None' },
  { label: t('pcl.auto2.kf4bbd91f79'), value: 'Default' },
  { label: t('pcl.auto2.kd95ef5ba39'), value: 'HTTP' },
  { label: t('pcl.auto2.k865df8bce7'), value: 'SOCKS5' },
];
const ariaProxyDisabled = computed(() => busyOrFollowingProxy());
function formatBytes(value: unknown): string {
  const bytes = Number(value || 0);
  if (!Number.isFinite(bytes) || bytes < 1024) return `${Math.max(0, Math.round(bytes))} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)} MiB`;
  return `${(bytes / 1024 / 1024 / 1024).toFixed(2)} GiB`;
}
function busyOrFollowingProxy() {
  return props.busy || bool('Aria2UseLauncherProxy') || draftValue('Aria2ProxyType', 'None') === 'None';
}
const consoleColorOptions = computed(() => [...new Set([
  'Builtin Dark', 'Builtin Light', 'Builtin Solarized Dark', 'Builtin Solarized Light',
  'Builtin Tango Dark', 'Builtin Tango Light', draftValue('ConsoleColorScheme', 'Builtin Dark'),
])].map(value => ({ label: value.replace('Builtin ', t('pcl.auto2.kacc7e7959f')), value })));
const consoleFontOptions = computed(() => [...new Set([
  draftValue('ConsoleFont', 'Courier New'), 'Cascadia Mono', 'Consolas', 'Courier New', 'Menlo', 'Monospace',
])].filter(Boolean).map(value => ({ label: value, value })));
const assetVerificationOptions = [
  { label: t('pcl.auto2.k1e3797b17e'), value: 0 }, { label: t('pcl.auto2.k06804c01de'), value: 1 },
  { label: t('pcl.auto2.ka9f4add9d6'), value: 2 }, { label: t('pcl.auto2.k53b916d50c'), value: 3 },
];
const assetVerificationDescriptions = [
  t('pcl.auto2.k05a706cafd'), t('pcl.auto2.k9512244fb6'),
  t('pcl.auto2.k941818ae16'), t('pcl.auto2.kc5d97cda92'),
];
const languageOptions = computed(() => languages.value.map(item => ({
  label: item.name || item.id,
  value: item.id,
})));

const settingsMap = computed(() => new Map(props.settings.map(setting => [setting.key, setting])));
const pathSettings = [
  { key: 'InstanceDir', label: t('pcl.auto2.k1252eac405'), placeholder: t('pcl.auto2.k1b269ad6f3') },
  { key: 'CentralModsDir', label: t('pcl.auto2.k4250e081b4') },
  { key: 'IconsDir', label: t('pcl.auto2.ka04545c951') },
  { key: 'DownloadsDir', label: t('pcl.auto2.kaf0bdd1ea7') },
  { key: 'SkinsDir', label: t('pcl.auto2.kc42aee88ea') },
  { key: 'JavaDir', label: t('pcl.auto2.k325cea939e') },
  { key: 'FTBAppInstancesPath', label: t('pcl.auto2.kcad8a90505') },
  { key: 'JProfilerPath', label: t('pcl.auto2.k314accf461') },
  { key: 'MCEditPath', label: t('pcl.auto2.ke756c0ecd0') },
] as const;
const toolPathSettings = [
  { key: 'Aria2ExecutablePath', label: t('pcl.auto2.ke332de5306') },
  { key: 'CurseForgeExternalToolPath', label: t('pcl.auto2.k6c0497a683') },
  { key: 'JVisualVMPath', label: t('pcl.auto2.kfeee3cc659') },
  { key: 'JsonEditor', label: t('pcl.auto2.kea37ad033c') },
] as const;
function val(key: string, fallback: any = ''): any {
  const setting = settingsMap.value.get(key);
  return setting ? setting.value : fallback;
}
function num(key: string, fallback = 0): number {
  const value = Number(val(key, fallback));
  return Number.isFinite(value) ? value : fallback;
}
function bool(key: string): boolean {
  const value = drafts[key] ?? val(key, false);
  if (typeof value === 'string') {
    const normalized = value.trim().toLowerCase();
    return normalized === 'true' || normalized === '1' || normalized === 'yes' || normalized === 'on';
  }
  return Boolean(value);
}
function draftNumber(key: string, fallback = 0): number {
  const value = Number(drafts[key]);
  return Number.isFinite(value) ? value : num(key, fallback);
}
function draftValue(key: string, fallback = ''): string {
  return drafts[key] === undefined || drafts[key] === null ? String(val(key, fallback)) : String(drafts[key]);
}
function formatMib(value: number): string {
  const gib = value / 1024;
  return `${Number.isInteger(gib) ? gib : gib.toFixed(1)} GiB`;
}
function formatSpeed(value: number): string {
  if (!value) return t('pcl.auto2.k142ff5c090');
  return value >= 1024 ? `${(value / 1024).toFixed(value % 1024 ? 1 : 0)} MiB/s` : `${value} KiB/s`;
}

const applicationVersion = computed(() => String(runtimeInfo.value.applicationVersion || runtimeInfo.value.version || runtimeInfo.value.launcherVersion || t('pcl.auto2.kd9892e1d1d')).trim());
const credits = [
  { name: 'PrismLauncher', info: t('pcl.auto2.k53ca260e72'), url: 'https://github.com/PrismLauncher/PrismLauncher' },
  { name: t('pcl.auto2.kc47f1374db'), info: t('pcl.auto2.k62b98f6a7e'), url: 'https://github.com/PCL-Community/PCL-CE' },
  { name: 'BMCLAPI', info: t('pcl.auto2.kc6d1ce501d'), url: 'https://bmclapidoc.bangbang93.com/' },
  { name: 'MCIM', info: t('pcl.auto2.k5ed7a01124'), url: 'https://mod.mcimirror.top/docs' },
  { name: 'Lucide', info: t('pcl.auto2.k2582b2c5c1'), url: 'https://lucide.dev' },
  { name: 'Qt', info: t('pcl.auto2.k9405b02a3e'), url: 'https://www.qt.io' },
];
const contributors = ['Luna Neo UI', 'PrismLauncher', 'BMCLAPI', 'MCIM', 'Lucide', 'Qt', t('pcl.auto2.ka7146a67de'), 'Bangbang93', 'Pysio'];
const projectUrl = 'https://github.com/AndreaFrederica/LunaLauncher';
const licenseUrl = `${projectUrl}/blob/main/COPYING.md`;
const docsUrl = 'https://lunalauncher.sirrus.cc';
const bmclApiUrl = 'https://bmclapidoc.bangbang93.com/';
const mcimUrl = 'https://mod.mcimirror.top/docs';
const maxMem = computed(() => draftNumber('MaxMemAlloc', 4096));
const memoryMode = ref<'auto' | 'custom'>('custom');
const memoryStats = ref({ totalMiB: 0, usedMiB: 0 });
const memoryTotalGiB = computed(() => (memoryStats.value.totalMiB / 1024).toFixed(1));
const memoryUsedGiB = computed(() => (memoryStats.value.usedMiB / 1024).toFixed(1));
const memoryUsedPercent = computed(() => memoryStats.value.totalMiB > 0
  ? Math.max(0, Math.min(100, memoryStats.value.usedMiB * 100 / memoryStats.value.totalMiB))
  : 0);
const memoryAllocPercent = computed(() => memoryStats.value.totalMiB > 0
  ? Math.max(0, Math.min(100, maxMem.value * 100 / memoryStats.value.totalMiB))
  : 0);
const memoryAllocAfterPercent = computed(() => Math.max(0, Math.min(100 - memoryUsedPercent.value, memoryAllocPercent.value)));
let memorySubscription = '';
let unlistenMemory: (() => void) | undefined;
let themeResourceActive = false;
async function refreshMemory() {
  try {
    const result = await props.execute('runtime.memory', {});
    const data = result?.data || result || {};
    const total = Number(data.totalMiB || 0), used = Number(data.usedMiB || 0);
    if (total > 0) memoryStats.value = { totalMiB: total, usedMiB: Math.max(0, Math.min(total, used)) };
  } catch {
    // Older backends may not expose runtime.memory.
  }
}
async function subscribeMemory() {
  try {
    const result: any = await props.execute('runtime.memory.subscribe');
    memorySubscription = String(result?.subscriptionId || '');
    if (!memorySubscription) return;
    unlistenMemory = await launcher.onStream((batch: any) => {
      if (!batch || batch.subscriptionId !== memorySubscription) return;
      for (const event of batch.events || []) {
        if (event.kind !== 'memory.sample') continue;
        const total = Number(event.totalMiB || 0);
        if (total > 0) memoryStats.value = { totalMiB: total, usedMiB: Math.max(0, Math.min(total, Number(event.usedMiB || 0))) };
      }
    });
  } catch {
    // Older backends may not expose the memory stream; the one-shot read remains.
  }
}
async function stopMemorySubscription() {
  unlistenMemory?.();
  unlistenMemory = undefined;
  if (memorySubscription) {
    const subscriptionId = memorySubscription;
    memorySubscription = '';
    await props.execute('event.unsubscribe', { subscriptionId }).catch(() => {});
  }
}
async function startMemorySubscription() {
  if (!themeResourceActive || props.active === false || memorySubscription) return;
  void refreshMemory();
  void subscribeMemory();
}
useThemeResource('pcl', () => {
  themeResourceActive = true;
  if (props.active !== false) void startMemorySubscription();
  return () => { themeResourceActive = false; void stopMemorySubscription(); };
});
watch(() => props.active, value => { if (value === false) void stopMemorySubscription(); else if (themeResourceActive) void startMemorySubscription(); });
onBeforeUnmount(() => { void stopMemorySubscription(); });
const windowMode = ref<'default' | 'maximized' | 'custom'>('default');
const mirrorType = ref(0);

watch(() => props.settings, (items, previous = []) => {
  const old = new Map(previous.map(item => [item.key, item]));
  for (const setting of items) {
    if (setting.redacted) { delete drafts[setting.key]; continue; }
    if (drafts[setting.key] === undefined || drafts[setting.key] === old.get(setting.key)?.value)
      drafts[setting.key] = setting.value;
  }
  mirrorType.value = num('DownloadMirrorType', 0);
  memoryMode.value = num('MinMemAlloc', 512) === 512 && num('MaxMemAlloc', 4096) === 4096 ? 'auto' : 'custom';
  windowMode.value = bool('LaunchMaximized') ? 'maximized'
    : (num('MinecraftWinWidth', 854) !== 854 || num('MinecraftWinHeight', 480) !== 480) ? 'custom' : 'default';
}, { immediate: true, deep: false });

watch(isInstance, () => { section.value = 'launch'; error.value = ''; notice.value = ''; });
watch(hiddenPclFeatures, () => {
  if (!groups.value.some(group => group.items.some(item => item.id === section.value))) section.value = 'launch';
}, { deep: true });
watch(section, value => {
  if (value === 'language') void loadLanguages();
  if (value === 'about' || value === 'logs') void loadRuntime();
  if (value === 'update') void loadUpdate();
  if (value === 'misc' && !isInstance.value) void loadProxy();
  if (value === 'cache' && !isInstance.value) void loadCache();
  if (value === 'network' && !isInstance.value) void loadOnlineBackends();
}, { immediate: true });
function setWindowMode(value: string | number) {
  windowMode.value = value as typeof windowMode.value;
  void save('LaunchMaximized', value === 'maximized');
  if (value === 'default') {
    void save('MinecraftWinWidth', 854);
    void save('MinecraftWinHeight', 480);
  }
}

function message(value: unknown): string {
  return value instanceof Error ? value.message : String(value);
}
function saveMirrorType(value: string | number) {
  mirrorType.value = Number(value);
  void save('DownloadMirrorType', mirrorType.value);
}
async function savePasteUrl() {
  const value = String(draftValue('PastebinCustomAPIBase', '')).trim().replace(/\/+$/, '');
  if (value) {
    try {
      if (!['http:', 'https:'].includes(new URL(value).protocol)) throw new Error();
    } catch { error.value = t('pcl.auto2.k1bf0b9499a'); return; }
  }
  await save('PastebinCustomAPIBase', value);
}
async function chooseGlobalDataPacks() {
  const selected = await pickPclDirectory({ title: t('pcl.auto2.k5f869fce49') });
  if (!selected) return;
  drafts.GlobalDataPacksPath = selected;
  save('GlobalDataPacksPath', selected);
}
async function chooseDirectory(key: string, label: string) {
  const selected = await pickPclDirectory({ title: t('pcl.setup.chooseNamed', { name: label }) });
  if (!selected) return;
  drafts[key] = selected;
  await save(key, selected);
}
async function chooseFilePath(key: string, label: string) {
  const selected = await pickPclFile({ title: t('pcl.setup.chooseNamed', { name: label }) });
  if (!selected) return;
  drafts[key] = selected;
  await save(key, selected);
}
const overrideGroups: Record<string, string[]> = {
  OverrideNativeWorkarounds: ['UseNativeGLFW', 'UseNativeOpenAL', 'CustomGLFWPath', 'CustomOpenALPath'],
  OverridePerformance: ['EnableFeralGamemode', 'EnableMangoHud', 'UseDiscreteGpu', 'UseZink'],
  OverrideLegacySettings: ['OnlineFixes'], OverrideGameTime: ['ShowGameTime', 'RecordGameTime'],
  OverrideAssetVerification: ['AssetVerificationMode', 'AssetCacheExpiryDays'],
  OverrideConsole: ['ShowConsole', 'AutoCloseConsole', 'ShowConsoleOnError', 'LogPrePostOutput'],
  OverrideWindow: ['LaunchMaximized', 'MinecraftWinWidth', 'MinecraftWinHeight'],
  OverrideMemory: ['MinMemAlloc', 'MaxMemAlloc', 'PermGen'],
  OverrideMiscellaneous: ['CloseAfterLaunch', 'QuitAfterGameStop'],
  OverrideCommands: ['PreLaunchCommand', 'WrapperCommand', 'PostExitCommand'],
  OverrideJavaArgs: ['JvmArgs'],
};
const sectionOverrideGroups = computed(() => {
  const sections: Record<string, { key: string; label: string }[]> = {
    launch: [{ key: 'OverrideWindow', label: t('pcl.auto2.k6d35264835') }, { key: 'OverrideConsole', label: t('pcl.auto2.kc0c465ebf4') }, { key: 'OverrideMiscellaneous', label: t('pcl.auto2.k42b3c4070c') }],
    memory: [{ key: 'OverrideMemory', label: t('pcl.auto2.k8952f181d8') }],
    advanced: [{ key: 'OverrideCommands', label: t('pcl.auto2.k347f57b633') }, { key: 'OverrideJavaArgs', label: t('pcl.auto2.k132a0f71b7') }],
    'advanced-settings': [{ key: 'OverrideAssetVerification', label: t('pcl.auto2.k38603922ea') }, { key: 'OverrideNativeWorkarounds', label: t('pcl.auto2.k1d8d0ba1d6') },
      { key: 'OverridePerformance', label: t('pcl.auto2.k5921928fa7') }, { key: 'OverrideLegacySettings', label: t('pcl.auto2.k74dc0961a0') }, { key: 'OverrideGameTime', label: t('pcl.auto2.k38b6f73b0a') }],
  };
  return (sections[section.value] || []).filter(item => settingsMap.value.has(item.key));
});
let saveQueue: Promise<unknown> = Promise.resolve();
function save(key: string, value: unknown): Promise<boolean> {
  const operation = saveQueue.then(() => persistSetting(key, value));
  saveQueue = operation.catch(() => {});
  return operation;
}
function sameSettingValue(left: unknown, right: unknown): boolean {
  if (Object.is(left, right)) return true;
  try { return JSON.stringify(left) === JSON.stringify(right); } catch { return String(left) === String(right); }
}
async function persistSetting(key: string, value: unknown): Promise<boolean> {
  const setting = settingsMap.value.get(key);
  if (value === undefined || !setting) return false;
  if (setting.redacted && value === '***') return false;
  error.value = '';
  const normalized = setting.type === 'QString' && typeof value === 'boolean' ? String(value) : value;
  try {
    if (isInstance.value) {
      const gate = Object.entries(overrideGroups).find(([, keys]) => keys.includes(key))?.[0];
      if (gate && !bool(gate)) {
        await props.execute('settings.set', { ...props.scope, key: gate, value: true });
        drafts[gate] = true;
      }
    }
    const applied = await props.execute('settings.set', { ...props.scope, key, value: normalized });
    // The API returns the value held by SettingsObject after conversion and
    // persistence. Verify it before reporting success so a stale backend or a
    // type conversion bug cannot leave the UI claiming that a setting applied.
    if (applied?.key === key && !applied.redacted && !sameSettingValue(applied.value, normalized)) {
      throw new Error(t('pcl.setup.notApplied', { key }));
    }
    if (!setting.redacted) drafts[key] = applied?.key === key ? applied.value : normalized;
    notice.value = t('pcl.auto2.kcb84a961b0');
    window.setTimeout(() => { if (notice.value === t('pcl.auto2.kcb84a961b0')) notice.value = ''; }, 1800);
    emit('changed');
    return true;
  } catch (exception) {
    error.value = message(exception);
    return false;
  }
}
function secretConfigured(key: string): boolean {
  const setting = settingsMap.value.get(key);
  return setting?.redacted ? !setting.isDefault : Boolean(setting?.value);
}
function onMemoryMode(value: string | number | boolean) {
  memoryMode.value = value === 'auto' ? 'auto' : 'custom';
  if (memoryMode.value === 'auto') {
    void save('MinMemAlloc', 512);
    void save('MaxMemAlloc', 4096);
  }
}
function refreshSection(id: string) {
  if (id === 'language') void loadLanguages(true);
  else if (id === 'update') void loadUpdate();
  else if (id === 'misc' && !isInstance.value) void loadProxy();
  else if (id === 'cache' && !isInstance.value) void loadCache(true);
  else if (id === 'network' && !isInstance.value) void loadOnlineBackends(true);
  else emit('changed');
}

async function loadOnlineBackends(force = false) {
  if (onlineBusy.value && !force) return;
  onlineBusy.value = true;
  try {
    const result = await props.execute('multiplayer.backends');
    onlineBackends.value = Array.isArray(result) ? result : (result?.backends || []);
  } catch {
    onlineBackends.value = [];
  } finally {
    onlineBusy.value = false;
  }
}
async function verifyAndTrust() {
  if (!isInstance.value || !props.scope.instance) return;
  error.value = '';
  try {
    await props.execute('instance.verify', { instance: props.scope.instance, trust: true });
    drafts.AssetIntegrityTrusted = true;
    notice.value = t('pcl.auto2.k772d16fa01');
    emit('changed');
  } catch (exception) {
    error.value = message(exception);
  }
}

async function loadCache(force = false) {
  if (cacheBusy.value && !force) return;
  cacheBusy.value = true;
  cacheError.value = '';
  try {
    const [yggdrasilResult, httpResult] = await Promise.allSettled([
      props.execute('yggdrasil.cache.status'),
      props.execute('launcher.cache.status'),
    ]);
    const yggdrasil = yggdrasilResult.status === 'fulfilled' ? yggdrasilResult.value : {};
    const http = httpResult.status === 'fulfilled' ? httpResult.value : {};
    cacheState.value = { ...(yggdrasil || {}), http: http || {} };
    cacheRoutes.value = Array.isArray(yggdrasil?.routes) ? yggdrasil.routes : [];
    if (yggdrasilResult.status === 'rejected' && httpResult.status === 'rejected')
      cacheError.value = message(yggdrasilResult.reason);
    else if (yggdrasilResult.status === 'rejected')
      cacheError.value = t('pcl.auto2.k5a86d98bca');
  } finally {
    cacheBusy.value = false;
  }
}
async function purgeYggdrasilCache() {
  if (!window.confirm(t('pcl.auto2.k8680dc4e80'))) return;
  cacheBusy.value = true;
  try {
    await props.execute('yggdrasil.cache.purge', { confirm: true });
    notice.value = t('pcl.auto2.kd4a34f3220');
    await loadCache(true);
  } catch (exception) {
    cacheError.value = message(exception);
  } finally {
    cacheBusy.value = false;
  }
}
async function clearMetadataCache() {
  if (!window.confirm(t('pcl.auto2.k0c2737f9ba'))) return;
  try {
    await props.execute('launcher.cache.clear', { confirm: true });
    notice.value = t('pcl.auto2.kdc0ba46d10');
    await loadCache(true);
  } catch (exception) {
    cacheError.value = message(exception);
  }
}

async function loadLanguages(force = false) {
  if (languagesLoading.value) return;
  if (languages.value.length && !force) return;
  languagesLoading.value = true;
  error.value = '';
  try {
    const result = await props.execute('language.list');
    languages.value = result?.languages || [];
    languageId.value = result?.selected || '';
    useSystemLocale.value = Boolean(result?.useSystemLocale);
    if (languageId.value) setNeoLocale(localeFromLauncherLanguage(languageId.value));
  } catch (exception) {
    error.value = message(exception);
  } finally {
    languagesLoading.value = false;
  }
}
async function selectLanguage(id: string | number) {
  error.value = '';
  try {
    const result = await props.execute('language.select', { language: String(id) });
    languageId.value = result?.selected || String(id);
    setNeoLocale(localeFromLauncherLanguage(languageId.value));
    notice.value = t('pcl.auto2.k2b01d892c2');
    emit('changed');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function setSystemLocale(value: boolean) {
  useSystemLocale.value = value;
  if (value && typeof navigator !== 'undefined') setNeoLocale(navigator.language);
  else if (languageId.value) setNeoLocale(localeFromLauncherLanguage(languageId.value));
  if (!languageId.value) return;
  try {
    await props.execute('language.select', { language: languageId.value, useSystemLocale: value });
    emit('changed');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function loadRuntime() {
  try {
    runtimeInfo.value = await props.execute('runtime.info');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function loadUpdate() {
  try {
    updateState.value = await props.execute('launcher.update.status');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function configureUpdate(values: Record<string, unknown>) {
  try {
    updateState.value = await props.execute('launcher.update.configure', values);
    emit('changed');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function checkUpdate() {
  if (updateChecking.value) return;
  updateChecking.value = true;
  error.value = '';
  try {
    updateCheck.value = await props.execute('launcher.update.check');
    updateChecked.value = true;
    if (updateCheck.value?.available) notice.value = t('pcl.setup.newVersion', { version: updateCheck.value.versionName || '' });
    else notice.value = t('pcl.auto2.kdb620f1761');
    await loadUpdate();
  } catch (exception) {
    error.value = message(exception);
  } finally {
    updateChecking.value = false;
  }
}
async function applyUpdate() {
  if (!updateCheck.value?.versionTag) return;
  error.value = '';
  try {
    await props.execute('launcher.update.apply', { versionTag: updateCheck.value.versionTag });
    notice.value = t('pcl.auto2.k7d8ec88815');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function loadProxy() {
  try {
    const result = await props.execute('network.proxy.get');
    proxy.type = result.type || 'None';
    proxy.host = result.host || '';
    proxy.port = Number(result.port) || 7890;
    proxy.username = result.username || '';
    proxy.password = result.password || '';
  } catch (exception) {
    error.value = message(exception);
  }
}
async function saveProxy() {
  error.value = '';
  try {
    const result = await props.execute('network.proxy.set', {
      type: proxy.type,
      host: proxy.host,
      port: Number(proxy.port) || 1080,
      username: proxy.username,
      password: proxy.password,
    });
    proxy.type = result.type || proxy.type;
    notice.value = t('pcl.auto2.k7be2e7e740');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function testProxy() {
  proxyTestResult.value = '';
  try {
    const result = await props.execute('network.proxy.test', { url: proxyTestUrl.value, timeoutMs: 10000 });
    proxyTestResult.value = result?.status
      ? t('pcl.setup.connectionSuccessHttp', { status: result.status, time: result.timeMs ?? '—' })
      : t('pcl.setup.connectionSuccess', { value: JSON.stringify(result) });
  } catch (exception) {
    proxyTestResult.value = t('pcl.setup.connectionFailed', { error: message(exception) });
  }
}
async function openUrl(url: string) {
  try {
    await props.execute('desktop.open-url', { url });
  } catch (exception) {
    error.value = message(exception);
  }
}
async function openPath(path: string) {
  try {
    await props.execute('desktop.open-path', { path });
  } catch (exception) {
    error.value = message(exception);
  }
}
async function openLogs() {
  try {
    const info = runtimeInfo.value.dataRoot ? runtimeInfo.value : await props.execute('runtime.info');
    const root = info.dataRoot || info.root;
    if (!root) throw new Error(t('pcl.auto2.k83bd4d1767'));
    await openPath(`${root}/logs`);
  } catch (exception) {
    error.value = message(exception);
  }
}
async function openUpdateLog() {
  try {
    const status = updateState.value.updateLogPath ? updateState.value : await props.execute('launcher.update.status');
    if (status.updateLogPath) await openPath(status.updateLogPath);
    else await openLogs();
  } catch (exception) {
    error.value = message(exception);
  }
}
provide(PclSetupContextKey, {
  t, props, settings: props.settings, scope: props.scope, busy: toRef(props, 'busy'), execute: props.execute,
  drafts, languages, languageId, useSystemLocale, languagesLoading, runtimeInfo, updateState, updateCheck,
  updateChecked, updateChecking, proxy, proxyTestUrl, proxyTestResult, cacheState, cacheRoutes, cacheBusy,
  cacheError, onlineBackends, onlineBusy, isInstance, windowMode, mirrorType, windowOptions, mirrorOptions,
  downloadBackendOptions, modApiMirrorOptions, proxyTypeOptions, consoleColorOptions, consoleFontOptions,
  assetVerificationOptions, assetVerificationDescriptions, languageOptions, pathSettings, toolPathSettings, ariaProxyDisabled, formatBytes,
  applicationVersion, credits, contributors, projectUrl, licenseUrl, docsUrl, bmclApiUrl, mcimUrl,
  maxMem, memoryMode, memoryTotalGiB, memoryUsedGiB, memoryUsedPercent, memoryAllocAfterPercent,
  section, sectionOverrideGroups, error, notice, val, num, bool, draftNumber, draftValue, formatMib, formatSpeed,
  setWindowMode, save, saveMirrorType, savePasteUrl, chooseGlobalDataPacks, chooseDirectory, chooseFilePath,
  secretConfigured, onMemoryMode, refreshSection, verifyAndTrust, loadOnlineBackends, loadCache, purgeYggdrasilCache,
  clearMetadataCache, loadLanguages, selectLanguage, setSystemLocale, loadRuntime, loadUpdate, configureUpdate,
  checkUpdate, applyUpdate, loadProxy, saveProxy, testProxy, openUrl, openPath, openLogs, openUpdateLog,
});</script>

<style>
.pcl-setup-page {
  background: var(--pcl-page-bg);
}

.pcl-setup-stack {
  max-width: 100%;
}

.pcl-form-row--top {
  align-items: flex-start;
  padding-top: 4px;
}

.pcl-form-row__control--column {
  flex-direction: column;
  align-items: stretch;
}

.pcl-check-row {
  flex-wrap: wrap;
  gap: 8px 18px;
}

.pcl-check-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
  gap: 0 18px;
  margin: 4px 0 8px;
}

.pcl-choice-row {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 10px 22px;
  min-height: 28px;
  margin-bottom: 10px;
}

.pcl-memory-live-head {
  display: flex;
  align-items: center;
  gap: 6px;
  margin-top: 4px;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-memory-live-head strong {
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 400;
}

.pcl-memory-live-head em {
  margin: 0 2px;
  font-style: normal;
  opacity: .6;
}

.pcl-memory-live-dot {
  width: 6px;
  height: 6px;
  margin-left: auto;
  border-radius: 50%;
  background: #34d399;
  animation: pcl-memory-pulse 2s ease-out infinite;
}

.pcl-memory-live-label {
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

@keyframes pcl-memory-pulse {
  0% { box-shadow: 0 0 0 0 rgba(52, 211, 153, .5); }
  70% { box-shadow: 0 0 0 5px rgba(52, 211, 153, 0); }
  100% { box-shadow: 0 0 0 0 rgba(52, 211, 153, 0); }
}

.pcl-memory-bar {
  position: relative;
  display: flex;
  width: 100%;
  height: 9px;
  margin: 8px 0 7px;
  overflow: hidden;
  border-radius: 3px;
  background: var(--pcl-c6);
}

.pcl-memory-bar.disabled {
  opacity: .45;
}

.pcl-memory-bar__allocated {
  position: absolute;
  top: 0;
  bottom: 0;
  border-radius: 0 3px 3px 0;
  background: #8fc4ff;
  transition: left .8s cubic-bezier(.16, 1, .3, 1), width .8s cubic-bezier(.16, 1, .3, 1);
}

.pcl-memory-bar__used {
  position: absolute;
  top: 0;
  bottom: 0;
  left: 0;
  border-radius: 3px 0 0 3px;
  background: var(--pcl-c3);
  transition: width .8s cubic-bezier(.16, 1, .3, 1);
}

.pcl-memory-bar__divider {
  position: absolute;
  top: 0;
  bottom: 0;
  width: 2px;
  background: #fff;
  box-shadow: 0 0 0 1px rgba(42, 128, 227, .32);
  transition: left .8s cubic-bezier(.16, 1, .3, 1);
}

.pcl-memory-values {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 12px;
  margin-bottom: 5px;
}

.pcl-memory-values strong {
  color: var(--pcl-text);
  font-size: calc(14px * var(--pcl-font-scale, 1));
  font-weight: 400;
}

.pcl-memory-values em {
  margin: 0 3px;
  color: var(--pcl-muted);
  font-style: normal;
}

.pcl-memory-live-right {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  margin-left: auto;
}

.pcl-memory-legend {
  display: flex;
  justify-content: space-between;
  gap: 12px;
  margin-bottom: 10px;
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  opacity: .72;
}

.pcl-memory-legend b {
  margin-right: 5px;
  font-weight: 400;
  opacity: .7;
}

.pcl-advanced-hint {
  margin-top: 12px;
}

.pcl-console-preview { padding: 12px; overflow: auto; border-radius: 4px; background: var(--pcl-c8); color: var(--pcl-text); }

.pcl-actions {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px;
  margin-top: 14px;
}

.pcl-diagnostic {
  margin-top: 10px;
  padding: 9px 12px;
  border-radius: 3px;
  background: var(--pcl-c7);
  color: var(--pcl-muted);
  font: calc(12px * var(--pcl-font-scale, 1))/1.5 ui-monospace, Consolas, monospace;
  white-space: pre-wrap;
}

.pcl-language-banner {
  position: relative;
  display: flex;
  min-height: 230px;
  margin: 15px 0;
  padding: 25px;
  overflow: hidden;
  border-radius: 5px;
  background: var(--pcl-c7);
}

.pcl-language-banner__earth {
  position: absolute;
  top: -43px;
  right: -25px;
  color: var(--pcl-c5);
  opacity: .62;
}

.pcl-language-banner__copy {
  position: relative;
  z-index: 1;
  max-width: 560px;
  margin-right: 170px;
  color: var(--pcl-text);
}

.pcl-language-banner__title {
  margin-bottom: 10px;
  font-size: calc(22px * var(--pcl-font-scale, 1));
  font-weight: 700;
  line-height: 1.3;
}

.pcl-language-banner__title b {
  font-family: Bahnschrift, "Segoe UI", var(--pcl-font);
  font-size: calc(42px * var(--pcl-font-scale, 1));
  font-weight: 700;
  letter-spacing: -.02em;
}

.pcl-language-banner__copy p {
  margin: 5px 0;
  color: var(--pcl-text);
  font-size: calc(14px * var(--pcl-font-scale, 1));
  line-height: 23px;
  opacity: .82;
}

.pcl-about-head {
  display: flex;
  align-items: center;
  gap: 14px;
  padding: 4px 0 16px;
}

.pcl-about-logo {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 56px;
  height: 56px;
  border-radius: 0;
  background: transparent;
  color: var(--pcl-text);
  font-size: calc(28px * var(--pcl-font-scale, 1));
  font-weight: 800;
}

.pcl-about-logo img {
  width: 100%;
  height: 100%;
  object-fit: contain;
}

.pcl-about-name {
  font-size: calc(20px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-about-name span {
  margin-left: 5px;
  color: var(--pcl-c3);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 500;
  letter-spacing: .08em;
}

.pcl-about-version {
  margin-top: 3px;
  color: var(--pcl-muted);
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-about-grid {
  display: grid;
  grid-template-columns: 90px minmax(0, 1fr);
  gap: 7px 12px;
  padding: 8px 0;
  font-size: calc(13px * var(--pcl-font-scale, 1));
}

.pcl-about-grid span {
  color: var(--pcl-muted);
}

.pcl-about-grid code {
  overflow: hidden;
  color: var(--pcl-text);
  font: calc(12px * var(--pcl-font-scale, 1))/1.5 ui-monospace, Consolas, monospace;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-credit-tiles {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  padding: 4px 0 10px;
}

.pcl-credit-tiles span {
  padding: 5px 10px;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c7);
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-credit-note {
  margin: 0;
  font-size: calc(12px * var(--pcl-font-scale, 1));
  line-height: 1.7;
}

.pcl-update-head {
  display: flex;
  align-items: center;
  gap: 14px;
}

.pcl-update-icon {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 50px;
  height: 50px;
  border-radius: 8px;
  background: var(--pcl-c3);
  color: #fff;
  font-size: calc(24px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-update-name {
  font-size: calc(20px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-update-name span {
  margin-left: 6px;
  color: var(--pcl-c3);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-update-head .pcl-btn {
  margin-left: auto;
}

.pcl-release-notes,
.pcl-log-tail {
  max-height: 260px;
  margin: 14px 0 0;
  padding: 10px 12px;
  overflow: auto;
  border-radius: 3px;
  background: var(--pcl-c8);
  color: var(--pcl-muted);
  font: calc(12px * var(--pcl-font-scale, 1))/1.55 ui-monospace, Consolas, monospace;
  white-space: pre-wrap;
}

.pcl-feedback-copy {
  max-width: 620px;
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  line-height: 1.8;
  opacity: .82;
}

.pcl-log-actions .pcl-btn {
  min-width: 120px;
}

.pcl-appearance-panel .preference-card {
  border: 0;
  border-radius: 5px;
  background: var(--pcl-card);
  box-shadow: var(--pcl-shadow);
}

.pcl-appearance-panel .preference-card h2,
.pcl-appearance-panel .preference-card h3 {
  font-size: calc(13px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

@media (max-width: 900px) {
  .pcl-language-banner__copy {
    margin-right: 60px;
  }
}

.pcl-setup-page.is-embedded {
  display: block;
  height: auto;
  max-width: none;
  margin: 0;
  overflow: visible;
}

.pcl-setup-page.is-embedded .pcl-page-main {
  width: 100%;
  height: auto;
  max-width: none;
  margin: 0;
  padding: 0;
  overflow: visible;
}
</style>



