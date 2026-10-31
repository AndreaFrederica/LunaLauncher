<template>
  <div class="pcl-appearance">
    <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

        <PclAppearanceCard1 />

        <PclAppearanceCard2 />

        <PclAppearanceCard3 />

        <PclAppearanceCard4 />

        <PclAppearanceCard5 />

        <PclAppearanceCard6 />

        <PclAppearanceCard7 />

        <PclAppearanceCard8 />

        <PclAppearanceCard9 />

        <PclAppearanceCard10 />

        <PclAppearanceCard11 />
  </div>
</template>

<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { computed, provide, ref } from 'vue';
import lunaLogo from '../../../../assets/luna-launcher.png';
import PclCard from '../../components/PclCard.vue';
import PclButton from '../../components/PclButton.vue';
import PclHint from '../../components/PclHint.vue';
import PclSelect from '../../components/PclSelect.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclColorPicker from '../../components/PclColorPicker.vue';
import PclInput from '../../components/PclInput.vue';
import PclNumberInput from '../../components/PclNumberInput.vue';
import { choosePclMusic, pclMusic, removePclMusic, setPclMusicVolume, togglePclMusic } from '../../composables/usePclMusic';
import { applyPclStylePreset, neoUiPreferences, persistNeoUiPreferences } from '../../../../neo-ui-preferences';
import { launcher } from '../../../../boot/launcher-api';
import { PclAppearanceContextKey } from './PclAppearanceContext';
import PclAppearanceCard1 from './PclAppearanceCard1.vue';
import PclAppearanceCard10 from './PclAppearanceCard10.vue';
import PclAppearanceCard11 from './PclAppearanceCard11.vue';
import PclAppearanceCard2 from './PclAppearanceCard2.vue';
import PclAppearanceCard3 from './PclAppearanceCard3.vue';
import PclAppearanceCard4 from './PclAppearanceCard4.vue';
import PclAppearanceCard5 from './PclAppearanceCard5.vue';
import PclAppearanceCard6 from './PclAppearanceCard6.vue';
import PclAppearanceCard7 from './PclAppearanceCard7.vue';
import PclAppearanceCard8 from './PclAppearanceCard8.vue';
import PclAppearanceCard9 from './PclAppearanceCard9.vue';

import {
  hiddenPclFeatures as hiddenFeatures,
  pclDownloadFeatures as downloadFeatures,
  pclSetupFeatures as setupFeatures,
  resetPclFeatures,
  setPclFeatureHidden,
} from '../../composables/usePclFeatures';

const preferences = neoUiPreferences;
const error = ref('');
const backgroundFile = ref<HTMLInputElement | null>(null);
const musicFile = ref<HTMLInputElement | null>(null);

function chooseMusic() {
  musicFile.value?.click();
}

function onMusicFile(event: Event) {
  const input = event.target as HTMLInputElement;
  const file = input.files?.[0];
  input.value = '';
  if (file) void choosePclMusic(file);
}

function onMusicVolume(event: Event) {
  setPclMusicVolume(Number((event.target as HTMLInputElement).value));
}

function chooseBackground() {
  backgroundFile.value?.click();
}

function onBackgroundFile(event: Event) {
  const input = event.target as HTMLInputElement;
  const file = input.files?.[0];
  input.value = '';
  if (!file) return;
  if (file.size > 1_400_000) {
    error.value = t('pcl.auto2.kd2a56a402e');
    return;
  }
  const reader = new FileReader();
  reader.onload = () => {
    preferences.pclBackgroundImage = String(reader.result || '');
    preferences.pclBackgroundImageName = file.name;
    error.value = '';
    persistNeoUiPreferences();
  };
  reader.onerror = () => { error.value = t('pcl.auto2.k25ff2e16db'); };
  reader.readAsDataURL(file);
}

const homeOrderItems = computed(() => {
  const items = [
    { key: 'hint', label: t('pcl.auto2.k096047b598') },
    ...preferences.pclHomeCards.map(card => ({ key: `custom:${card.id}`, label: card.title || t('pcl.auto2.kced8d0f34e') })),
    { key: 'log', label: t('pcl.auto2.k8ad868abbc') },
    { key: 'instances', label: t('pcl.auto2.k9d2dba1a26') },
    { key: 'servers', label: t('pcl.auto2.kb2cfd67505') },
  ];
  const valid = new Set(items.map(item => item.key));
  const ordered = preferences.pclHomeCardOrder.filter(key => valid.has(key));
  for (const item of items) if (!ordered.includes(item.key)) ordered.push(item.key);
  return ordered.map(key => items.find(item => item.key === key)!).filter(Boolean);
});
function moveHomeOrder(index: number, delta: number) {
  const keys = homeOrderItems.value.map(item => item.key);
  const target = index + delta;
  if (target < 0 || target >= keys.length) return;
  [keys[index], keys[target]] = [keys[target], keys[index]];
  preferences.pclHomeCardOrder = keys;
  persistNeoUiPreferences();
}
function selectPclPreset(value: string) {
  applyPclStylePreset(value === 'pcl' ? 'pcl' : 'luna-neo');
}
function addHomeCard() {
  preferences.pclHomeCards.push({
    id: `${Date.now()}-${Math.random().toString(36).slice(2, 7)}`,
    title: t('pcl.auto2.kef4fd94620'),
    content: '',
  });
  persistNeoUiPreferences();
}

function removeHomeCard(id: string) {
  preferences.pclHomeCards = preferences.pclHomeCards.filter(card => card.id !== id);
  persistNeoUiPreferences();
}

function clearBackground() {
  preferences.pclBackgroundImage = '';
  preferences.pclBackgroundImageName = '';
  persistNeoUiPreferences();
}
const pclStylePresetOptions = [
  { label: t('pcl.auto2.ka6dc467052'), value: 'luna-neo' },
  { label: 'PCL', value: 'pcl' },
];
const navLayoutOptions = [
  { label: t('pcl.auto2.ka4421ee0b9'), value: 'pcl' },
  { label: 'MMC Classic', value: 'mmc-classic' },
];
const classicSideOptions = [
  { label: t('pcl.auto2.k440495fc2d'), value: 'left' },
  { label: t('pcl.auto2.k1e4e7a177d'), value: 'right' },
];
const classicIconOptions = [
  { label: '64 px', value: 64 },
  { label: '80 px', value: 80 },
  { label: '96 px', value: 96 },
  { label: '120 px', value: 120 },
  { label: '160 px', value: 160 },
];
const shellOptions = [
  { label: '忘却的旋律', value: 'melody' },
  { label: t('pcl.auto2.k67cf30199a'), value: 'pcl' },
  { label: t('pcl.auto2.k453b088186'), value: 'neo' },
];
const themeOptions = [
  { label: t('pcl.auto2.k30b2c979ac'), value: 'dark' },
  { label: t('pcl.auto2.k80ec9e2b1b'), value: 'light' },
  { label: t('pcl.auto2.kf4bbd91f79'), value: 'system' },
];
const densityOptions = [
  { label: t('pcl.auto2.kb97c3bc09e'), value: 'comfortable' },
  { label: t('pcl.auto2.kc837edc258'), value: 'compact' },
];
const backgroundOptions = [
  { label: t('pcl.auto2.k6fd96f07e8'), value: 'gradient' },
  { label: t('pcl.auto2.ke139ab0c18'), value: 'solid' },
];
const loadingAnimationOptions = [
  { label: t('pcl.auto2.kb71ec0b422'), value: 'luna-spinner' },
  { label: t('pcl.auto2.k490a4502f2'), value: 'pcl-pickaxe' },
  { label: t('pcl.auto2.k722c032691'), value: 'pcl-bar' },
];
const instanceWidthOptions = [
  { label: '60%', value: 60 },
  { label: '70%', value: 70 },
  { label: '80%', value: 80 },
  { label: '90%', value: 90 },
  { label: '100%', value: 100 },
];
const instanceLayoutOptions = [
  { label: t('pcl.auto2.kcd1bbb1239'), value: 'grid' },
  { label: t('pcl.auto2.ke94a88a32b'), value: 'list' },
];
const instanceGroupingOptions = [
  { label: t('pcl.auto2.kbf3327d8de'), value: 'flat' },
  { label: t('pcl.auto2.k44c1c6bab7'), value: 'grouped' },
];
const instanceSortOptions = [
  { label: t('pcl.auto2.kc8b7b63a05'), value: 'manual' },
  { label: t('pcl.auto2.k8065cff0a2'), value: 'name' },
  { label: t('pcl.auto2.k265250b276'), value: 'version' },
];
const instanceColumnOptions = [
  { label: t('pcl.auto2.k2907c13b04'), value: 'auto' },
  { label: t('pcl.auto2.k620e6129cf'), value: 1 },
  { label: t('pcl.auto2.k1e7e0fdbaa'), value: 2 },
  { label: t('pcl.auto2.kf32016d001'), value: 3 },
  { label: t('pcl.auto2.k2a5c49468f'), value: 4 },
  { label: t('pcl.auto2.k57d6fb6ef9'), value: 5 },
  { label: t('pcl.auto2.kf90d03d8d0'), value: 6 },
  { label: t('pcl.auto2.ka03a27317c'), value: 7 },
  { label: t('pcl.auto2.k5a2ec4199e'), value: 8 },
  { label: t('pcl.auto2.k68c3715fb2'), value: 10 },
];
const instanceTextPositionOptions = [
  { label: t('pcl.auto2.k381c24c2e7'), value: 'beside' },
  { label: t('pcl.auto2.kb8b901d2ee'), value: 'below' },
];
const instanceRowOptions = [
  { label: t('pcl.auto2.kbf3fa26ef0'), value: 2 },
  { label: t('pcl.auto2.kd60a2cddbc'), value: 3 },
  { label: t('pcl.auto2.kb9af82b78e'), value: 4 },
  { label: t('pcl.auto2.k5fc5b3f3a8'), value: 5 },
  { label: t('pcl.auto2.k12eee8ee0d'), value: 6 },
  { label: t('pcl.auto2.k0b6a6cd748'), value: 8 },
];
const instanceFontOptions = [
  { label: '10 px', value: 10 },
  { label: '11 px', value: 11 },
  { label: '12 px', value: 12 },
  { label: '13 px', value: 13 },
  { label: t('pcl.auto2.kb52c0b7086'), value: 14 },
  { label: '15 px', value: 15 },
  { label: '16 px', value: 16 },
  { label: '18 px', value: 18 },
];
const baseFontOptions = [
  { label: '11 px', value: 11 },
  { label: '12 px', value: 12 },
  { label: '13 px', value: 13 },
  { label: t('pcl.auto2.kb52c0b7086'), value: 14 },
  { label: '15 px', value: 15 },
  { label: '16 px', value: 16 },
  { label: '18 px', value: 18 },
];
const fontOptions = [
  { label: t('pcl.auto2.kaab0c72d2d'), value: 'system' },
  { label: t('pcl.auto2.k85275a6427'), value: 'yahei' },
  { label: t('pcl.auto2.k7791b0d498'), value: 'dengxian' },
  { label: t('pcl.auto2.k24e636c047'), value: 'simsun' },
];
async function openWebUiManager() {
  error.value = '';
  try {
    await launcher.openWebUiManager();
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}
function reset() {
  Object.assign(neoUiPreferences, {
    shell: 'pcl',
    theme: 'dark',
    accent: '#2a80e3',
    density: 'comfortable',
    background: 'gradient',
    animations: true,
    pclFont: 'system',
    pclStylePreset: 'luna-neo',
    pclLoadingAnimation: 'luna-spinner',
    pclAutoRefreshOnPageOpen: true,
    pclBaseFontSize: 14,
    pclPageNavWidth: 220,
    pclNavLayout: 'mmc-classic',
    pclClassicSide: 'right',
    pclClassicIconSize: 96,
    pclLaunchAnimation: false,
    pclClearLaunchLogOnStart: true,
    pclLaunchLogAutoScroll: true,
    pclEditCurrentInWindow: true,
    pclFloatTasks: false,
    pclFloatLog: false,
    pclFloatRefresh: false,
    pclFloatMusic: false,
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
    pclHomeCards: [{ id: 'welcome', title: t('pcl.auto2.ka5d9fdec5a'), content: t('pcl.auto2.k5d577eb245') }],
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
  });
  persistNeoUiPreferences();
}

provide(PclAppearanceContextKey, { t, lunaLogo, preferences, error, backgroundFile, musicFile, pclMusic, homeOrderItems, pclStylePresetOptions, navLayoutOptions, classicSideOptions, classicIconOptions, shellOptions, themeOptions, densityOptions, backgroundOptions, loadingAnimationOptions, instanceWidthOptions, instanceLayoutOptions, instanceGroupingOptions, instanceSortOptions, instanceColumnOptions, instanceTextPositionOptions, instanceRowOptions, instanceFontOptions, baseFontOptions, fontOptions, chooseMusic, onMusicFile, onMusicVolume, chooseBackground, onBackgroundFile, moveHomeOrder, selectPclPreset, addHomeCard, removeHomeCard, clearBackground, openWebUiManager, reset, togglePclMusic, removePclMusic, choosePclMusic, setPclMusicVolume, resetPclFeatures, downloadFeatures, setupFeatures, hiddenFeatures, setPclFeatureHidden });
</script>

<style>
.pcl-appearance {
  max-width: 100%;
}

.pcl-hidden-file {
  display: none;
}

.pcl-home-editor {
  display: flex;
  flex-direction: column;
  gap: 7px;
  margin-bottom: 10px;
  padding: 9px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-half-white);
}

.pcl-home-editor__head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-background-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 7px;
}

.pcl-background-preview {
  height: 120px;
  margin: 4px 0 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background-position: center;
  background-size: cover;
  background-repeat: no-repeat;
}

.pcl-range {
  width: 100%;
  accent-color: var(--pcl-c3);
}

.pcl-appearance-copy {
  margin: 0 0 8px;
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  line-height: 1.8;
}

.pcl-appearance-preview {
  margin: 16px 0 4px;
  overflow: hidden;
  border: 1px solid var(--pcl-c5);
  border-radius: 5px;
  background: var(--pcl-c8);
}

.pcl-appearance-preview__bar {
  position: relative;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  height: 48px;
  padding: 0 8px 0 19px;
  background: var(--pcl-c3);
  color: #fff;
}

.pcl-appearance-preview__brand {
  display: inline-flex;
  align-items: center;
  gap: 0;
  min-width: 0;
  flex: 0 0 auto;
}

.pcl-appearance-preview__brand strong {
  overflow: hidden;
  max-width: 160px;
  font-size: calc(14px * var(--pcl-font-scale, 1));
  font-weight: 700;
  letter-spacing: .01em;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-appearance-preview__brand small {
  margin-left: 7px;
  color: rgba(255, 255, 255, .72);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  font-weight: 700;
  letter-spacing: .14em;
  white-space: nowrap;
}

.pcl-appearance-preview__logo {
  display: block;
  align-items: center;
  justify-content: center;
  width: 24px;
  height: 24px;
  margin-right: 8px;
  border-radius: 4px;
  flex: 0 0 auto;
  object-fit: contain;
}

.pcl-appearance-preview__navs {
  position: absolute;
  left: 50%;
  top: 0;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 0;
  min-width: 0;
  flex: 1 1 auto;
  height: 48px;
  transform: translateX(-50%);
}

.pcl-appearance-preview__nav {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 7px;
  height: 27px;
  margin: 0 5px;
  padding: 0 12px;
  border-radius: 13.5px;
  color: #fff;
  font-size: calc(13px * var(--pcl-font-scale, 1));
  white-space: nowrap;
}

.pcl-appearance-preview__nav .pcl-icon {
  width: 13px;
  height: 13px;
}

.pcl-appearance-preview__nav.active {
  background: #fff;
  color: var(--pcl-c3);
}

.pcl-appearance-preview__window-actions {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  flex: 0 0 auto;
}

.pcl-appearance-preview__window-actions i {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 28px;
  height: 28px;
  color: #fff;
  font: 400 18px/1 "Segoe UI", sans-serif;
  font-style: normal;
}

.pcl-appearance-preview__window-actions .close {
  margin-left: 4px;
}

.pcl-appearance-preview__body {
  display: flex;
  min-height: 92px;
}

.pcl-appearance-preview__side {
  display: flex;
  flex-direction: column;
  gap: 9px;
  width: 92px;
  padding: 12px;
  background: var(--pcl-nav-bg);
}

.pcl-appearance-preview__side i {
  height: 6px;
  border-radius: 3px;
  background: var(--pcl-c5);
  opacity: .55;
}

.pcl-appearance-preview__card {
  display: flex;
  flex: 1;
  flex-direction: column;
  gap: 8px;
  margin: 12px;
  padding: 12px;
  border-radius: 4px;
  background: var(--pcl-card);
}

.pcl-appearance-preview__card b {
  width: 90px;
  height: 7px;
  border-radius: 3px;
  background: var(--pcl-c3);
}

.pcl-appearance-preview__card span {
  height: 5px;
  border-radius: 3px;
  background: var(--pcl-c5);
  opacity: .55;
}

.pcl-appearance-preview.light {
  background: #eef4fb;
}

.pcl-appearance-preview.light .pcl-appearance-preview__side {
  background: #f7fafd;
}

.pcl-hide-groups {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 18px 28px;
  margin-top: 4px;
}

.pcl-hide-group {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  min-width: 0;
  padding: 0 0 8px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 5px;
  background: color-mix(in srgb, var(--pcl-card) 72%, transparent);
}

.pcl-hide-group h4 {
  grid-column: 1 / -1;
  margin: 0;
  padding: 10px 12px 8px;
  border-bottom: 1px solid var(--pcl-card-border);
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 700;
  opacity: .72;
}

.pcl-hide-group > .pcl-check {
  display: grid;
  grid-template-columns: 18px minmax(0, 1fr);
  align-items: center;
  min-width: 0;
  min-height: 32px;
  margin: 0;
  padding: 5px 10px;
  border-bottom: 1px solid color-mix(in srgb, var(--pcl-card-border) 55%, transparent);
}

.pcl-hide-group > .pcl-check:nth-last-child(-n + 2) {
  border-bottom: 0;
}

.pcl-hide-group > .pcl-check:hover {
  background: color-mix(in srgb, var(--pcl-c3) 10%, transparent);
}

.pcl-hide-group .pcl-check {
  margin-bottom: 0;
}

.pcl-hide-group .pcl-check__label {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-font-sample {
  display: flex;
  flex-direction: column;
  gap: 4px;
  margin-top: 12px;
  padding: 12px 14px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c7);
  color: var(--pcl-text);
  font-family: var(--pcl-font);
}

.pcl-font-sample strong {
  font-size: calc(15px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-font-sample span {
  font-size: calc(13px * var(--pcl-font-scale, 1));
}

.pcl-font-sample small {
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-font-sample.serif {
  font-family: SimSun, "Songti SC", serif;
}

@media (max-width: 720px) {
  .pcl-hide-groups {
    grid-template-columns: 1fr;
  }
}

@media (min-width: 721px) and (max-width: 1050px) {
  .pcl-hide-group {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (max-width: 520px) {
  .pcl-hide-group {
    grid-template-columns: 1fr;
  }
}

.pcl-accent-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 18px;
  margin-bottom: 12px;
}

.pcl-accent-row > div:first-child {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.pcl-accent-row strong {
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-accent-row small {
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-accent-preview {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 10px;
  border: 1px solid var(--pcl-border);
  border-radius: 5px;
  background: var(--pcl-c8);
}

.pcl-accent-preview__bar {
  display: block;
  width: 42px;
  height: 5px;
  border-radius: 3px;
  background: var(--pcl-c3);
}

.pcl-accent-preview__button {
  padding: 4px 9px;
  border-radius: 4px;
  background: var(--pcl-c3);
  color: #fff;
  font-size: calc(9px * var(--pcl-font-scale, 1));
}

.pcl-accent-preview__dot {
  display: block;
  width: 20px;
  height: 20px;
  border: 2px solid var(--pcl-c3);
  border-radius: 50%;
  box-shadow: inset 0 0 0 4px var(--pcl-c8);
  background: var(--pcl-c3);
}

@media (max-width: 620px) {
  .pcl-accent-row {
    align-items: flex-start;
    flex-direction: column;
  }
}

.pcl-home-order {
  display: flex;
  flex-direction: column;
  gap: 3px;
  margin: 10px 0 14px;
  padding: 9px 10px;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c8);
}

.pcl-home-order__title {
  margin-bottom: 4px;
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-home-order__row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  min-height: 24px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}
</style>
