<template>
  <section class="pcl-page pcl-download-page">
    <PclPageNav v-model="section" :groups="visibleNavGroups" @refresh="refreshSection" />

    <main class="pcl-page-main">
      <div class="pcl-page-view pcl-download-view">
                    <PclDownloadInstallSection v-show="section === 'install'" />

                    <PclDownloadComponentSection v-show="Boolean(componentItem)" />

                    <PclDownloadImportSection v-show="section === 'import'" />

                    <PclDownloadWorldsSection v-show="section === 'worlds'" />

                    <PclDownloadResourceSection v-show="isResource" />

                    <PclDownloadModpacksSection v-show="section === 'modpacks'" />

                    <PclDownloadFavoritesSection v-show="section === 'favorites'" />
        </div>
    </main>
  </section>
</template>

<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { computed, provide, ref, toRef, watch } from 'vue';
import PclPageNav from '../../components/PclPageNav.vue';
import PclCard from '../../components/PclCard.vue';
import PclButton from '../../components/PclButton.vue';
import PclHint from '../../components/PclHint.vue';
import PclInput from '../../components/PclInput.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import { pickPclFile } from '../../composables/usePclFileDialog';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclInstallView from './PclInstallView.vue';
import PclCatalogBrowser from './PclCatalogBrowser.vue';
import PclResourceCatalogView from './PclResourceCatalogView.vue';
import PclDownloadInstallSection from './PclDownloadInstallSection.vue';
import PclDownloadComponentSection from './PclDownloadComponentSection.vue';
import PclDownloadImportSection from './PclDownloadImportSection.vue';
import PclDownloadWorldsSection from './PclDownloadWorldsSection.vue';
import PclDownloadResourceSection from './PclDownloadResourceSection.vue';
import PclDownloadModpacksSection from './PclDownloadModpacksSection.vue';
import PclDownloadFavoritesSection from './PclDownloadFavoritesSection.vue';
import type { PclNavGroup } from '../../types';
import { hiddenPclFeatures, isPclFeatureHidden } from '../../composables/usePclFeatures';
import { neoUiPreferences } from '../../../../neo-ui-preferences';
import { useRuntimeLifecycle } from '../../../../common/runtime/lifecycle-debug';
import { useThemeResource } from '../../../../common/runtime/theme-runtime';
import { PclDownloadContextKey } from './PclDownloadContext';
import { isTaskItemFinished } from '../../composables/useTaskProgress';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
type ComponentItem = { id: string; label: string; icon: string; uid?: string; alias?: string; special?: string; channel?: string };

const props = defineProps<{ busy: boolean; execute: Execute; selected?: any; instances?: any[]; initialComponentUid?: string; initialSection?: string; ignoreFeatureVisibility?: boolean; active?: boolean }>();
const emit = defineEmits<{ refresh: []; launch: []; installed: [result: any]; 'install-start': [info: any]; 'install-progress': [task: any]; 'install-done': [info: any] }>();
const pageActive = computed(() => props.active !== false);
useRuntimeLifecycle('theme:pcl.download-page', pageActive);

const section = ref(props.initialSection || 'install');
const preferences = neoUiPreferences;
const downloadTargetInstanceId = computed(() => String(props.selected?.id || props.instances?.[0]?.id || ''));
const refreshKey = ref(0);
const preset = ref<{
  label: string;
  version: string;
  alias: string;
  special?: string;
  channel?: string;
  loaderVersion?: string;
  commitRef?: string;
} | null>(null);
const initialMinecraftVersion = ref('');

const componentItems: ComponentItem[] = [
  { id: 'client', label: 'Minecraft', icon: 'package', uid: 'net.minecraft' },
  { id: 'optifine', label: 'OptiFine', icon: 'gauge' },
  { id: 'forge', label: 'Forge', icon: 'anvil', uid: 'net.minecraftforge', alias: 'forge' },
  { id: 'neoforge', label: 'NeoForge', icon: 'cat', uid: 'net.neoforged', alias: 'neoforge' },
  { id: 'cleanroom', label: 'Cleanroom', icon: 'flask-conical', uid: 'com.cleanroommc.cleanroom', alias: 'cleanroom' },
  { id: 'fabric', label: 'Fabric', icon: 'scroll', uid: 'net.fabricmc.fabric-loader', alias: 'fabric' },
  { id: 'quilt', label: 'Quilt', icon: 'layers', uid: 'org.quiltmc.quilt-loader', alias: 'quilt' },
  { id: 'liteloader', label: 'LiteLoader', icon: 'egg', uid: 'com.mumfrey.liteloader', alias: 'liteloader' },
  { id: 'legacyfabric', label: 'Legacy Fabric', icon: 'scroll', alias: 'legacyfabric', special: 'legacyfabric' },
  { id: 'labymod-production', label: t('pcl.auto2.k00de822a92'), icon: 'sparkles', alias: 'labymod-production', special: 'labymod-production' },
  { id: 'labymod-snapshot', label: t('pcl.auto2.k46e7c2f01d'), icon: 'sparkles', alias: 'labymod-snapshot', special: 'labymod-snapshot' },
];

const navGroups: PclNavGroup[] = [
  { items: [
    { id: 'install', label: 'Minecraft', icon: 'boxes', refresh: true },
    { id: 'import', label: t('pcl.auto2.k6ddefa974c'), icon: 'file-input' },
  ] },
  { label: t('pcl.auto2.k3eac9be3ff'), items: [
    { id: 'mods', label: t('pcl.auto2.kda121cbcd9'), icon: 'puzzle', refresh: true },
    { id: 'modpacks', label: t('pcl.auto2.kb3a621217c'), icon: 'package', refresh: true },
    { id: 'datapacks', label: t('pcl.auto2.kf0e3fc625e'), icon: 'file-archive', refresh: true },
    { id: 'resourcepacks', label: t('pcl.auto2.k826f79355b'), icon: 'layers', refresh: true },
    { id: 'shaders', label: t('pcl.auto2.k71adc562a4'), icon: 'sparkles', refresh: true },
    { id: 'worlds', label: t('pcl.auto2.kcf1656101e'), icon: 'globe', refresh: true },
    { id: 'favorites', label: t('pcl.auto2.ke6f497d8b4'), icon: 'heart', refresh: true },
  ] },
  { label: t('pcl.auto2.k4f4d1ab15f'), items: componentItems.map(item => ({
    id: item.id,
    label: item.label,
    icon: item.icon,
    refresh: true,
  })) },
];

const visibleNavGroups = computed<PclNavGroup[]>(() => navGroups
  .map(group => ({
    ...group,
    items: group.items.filter(item => props.ignoreFeatureVisibility || item.id === 'install' || !isPclFeatureHidden(`download.${item.id}`)),
  }))
  .filter(group => group.items.length));

const componentItem = computed(() => componentItems.find(item => item.id === section.value));
const componentVersions = ref<any[]>([]);
const componentMinecraft = ref('');
const downloadMinecraft = ref('');
const downloadLoader = ref('');

async function loadSelectedContext() {
  if (props.active === false) return;
  const target = props.selected || props.instances?.find(instance => String(instance?.id) === downloadTargetInstanceId.value) || props.instances?.[0];
  downloadMinecraft.value = String(target?.runtime?.minecraftVersion || target?.minecraftVersion || '');
  downloadLoader.value = '';
  if (!target?.id) return;
  try {
    const result = await props.execute('instance.components.list', { instance: target.id });
    for (const component of result?.components || []) {
      const id = String(component.id || '').toLowerCase();
      if (id === 'net.minecraft' && component.version) downloadMinecraft.value = String(component.version);
      if (id.includes('neoforge')) downloadLoader.value = 'neoforge';
      else if (id.includes('minecraftforge') && !downloadLoader.value) downloadLoader.value = 'forge';
      else if (id.includes('fabric-loader') && !downloadLoader.value) downloadLoader.value = 'fabric';
      else if (id.includes('quilt-loader') && !downloadLoader.value) downloadLoader.value = 'quilt';
    }
  } catch { /* Keep the runtime fallback. */ }
  // The download page is a view of the globally selected instance. When the
  // selection changes, reset the default filter to that instance's version;
  // manual edits made afterward remain local to this page.
  componentMinecraft.value = downloadMinecraft.value;
  if (componentItems.some(item => item.id === section.value) && downloadMinecraft.value) {
    componentMinecraft.value = downloadMinecraft.value;
    void loadComponentVersions();
  }
}
const componentLoading = ref(false), componentError = ref('');

const resourceKinds: Record<string, string> = {
  mods: 'mods',
  datapacks: 'datapacks',
  resourcepacks: 'resourcepacks',
  shaders: 'shaderpacks',
};
const isResource = computed(() => Boolean(resourceKinds[section.value]));
const resourceKind = computed(() => resourceKinds[section.value]);
const resourceProvider = ref('modrinth');
const resourceTag = ref(t('pcl.auto2.k778fc8f994'));
const favorites = ref<any[]>([]);
const importPath = ref(''), importName = ref(''), importError = ref('');
const importing = ref(false), importDragOver = ref(false);
let unlistenDragDrop: (() => void) | undefined;

function readFavorites() {
  try {
    const raw = localStorage.getItem('luna-pcl-favorites');
    const value = raw ? JSON.parse(raw) : [];
    favorites.value = Array.isArray(value) ? value : [];
  } catch {
    favorites.value = [];
  }
}
function openFavorite(item: any) {
  const kind = item.kind || 'mods';
  section.value = kind === 'modpack' || kind === 'modpacks' ? 'modpacks' : kind;
  if (componentItems.some(value => value.id === kind)) section.value = 'install';
}
async function loadComponentVersions() {
  const item = componentItem.value;
  if (!item?.uid && !item?.special) return;
  componentLoading.value = true;
  componentError.value = '';
  try {
    const result = item.special
      ? await props.execute('special.loader.versions', { loader: item.special, ...(componentMinecraft.value.trim() ? { minecraftVersion: componentMinecraft.value.trim() } : {}) })
      : await props.execute('component.versions', {
        uid: item.uid,
        ...(componentMinecraft.value.trim() ? { minecraftVersion: componentMinecraft.value.trim() } : {}),
      });
    componentVersions.value = result?.versions || [];
  } catch (exception) {
    componentError.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    componentLoading.value = false;
  }
}
function useComponentVersion(version: any) {
  const item = componentItem.value;
  if (!item) return;
  if (item.id === 'client') {
    preset.value = null;
    initialMinecraftVersion.value = String(version.version || '');
    section.value = 'install';
    return;
  }
  initialMinecraftVersion.value = '';
  if (!item.alias) {
    componentError.value = t('pcl.downloadPage.componentUnavailable', { name: item.label });
    return;
  }
  const requiredMinecraft = Array.isArray(version.requires)
    ? version.requires.find((requirement: any) => String(requirement?.uid || '') === 'net.minecraft')?.equals
    : '';
  const minecraftVersion = String(version.minecraftVersion || requiredMinecraft || componentMinecraft.value || '').trim();
  initialMinecraftVersion.value = minecraftVersion;
  preset.value = {
    label: item.label,
    version: String(version.version || ''),
    alias: item.alias,
    ...(item.special ? {
      special: item.special,
      channel: version.channel || item.channel,
      loaderVersion: version.loaderVersion || version.version,
      commitRef: version.commitRef,
    } : {}),
  };
  section.value = 'install';
}
function refreshSection(id: string) {
  if (section.value !== id) {
    section.value = id;
    return;
  }
  refreshKey.value += 1;
  if (componentItem.value) void loadComponentVersions();
  if (id === 'favorites') readFavorites();
  emit('refresh');
}

function sectionForComponentUid(uid: string): string {
  const target = String(uid || '').trim().toLowerCase();
  return componentItems.find(item => String(item.uid || '').toLowerCase() === target)?.id || 'install';
}
function onInstalled(result: any) {
  emit('installed', result);
  emit('refresh');
}
function onModpackInstalled(result: any) {
  emit('installed', result);
  emit('refresh');
}
function onImportDrop(event: DragEvent) {
  importDragOver.value = false;
  const file = event.dataTransfer?.files?.[0] as any;
  if (file?.path) importPath.value = String(file.path);
  else if (file?.name) importError.value = t('pcl.auto2.k1ee23fe480');
}
async function chooseImportFile() {
  const selected = await pickPclFile({ title: t('pcl.auto2.k940f61fa15'), filters: [{ name: t('pcl.auto2.kb3a621217c'), extensions: ['zip', 'mrpack'] }] });
  if (selected) importPath.value = selected;
}

function parseCurseForgeImport(value: string) {
  try {
    const url = new URL(value);
    if (url.protocol.toLowerCase() !== 'curseforge:') return null;
    const projectId = url.searchParams.get('addonId') || '';
    const versionId = url.searchParams.get('fileId') || '';
    return projectId && versionId ? { projectId, versionId } : null;
  } catch {
    return null;
  }
}

function normalizedImportSource(value: string) {
  const source = value.trim();
  return source.replace(/\?client=y$/i, '/file');
}

async function importInstance() {
  if (!importPath.value.trim()) return;
  importing.value = true;
  importError.value = '';
  const source = normalizedImportSource(importPath.value);
  const curseForge = parseCurseForgeImport(source);
  const info = { title: curseForge ? t('pcl.auto2.kbe0cfa607f') : t('pcl.auto2.k2e9a411bcc'), name: importName.value.trim() || source.split(/[\\/]/).pop() || t('pcl.auto2.kf1f38acef7') };
  emit('install-start', info);
  try {
    let task: any = await props.execute('neo.task.start', {
      operation: curseForge ? 'modpack.install' : 'instance.import',
      priority: 'high',
      parameters: curseForge
        ? { provider: 'curseforge', projectId: curseForge.projectId, versionId: curseForge.versionId, ...(importName.value.trim() ? { name: importName.value.trim() } : {}) }
        : { source, ...(importName.value.trim() ? { name: importName.value.trim() } : {}) },
    });
    while (task && !isTaskItemFinished(task)) {
      emit('install-progress', task);
      await new Promise(resolve => setTimeout(resolve, 400));
      task = await props.execute('neo.task.status', { taskId: task.taskId });
    }
    emit('install-progress', task);
    if (!task || task.state !== 'succeeded') throw new Error(task?.error || t('pcl.auto2.kc042def72c'));
    emit('installed', {
      id: task.createdInstanceId || '',
      name: importName.value.trim() || source.split(/[\\/]/).pop() || t('pcl.auto2.kf1f38acef7'),
    });
  } catch (exception) {
    importError.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    importing.value = false;
    emit('install-done', info);
  }
}
useThemeResource('pcl', () => {
  let disposed = false;
  const listen = async () => {
    const tauri = (window as any).__TAURI__;
    if (!tauri?.event?.listen) return;
    try {
      const unlisten = await tauri.event.listen('tauri://drag-drop', (event: any) => {
        const payload = event?.payload;
        const path = Array.isArray(payload) ? payload[0] : payload?.paths?.[0] || payload?.path;
        if (path) importPath.value = String(path);
      });
      if (disposed) unlisten?.();
      else unlistenDragDrop = unlisten;
    } catch {
      unlistenDragDrop = undefined;
    }
  };
  void listen();
  return () => {
    disposed = true;
    unlistenDragDrop?.();
    unlistenDragDrop = undefined;
  };
});
watch(downloadTargetInstanceId, () => { if (props.active !== false) void loadSelectedContext(); }, { immediate: true });
watch(() => props.initialComponentUid, uid => {
  section.value = uid ? sectionForComponentUid(uid) : props.initialSection || 'install';
}, { immediate: true });
watch(section, value => {
  if (props.active === false) return;
  componentVersions.value = [];
  componentError.value = '';
  if (componentItems.some(item => item.id === value)) {
    if (downloadMinecraft.value) componentMinecraft.value = downloadMinecraft.value;
    void loadComponentVersions();
  }
  if (value === 'favorites') readFavorites();
}, { immediate: true });
watch(() => props.active, active => {
  if (!active) return;
  void loadSelectedContext();
  if (componentItems.some(item => item.id === section.value)) void loadComponentVersions();
});
watch(hiddenPclFeatures, () => {
  if (!visibleNavGroups.value.some(group => group.items.some(item => item.id === section.value))) section.value = 'install';
}, { deep: true });
readFavorites();

// This page stays mounted behind v-show. Keep the lifecycle flag reactive;
// passing props.active directly freezes the initial page state (usually
// false on the instances page), so the install child never loads metadata
// after navigating to the download page.
provide(PclDownloadContextKey, { t, props, emit, busy: toRef(props, 'busy'), execute: props.execute, instances: props.instances, selected: props.selected, section, preferences, downloadTargetInstanceId, refreshKey, preset, initialMinecraftVersion, componentItem, componentVersions, componentMinecraft, componentLoading, componentError, loadComponentVersions, useComponentVersion, isResource, resourceKind, resourceProvider, resourceTag, favorites, importPath, importName, importError, importing, importDragOver, onImportDrop, chooseImportFile, importInstance, openFavorite, onInstalled, onModpackInstalled, active: pageActive, downloadMinecraft, downloadLoader, resourceKinds });
</script>

<style>
.pcl-download-page {
  background: var(--pcl-page-bg);
}

.pcl-download-view {
  max-width: 100%;
}

.pcl-component-filter {
  display: flex;
  align-items: center;
  gap: 9px;
  margin-bottom: 12px;
}

.pcl-component-filter .pcl-input {
  max-width: 220px;
}

.pcl-component-versions {
  max-height: 480px;
  overflow-y: auto;
}

.pcl-component-use {
  color: var(--pcl-c3);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  white-space: nowrap;
}

.pcl-favorites {
  max-height: 480px;
  overflow-y: auto;
}

.pcl-import-drop {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 5px;
  min-height: 128px;
  margin: 14px 0;
  padding: 18px;
  border: 1px dashed var(--pcl-c5);
  border-radius: 5px;
  background: var(--pcl-c7);
  color: var(--pcl-muted);
  text-align: center;
  transition: border-color .12s ease, background .12s ease, color .12s ease;
}

.pcl-import-drop.active {
  border-color: var(--pcl-c3);
  background: var(--pcl-c6);
  color: var(--pcl-c2);
}

.pcl-import-drop strong {
  color: var(--pcl-text);
  font-size: calc(14px * var(--pcl-font-scale, 1));
  font-weight: 400;
}

.pcl-import-drop span {
  font-size: calc(12px * var(--pcl-font-scale, 1));
}
</style>

