<template>
  <section class="pcl-page pcl-download-page">
    <PclPageNav v-model="section" :groups="visibleNavGroups" @refresh="refreshSection" />

    <main class="pcl-page-main">
      <div class="pcl-page-view pcl-download-view">
          <div v-show="section === 'install'" class="pcl-download-section">
            <PclInstallView
              :execute="execute"
              :busy="busy"
              :instances="instances"
              :preset="preset"
              :initial-minecraft-version="initialMinecraftVersion"
              @installed="onInstalled"
              @preset-consumed="preset = null"
              @install-start="emit('install-start', $event)"
              @install-progress="emit('install-progress', $event)"
              @install-done="emit('install-done', $event)"
            />
          </div>

          <div v-show="Boolean(componentItem)" class="pcl-download-section">
            <PclCard :title="componentItem?.label || ''">
              <PclHint v-if="!componentItem?.uid && !componentItem?.special" theme="yellow">
                {{ componentItem?.label || '' }} 暂无 Luna 后端接口，无法在此处读取版本或安装。
              </PclHint>
              <template v-else-if="componentItem?.uid || componentItem?.special">
                <div class="pcl-component-filter">
                  <PclInput v-model="componentMinecraft" placeholder="Minecraft 版本（可选）" @enter="loadComponentVersions" />
                  <PclButton variant="highlight" icon="refresh-cw" :disabled="busy || componentLoading" @click="loadComponentVersions">
                    {{ componentLoading ? '正在加载' : '加载版本' }}
                  </PclButton>
                </div>
                <PclHint v-if="componentError" theme="red">{{ componentError }}</PclHint>
                <PclLoading v-if="componentLoading" text="正在读取版本列表" inline />
                <div v-else class="pcl-component-versions">
                  <PclListItem
                    v-for="version in componentVersions"
                    :key="version.version"
                    :title="version.version"
                    :info="componentItem?.special ? `Minecraft ${version.minecraftVersion || version.version} · ${version.type || '版本'}` : (version.type || '版本')"
                    :tags="version.recommended ? ['推荐'] : []"
                    @click="useComponentVersion(version)"
                  >
                    <template #icon><PclIcon :name="componentItem?.icon || 'package'" :size="20" /></template>
                    <template #actions>
                      <span class="pcl-component-use">{{ componentItem?.id === 'client' ? '选择' : '用于安装' }}</span>
                    </template>
                  </PclListItem>
                  <div v-if="!componentVersions.length" class="pcl-empty">
                    点击“加载版本”读取可选版本。
                  </div>
                </div>
              </template>
            </PclCard>
          </div>

          <div v-show="section === 'import'" class="pcl-download-section">
          <PclCard title="导入整合包">
            <PclHint theme="blue">
              支持本地 .zip / .mrpack 文件、HTTP(S) 下载地址，以及 CurseForge 安装链接。把文件拖到下方，或直接粘贴路径 / URL。
            </PclHint>
            <div
              class="pcl-import-drop"
              :class="{ active: importDragOver }"
              @dragover.prevent="importDragOver = true"
              @dragleave.prevent="importDragOver = false"
              @drop.prevent="onImportDrop"
            >
              <PclIcon name="upload" :size="34" />
              <strong>拖入整合包文件</strong>
              <span>或在下方的路径框里粘贴本地文件路径</span>
            </div>
            <div class="pcl-form-row pcl-form-row--top">
              <span class="pcl-form-row__label">本地文件</span>
              <div class="pcl-form-row__control pcl-form-row__control--column">
                <PclInput v-model="importPath" placeholder="D:\下载\整合包.zip 或 https://…/pack.zip" />
                <PclIconButton icon="folder-open" label="选择整合包文件" :size="28" :disabled="importing" @click="chooseImportFile" />
              </div>
            </div>
            <div class="pcl-form-row">
              <span class="pcl-form-row__label">实例名称</span>
              <div class="pcl-form-row__control">
                <PclInput v-model="importName" placeholder="可选；留空使用整合包名称" />
              </div>
            </div>
            <PclHint v-if="importError" theme="red" closable @close="importError = ''">{{ importError }}</PclHint>
            <div class="pcl-actions">
              <PclButton variant="highlight" icon="file-input" :disabled="busy || importing || !importPath.trim()" @click="importInstance">
                {{ importing ? '正在导入' : '开始导入' }}
              </PclButton>
              <PclButton variant="ghost" icon="download" @click="emit('launch')">返回实例</PclButton>
            </div>
          </PclCard>
          </div>

          <div v-show="section === 'worlds'" class="pcl-download-section">
          <PclCard title="世界">
            <PclHint theme="yellow">
              Luna 的 Launcher API 暂不支持世界资源类型。你仍然可以在实例目录中手动安装世界存档。
            </PclHint>
            <div class="pcl-actions">
              <PclButton icon="folder-open" @click="emit('launch')">返回实例</PclButton>
            </div>
          </PclCard>
          </div>

          <div v-show="isResource" class="pcl-download-section">
          <PclCard class="pcl-resource-panel">
            <PclResourceCatalogView
              :refresh-token="refreshKey"
              :instance="selected?.id"
              :kind="resourceKind"
              :minecraft-version="downloadMinecraft"
              :loader="downloadLoader"
              :busy="busy"
              :execute="execute"
              :default-provider="resourceProvider"
              :tag="resourceTag"
              :auto-refresh="preferences.pclAutoRefreshOnPageOpen && isResource"
              @installed="emit('refresh')"
              @install-start="emit('install-start', $event)"
              @install-progress="emit('install-progress', $event)"
              @install-done="emit('install-done', $event)"
            />
          </PclCard>
          </div>

          <div v-show="section === 'modpacks'" class="pcl-download-section">
          <PclCard>
            <PclCatalogBrowser
              :refresh-token="refreshKey"
              packs
              :busy="busy"
              :execute="execute"
              default-provider="modrinth"
              :auto-refresh="preferences.pclAutoRefreshOnPageOpen && section === 'modpacks'"
              @installed="onModpackInstalled"
              @install-start="emit('install-start', $event)"
              @install-progress="emit('install-progress', $event)"
              @install-done="emit('install-done', $event)"
            />
          </PclCard>
          </div>

          <div v-show="section === 'favorites'" class="pcl-download-section">
          <PclCard title="我的收藏">
            <div class="pcl-favorites">
              <PclListItem
                v-for="item in favorites"
                :key="item.key || `${item.provider}-${item.projectId}`"
                :title="item.name || item.projectId"
                :info="`${item.provider || 'modrinth'} · ${item.kind || 'mod'}`"
                @click="openFavorite(item)"
              >
                <template #icon><PclIcon name="heart-filled" :size="19" /></template>
                <template #actions><PclIcon name="chevron-right" :size="16" /></template>
              </PclListItem>
              <div v-if="!favorites.length" class="pcl-empty">
                <PclIcon name="heart" :size="34" />
                <strong>还没有收藏</strong>
                <span>在资源详情页收藏项目后会显示在这里。</span>
              </div>
            </div>
          </PclCard>
          </div>
        </div>
    </main>
  </section>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue';
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
import type { PclNavGroup } from '../../types';
import { hiddenPclFeatures, isPclFeatureHidden } from '../../composables/usePclFeatures';
import { neoUiPreferences } from '../../../neo-ui-preferences';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
type ComponentItem = { id: string; label: string; icon: string; uid?: string; alias?: string; special?: string; channel?: string };

const props = defineProps<{ busy: boolean; execute: Execute; selected?: any; instances?: any[]; initialComponentUid?: string }>();
const emit = defineEmits<{ refresh: []; launch: []; installed: [result: any]; 'install-start': [info: any]; 'install-progress': [task: any]; 'install-done': [info: any] }>();

const section = ref('install');
const preferences = neoUiPreferences;
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
  { id: 'labymod-production', label: 'LabyMod（正式版）', icon: 'sparkles', alias: 'labymod-production', special: 'labymod-production' },
  { id: 'labymod-snapshot', label: 'LabyMod（快照）', icon: 'sparkles', alias: 'labymod-snapshot', special: 'labymod-snapshot' },
];

const navGroups: PclNavGroup[] = [
  { items: [
    { id: 'install', label: 'Minecraft', icon: 'boxes', refresh: true },
    { id: 'import', label: '导入整合包', icon: 'file-input' },
  ] },
  { label: '社区资源', items: [
    { id: 'mods', label: '模组', icon: 'puzzle', refresh: true },
    { id: 'modpacks', label: '整合包', icon: 'package', refresh: true },
    { id: 'datapacks', label: '数据包', icon: 'file-archive', refresh: true },
    { id: 'resourcepacks', label: '资源包', icon: 'layers', refresh: true },
    { id: 'shaders', label: '光影包', icon: 'sparkles', refresh: true },
    { id: 'worlds', label: '世界', icon: 'globe', refresh: true },
    { id: 'favorites', label: '收藏夹', icon: 'heart', refresh: true },
  ] },
  { label: '安装包', items: componentItems.map(item => ({
    id: item.id,
    label: item.label,
    icon: item.icon,
    refresh: true,
  })) },
];

const visibleNavGroups = computed<PclNavGroup[]>(() => navGroups
  .map(group => ({
    ...group,
    items: group.items.filter(item => item.id === 'install' || !isPclFeatureHidden(`download.${item.id}`)),
  }))
  .filter(group => group.items.length));

const componentItem = computed(() => componentItems.find(item => item.id === section.value));
const componentVersions = ref<any[]>([]);
const componentMinecraft = ref('');
const downloadMinecraft = ref('');
const downloadLoader = ref('');

async function loadSelectedContext() {
  downloadMinecraft.value = String(props.selected?.runtime?.minecraft || props.selected?.minecraftVersion || '');
  downloadLoader.value = '';
  if (!props.selected?.id) return;
  try {
    const result = await props.execute('instance.components.list', { instance: props.selected.id });
    for (const component of result?.components || []) {
      const id = String(component.id || '').toLowerCase();
      if (id === 'net.minecraft' && component.version) downloadMinecraft.value = String(component.version);
      if (id.includes('neoforge')) downloadLoader.value = 'neoforge';
      else if (id.includes('minecraftforge') && !downloadLoader.value) downloadLoader.value = 'forge';
      else if (id.includes('fabric-loader') && !downloadLoader.value) downloadLoader.value = 'fabric';
      else if (id.includes('quilt-loader') && !downloadLoader.value) downloadLoader.value = 'quilt';
    }
  } catch { /* Keep the runtime fallback. */ }
  if (!componentMinecraft.value) componentMinecraft.value = downloadMinecraft.value;
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
const resourceTag = ref('全部');
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
    componentError.value = `${item.label} 暂未接入 Luna 后端。`;
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
  else if (file?.name) importError.value = '浏览器没有提供本地文件路径，请在下方粘贴完整路径。';
}
async function chooseImportFile() {
  const selected = await pickPclFile({ title: '选择整合包文件', filters: [{ name: '整合包', extensions: ['zip', 'mrpack'] }] });
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
  const info = { title: curseForge ? '正在安装 CurseForge 整合包' : '正在导入整合包', name: importName.value.trim() || source.split(/[\\/]/).pop() || '导入实例' };
  emit('install-start', info);
  try {
    let task: any = await props.execute('neo.task.start', {
      operation: curseForge ? 'modpack.install' : 'instance.import',
      priority: 'high',
      parameters: curseForge
        ? { provider: 'curseforge', projectId: curseForge.projectId, versionId: curseForge.versionId, ...(importName.value.trim() ? { name: importName.value.trim() } : {}) }
        : { source, ...(importName.value.trim() ? { name: importName.value.trim() } : {}) },
    });
    while (task && !task.finished) {
      emit('install-progress', task);
      await new Promise(resolve => setTimeout(resolve, 400));
      task = await props.execute('neo.task.status', { taskId: task.taskId });
    }
    emit('install-progress', task);
    if (!task || task.state !== 'succeeded') throw new Error(task?.error || '导入任务失败');
    emit('installed', {
      id: task.createdInstanceId || '',
      name: importName.value.trim() || source.split(/[\\/]/).pop() || '导入实例',
    });
  } catch (exception) {
    importError.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    importing.value = false;
    emit('install-done', info);
  }
}
onMounted(async () => {
  const tauri = (window as any).__TAURI__;
  if (tauri?.event?.listen) {
    try {
      unlistenDragDrop = await tauri.event.listen('tauri://drag-drop', (event: any) => {
        const payload = event?.payload;
        const path = Array.isArray(payload) ? payload[0] : payload?.paths?.[0] || payload?.path;
        if (path) importPath.value = String(path);
      });
    } catch {
      unlistenDragDrop = undefined;
    }
  }
});
onBeforeUnmount(() => unlistenDragDrop?.());
watch(() => props.selected?.id, () => { void loadSelectedContext(); }, { immediate: true });
watch(() => props.initialComponentUid, uid => {
  section.value = uid ? sectionForComponentUid(uid) : 'install';
}, { immediate: true });
watch(section, value => {
  componentVersions.value = [];
  componentError.value = '';
  if (componentItems.some(item => item.id === value)) {
    if (downloadMinecraft.value) componentMinecraft.value = downloadMinecraft.value;
    void loadComponentVersions();
  }
  if (value === 'favorites') readFavorites();
}, { immediate: true });
watch(hiddenPclFeatures, () => {
  if (!visibleNavGroups.value.some(group => group.items.some(item => item.id === section.value))) section.value = 'install';
}, { deep: true });
readFavorites();
</script>

<style scoped>
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
