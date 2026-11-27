<template>
  <div class="system-bar" :class="{ inner: isPclInner }" role="toolbar" :aria-label="t('xmcl.system.toolbar')">
    <button class="system-bar-back" :aria-label="t('xmcl.system.refreshInstances')" @click="emit('refresh')"><span class="material-icons">arrow_back</span></button><button v-if="isPclInner" class="system-bar-inner-back" :aria-label="t('xmcl.system.backToLaunch')" @click="emit('navigate', 'instances')"><span class="material-icons">arrow_back</span></button>
    <div class="system-bar-drag" data-tauri-drag-region @mousedown="startDragging">
      <strong v-if="isPclInner" class="system-bar-inner-title">{{ instanceName || t('xmcl.system.instanceManagement') }}</strong>
      <span class="system-bar-brand">
        <img v-if="isPclShell && neoUiPreferences.pclTitleIcon" class="system-bar-brand-icon" :src="lunaLogo" alt="" aria-hidden="true" />
        <strong class="system-bar-title-default">{{ neoUiPreferences.pclTitleText || 'Luna Launcher' }}</strong>
        <strong class="system-bar-title-pcl">{{ neoUiPreferences.pclTitleText || 'LunaLauncher' }}</strong>
      </span>
      <small class="system-bar-title-default-subtitle">NEO UI</small>
      <small class="system-bar-title-pcl-subtitle">NEOUI</small>
      <span v-if="showVersion && launcherVersion" class="system-bar-version">{{ launcherVersion }}</span>
    </div>
    <nav ref="pclTabsRef" class="system-bar-pcl-tabs" :aria-label="t('xmcl.system.mainNavigation')">
      <span class="system-bar-pcl-tabs__indicator" aria-hidden="true" :style="pclTabIndicatorStyle" />
      <button :ref="el => setTabRef('home', el)" :aria-label="t('xmcl.system.home')" :title="t('xmcl.system.home')" :class="{ active: activePclTab === 'home' }" @click="emit('navigate', 'instances')"><PclIcon name="layout-dashboard" :size="16" /><span v-if="neoUiPreferences.pclMainNavigationText">{{ t('xmcl.system.home') }}</span></button>
      <button :ref="el => setTabRef('instance', el)" :aria-label="t('xmcl.system.instance')" :class="{ active: activePclTab === 'instance' }" :title="`${t('xmcl.system.instance')}：${instanceName || t('xmcl.system.noInstance')}`" :disabled="busy" @click="emit('navigate', 'manage')"><PclIcon name="box" :size="16" /><span v-if="neoUiPreferences.pclMainNavigationText">{{ t('xmcl.system.instance') }}</span></button>
      <button :ref="el => setTabRef('download', el)" :aria-label="t('xmcl.system.download')" :title="t('xmcl.system.download')" :class="{ active: activePclTab === 'download' }" @click="emit('navigate', 'pcl-download')"><PclIcon name="download" :size="16" /><span v-if="neoUiPreferences.pclMainNavigationText">{{ t('xmcl.system.download') }}</span></button>
      <button :ref="el => setTabRef('settings', el)" :aria-label="t('xmcl.system.settings')" :title="t('xmcl.system.settings')" :class="{ active: activePclTab === 'settings' }" @click="emit('navigate', 'pcl-settings')"><PclIcon name="settings" :size="16" /><span v-if="neoUiPreferences.pclMainNavigationText">{{ t('xmcl.system.settings') }}</span></button>
      <button :ref="el => setTabRef('tools', el)" :aria-label="t('xmcl.system.tools')" :title="t('xmcl.system.tools')" :class="{ active: activePclTab === 'tools' }" @click="emit('navigate', 'pcl-tools')"><PclIcon name="wrench" :size="16" /><span v-if="neoUiPreferences.pclMainNavigationText">{{ t('xmcl.system.tools') }}</span></button>
    </nav>
    <div class="system-bar-spacer" data-tauri-drag-region @mousedown="startDragging" />
    <button class="system-bar-badge" :aria-label="t('xmcl.system.webManager')" :title="t('xmcl.system.webManagerTitle')" @click="openWebUiManager"><span class="material-icons">web</span></button>
    <button class="system-bar-badge" aria-label="NeoUI Runtime Debugger" title="NeoUI Runtime Debugger" @click="openRuntimeDebug"><span class="material-icons">bug_report</span></button>
    <button class="system-bar-badge" :aria-label="t('xmcl.system.search')" @click="emit('search')"><span class="material-icons">search</span></button>
    <button class="system-bar-badge" :aria-label="t('xmcl.system.tasks')" @click="emit('tasks')"><span class="material-icons">assignment</span><i v-if="busy" /></button>
    <span class="system-bar-status"><span class="status-dot" :class="{ connected }" />{{ connected ? t('xmcl.system.connected') : t('xmcl.system.disconnected') }}</span>
    <button class="system-bar-badge" :disabled="busy" :aria-label="t('common.refresh')" @click="emit('refresh')"><span class="material-icons">refresh</span></button>
    <button class="system-bar-window" :aria-label="t('xmcl.system.minimize')" @click="minimize"><span class="material-icons">remove</span></button>
    <button class="system-bar-window" :aria-label="t('xmcl.system.maximize')" @click="toggleMaximize"><span class="material-icons">crop_square</span></button>
    <button class="system-bar-window close" :aria-label="t('xmcl.system.close')" @click="closeWindow"><span class="material-icons">close</span></button>
  </div>
</template>
<script setup lang="ts">
import { getCurrentWindow } from '@tauri-apps/api/window';
import PclIcon from '../../pcl/components/PclIcon.vue';
import { launcher } from '../../../boot/launcher-api';
import { invoke, isTauri } from '@tauri-apps/api/core';
import { useQuasar } from 'quasar';
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { neoUiPreferences } from '../../../neo-ui-preferences';
import lunaLogo from '../../../assets/luna-launcher.png';
import { showPclToast } from '../../pcl/composables/usePclToast';
const quasar = useQuasar();
const { t } = useI18n();
async function openWebUiManager() {
  try { await launcher.openWebUiManager(); }
  catch (e: any) {
    if (neoUiPreferences.shell === 'pcl') showPclToast({ type: 'error', message: e.message || String(e), timeout: 4000 });
    else quasar.notify({ type: 'negative', message: e.message || String(e), timeout: 4000 });
  }
}
async function openRuntimeDebug() {
  if (!isTauri()) return;
  try { await invoke('open_runtime_debug'); }
  catch (e: any) { quasar.notify({ type: 'negative', message: e.message || String(e), timeout: 4000 }); }
}
const props = defineProps<{ connected: boolean; busy: boolean; page?: string; instanceName?: string }>();
const isPclShell = computed(() => neoUiPreferences.shell === 'pcl');
const showVersion = computed(() => isPclShell.value && neoUiPreferences.pclTitleVersion);
const launcherVersion = ref('');
watch([showVersion, () => props.connected], async ([visible, connected]) => {
  if (!visible || !connected || launcherVersion.value) return;
  try {
    const { data } = await launcher.execute<{ applicationVersion?: string; version?: string }>('runtime.info');
    launcherVersion.value = data?.applicationVersion || data?.version || '';
  } catch { /* The title bar remains usable if runtime information is unavailable. */ }
}, { immediate: true });
const page = computed(() => props.page || '');
const isPclInner = computed(() => !isPclShell.value && page.value === 'manage');
const activePclTab = computed(() => {
  if (page.value === 'manage') return 'instance';
  if (page.value === 'store' || page.value === 'create' || page.value === 'pcl-download') return 'download';
  if (page.value === 'settings' || page.value === 'pcl-settings') return 'settings';
  if (page.value === 'pcl-tools') return 'tools';
  return 'home';
});
const pclTabsRef = ref<HTMLElement | null>(null);
const tabRefs = new Map<string, HTMLElement>();
const indicator = ref({ left: 0, top: 0, width: 0, height: 0, ready: false });
let tabsResizeObserver: ResizeObserver | undefined;
function setTabRef(id: string, element: unknown) {
  if (element instanceof HTMLElement) tabRefs.set(id, element);
  else tabRefs.delete(id);
}
function updatePclTabIndicator() {
  void nextTick(() => {
    const nav = pclTabsRef.value;
    const tab = tabRefs.get(activePclTab.value);
    if (!nav || !tab) return;
    const navRect = nav.getBoundingClientRect();
    const tabRect = tab.getBoundingClientRect();
    indicator.value = {
      left: tabRect.left - navRect.left - nav.clientLeft + nav.scrollLeft,
      top: tabRect.top - navRect.top - nav.clientTop + nav.scrollTop,
      width: tabRect.width,
      height: tabRect.height,
      ready: tabRect.width > 0 && tabRect.height > 0,
    };
  });
}
const pclTabIndicatorStyle = computed(() => ({
  width: `${indicator.value.width}px`,
  height: `${indicator.value.height}px`,
  transform: `translate(${indicator.value.left}px, ${indicator.value.top}px)`,
  opacity: indicator.value.ready ? '1' : '0',
}));
function onPclTabsResize() { updatePclTabIndicator(); }
watch(activePclTab, updatePclTabIndicator, { flush: 'post' });
watch(() => neoUiPreferences.shell, updatePclTabIndicator, { flush: 'post' });
watch(() => neoUiPreferences.pclMainNavigationText, updatePclTabIndicator, { flush: 'post' });
watch(() => neoUiPreferences.pclVerticalNavigation, updatePclTabIndicator, { flush: 'post' });
onMounted(() => {
  updatePclTabIndicator();
  tabsResizeObserver = new ResizeObserver(updatePclTabIndicator);
  if (pclTabsRef.value) tabsResizeObserver.observe(pclTabsRef.value);
  tabRefs.forEach(tab => tabsResizeObserver?.observe(tab));
  window.addEventListener('resize', onPclTabsResize);
});
onBeforeUnmount(() => {
  tabsResizeObserver?.disconnect();
  window.removeEventListener('resize', onPclTabsResize);
});
const emit = defineEmits<{ refresh: []; tasks: []; search: []; navigate: [page: string] }>();
async function minimize() { try { await getCurrentWindow().minimize(); } catch { /* Web preview has no native window. */ } }
async function toggleMaximize() { try { const window = getCurrentWindow(); if (await window.isMaximized()) await window.unmaximize(); else await window.maximize(); } catch { /* Web preview has no native window. */ } }
async function closeWindow() { try { await getCurrentWindow().close(); } catch { /* Web preview has no native window. */ } }
async function startDragging(event: MouseEvent) {
  if (event.button !== 0) return;
  try { await getCurrentWindow().startDragging(); } catch { /* Browser preview has no native window. */ }
}
</script>
<style scoped>
.system-bar-version { margin-inline-start: 8px; font-size: 11px; white-space: nowrap; }
.system-bar{position:fixed;top:0;left:0;right:0;height:38px;z-index:100;display:flex;align-items:center;gap:3px;padding:0 8px;background:rgba(17,17,22,.86);backdrop-filter:blur(18px);color:rgba(255,255,255,.7);font-size:12px;box-shadow:0 1px 0 rgba(255,255,255,.06)}.system-bar-drag{height:38px;display:flex;align-items:center;gap:8px;padding:0 12px;flex:0 0 auto;-webkit-app-region:drag;user-select:none}.system-bar-drag strong{color:#fff;font-size:12px;letter-spacing:.02em}.system-bar-brand{display:inline-flex;align-items:center;gap:8px}.system-bar-brand-icon{display:block;width:22px;height:22px;flex:0 0 auto;object-fit:cover;border-radius:4px}.system-bar-title-pcl,.system-bar-pcl-tabs,.system-bar-title-pcl-subtitle{display:none}.system-bar-title-default-subtitle{display:inline}.system-bar-drag small{color:#818cf8;font-size:9px;letter-spacing:.14em;font-weight:700}.system-bar-spacer{height:38px;flex:1;-webkit-app-region:drag}.system-bar-back,.system-bar-badge,.system-bar-window{height:38px;min-width:32px;padding:0 8px;display:inline-flex;align-items:center;justify-content:center;border:0;background:transparent;color:inherit;cursor:pointer;-webkit-app-region:no-drag}.system-bar-back:hover,.system-bar-badge:hover,.system-bar-window:hover{background:rgba(255,255,255,.12);color:#fff}.system-bar-window.close:hover{background:#c42b3c;color:#fff}.system-bar-badge{position:relative}.system-bar-badge i{position:absolute;right:4px;top:7px;width:5px;height:5px;border-radius:50%;background:#fbc02d}.system-bar-status{display:flex;align-items:center;gap:5px;margin:0 4px;color:rgba(255,255,255,.55);-webkit-app-region:no-drag}.status-dot{width:7px;height:7px;border-radius:50%;background:#777}.status-dot.connected{background:#4caf50}.material-icons{font-size:18px;line-height:1}
</style>
