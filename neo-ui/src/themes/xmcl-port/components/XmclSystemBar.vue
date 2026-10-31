<template>
  <div class="system-bar" :class="{ inner: isPclInner }" role="toolbar" :aria-label="t('xmcl.system.toolbar')">
    <button class="system-bar-back" :aria-label="t('xmcl.system.refreshInstances')" @click="emit('refresh')"><span class="material-icons">arrow_back</span></button><button v-if="isPclInner" class="system-bar-inner-back" :aria-label="t('xmcl.system.backToLaunch')" @click="emit('navigate', 'instances')"><span class="material-icons">arrow_back</span></button>
    <div class="system-bar-drag" data-tauri-drag-region @mousedown="startDragging"><strong class="system-bar-inner-title" v-if="isPclInner">{{ instanceName || t('xmcl.system.instanceManagement') }}</strong><span class="system-bar-brand"><img v-if="isPclShell && neoUiPreferences.pclTitleIcon" class="system-bar-brand-icon" :src="lunaLogo" alt="" aria-hidden="true" /><strong class="system-bar-title-default">Luna Launcher</strong><strong class="system-bar-title-pcl">LunaLauncher</strong></span><small class="system-bar-title-default-subtitle">NEO UI</small><small class="system-bar-title-pcl-subtitle">NEOUI</small></div>
    <nav ref="pclTabsRef" class="system-bar-pcl-tabs" :aria-label="t('xmcl.system.mainNavigation')">
      <span class="system-bar-pcl-tabs__indicator" aria-hidden="true" :style="pclTabIndicatorStyle" />
      <button :ref="el => setTabRef('launch', el)" :class="{ active: activePclTab === 'launch' }" @click="emit('navigate', 'instances')"><PclIcon name="play" :size="16" />{{ t('xmcl.system.launch') }}</button>
      <button :ref="el => setTabRef('download', el)" :class="{ active: activePclTab === 'download' }" @click="emit('navigate', 'pcl-download')"><PclIcon name="download" :size="16" />{{ t('xmcl.system.download') }}</button>
      <button :ref="el => setTabRef('settings', el)" :class="{ active: activePclTab === 'settings' }" @click="emit('navigate', 'pcl-settings')"><PclIcon name="settings" :size="16" />{{ t('xmcl.system.settings') }}</button>
      <button :ref="el => setTabRef('tools', el)" :class="{ active: activePclTab === 'tools' }" @click="emit('navigate', 'pcl-tools')"><PclIcon name="wrench" :size="16" />{{ t('xmcl.system.tools') }}</button>
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
const page = computed(() => props.page || '');
const isPclInner = computed(() => page.value === 'manage');
const activePclTab = computed(() => {
  if (page.value === 'store' || page.value === 'create' || page.value === 'pcl-download') return 'download';
  if (page.value === 'settings' || page.value === 'pcl-settings') return 'settings';
  if (page.value === 'pcl-tools') return 'tools';
  return 'launch';
});
const pclTabsRef = ref<HTMLElement | null>(null);
const tabRefs = new Map<string, HTMLElement>();
const indicator = ref({ left: 0, width: 0, ready: false });
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
    indicator.value = { left: tabRect.left - navRect.left, width: tabRect.width, ready: true };
  });
}
const pclTabIndicatorStyle = computed(() => ({
  width: `${indicator.value.width}px`,
  transform: `translateX(${indicator.value.left}px)`,
  opacity: indicator.value.ready ? '1' : '0',
}));
function onPclTabsResize() { updatePclTabIndicator(); }
watch(activePclTab, updatePclTabIndicator, { flush: 'post' });
watch(() => neoUiPreferences.shell, updatePclTabIndicator, { flush: 'post' });
onMounted(() => {
  updatePclTabIndicator();
  window.addEventListener('resize', onPclTabsResize);
});
onBeforeUnmount(() => window.removeEventListener('resize', onPclTabsResize));
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
.system-bar{position:fixed;top:0;left:0;right:0;height:38px;z-index:100;display:flex;align-items:center;gap:3px;padding:0 8px;background:rgba(17,17,22,.86);backdrop-filter:blur(18px);color:rgba(255,255,255,.7);font-size:12px;box-shadow:0 1px 0 rgba(255,255,255,.06)}.system-bar-drag{height:38px;display:flex;align-items:center;gap:8px;padding:0 12px;flex:0 0 auto;-webkit-app-region:drag;user-select:none}.system-bar-drag strong{color:#fff;font-size:12px;letter-spacing:.02em}.system-bar-brand{display:inline-flex;align-items:center;gap:8px}.system-bar-brand-icon{display:block;width:22px;height:22px;flex:0 0 auto;object-fit:cover;border-radius:4px}.system-bar-title-pcl,.system-bar-pcl-tabs,.system-bar-title-pcl-subtitle{display:none}.system-bar-title-default-subtitle{display:inline}.system-bar-drag small{color:#818cf8;font-size:9px;letter-spacing:.14em;font-weight:700}.system-bar-spacer{height:38px;flex:1;-webkit-app-region:drag}.system-bar-back,.system-bar-badge,.system-bar-window{height:38px;min-width:32px;padding:0 8px;display:inline-flex;align-items:center;justify-content:center;border:0;background:transparent;color:inherit;cursor:pointer;-webkit-app-region:no-drag}.system-bar-back:hover,.system-bar-badge:hover,.system-bar-window:hover{background:rgba(255,255,255,.12);color:#fff}.system-bar-window.close:hover{background:#c42b3c;color:#fff}.system-bar-badge{position:relative}.system-bar-badge i{position:absolute;right:4px;top:7px;width:5px;height:5px;border-radius:50%;background:#fbc02d}.system-bar-status{display:flex;align-items:center;gap:5px;margin:0 4px;color:rgba(255,255,255,.55);-webkit-app-region:no-drag}.status-dot{width:7px;height:7px;border-radius:50%;background:#777}.status-dot.connected{background:#4caf50}.material-icons{font-size:18px;line-height:1}
</style>
