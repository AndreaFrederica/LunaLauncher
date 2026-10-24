<template>
  <div class="pcl-resource-download-window">
    <PclWindowTitleBar :title="`下载${kindLabel}`" :subtitle="instanceName || instanceId" />
    <div class="pcl-resource-download-window__body">
      <div class="pcl-resource-download-window__toolbar">
        <PclSelect v-model="provider" :options="providerOptions" @update:model-value="changeProvider" />
        <PclInput v-model="query" :placeholder="`搜索${kindLabel}`" @enter="search(true)" />
        <PclButton variant="highlight" icon="search" :disabled="searching" @click="search(true)">{{ searching ? '搜索中…' : '搜索' }}</PclButton>
        <div class="pcl-resource-download-window__view">
          <PclIconButton icon="grid-2x2" label="卡片视图" :size="28" :class="{ active: viewMode === 'grid' }" @click="setViewMode('grid')" />
          <PclIconButton icon="layout-list" label="列表视图" :size="28" :class="{ active: viewMode === 'list' }" @click="setViewMode('list')" />
        </div>
      </div>

      <div class="pcl-resource-download-window__constraints">
        <PclIcon name="gauge" :size="13" />
        <span v-if="instanceVersion">Minecraft {{ instanceVersion }}</span>
        <span v-if="instanceLoaderLabel" class="pcl-resource-download-window__loader">{{ instanceLoaderLabel }}</span>
        <span class="pcl-resource-download-window__constraint-hint">已按当前实例筛选</span>
      </div>

      <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

      <div v-if="!projects.length && !searching" class="pcl-empty">
        <PclIcon name="download" :size="36" />
        <strong>搜索要下载的{{ kindLabel }}</strong>
        <span>勾选多个项目，选择版本后一起下载。</span>
      </div>

      <div ref="resultsList" class="pcl-resource-download-window__list" :class="`is-${viewMode}`">
        <article v-for="project in projects" :key="project.projectId" class="pcl-resource-download-card" :class="{ selected: selected[project.projectId] }">
          <PclCheckbox :model-value="Boolean(selected[project.projectId])" @update:model-value="toggleProject(project, $event)" />
          <img v-if="project.iconUrl" :src="project.iconUrl" alt="" @click="openDetails(project)" />
          <span v-else class="pcl-resource-download-card__fallback"><PclIcon name="box" :size="20" /></span>
          <div class="pcl-resource-download-card__copy" @click="openDetails(project)">
            <strong>{{ project.name }}</strong>
            <small>{{ project.description }}</small>
            <PclSelect
              v-if="selected[project.projectId]"
              v-model="selected[project.projectId].versionId"
              :options="selected[project.projectId].versionOptions"
              :disabled="selected[project.projectId].loadingVersions"
            />
          </div>
          <span v-if="selected[project.projectId]?.state" class="pcl-resource-download-card__state" :class="selected[project.projectId].state">
            {{ selected[project.projectId].status || selected[project.projectId].state }}
          </span>
        </article>
      </div>

      <div v-if="offset > 0 || hasNext" class="pcl-resource-download-window__pager">
        <PclButton size="sm" variant="ghost" icon="arrow-left" :disabled="offset === 0 || searching" @click="previousPage">上一页</PclButton>
        <span>第 {{ pageNumber }} 页</span>
        <PclButton size="sm" variant="ghost" icon="arrow-right" :disabled="!hasNext || searching" @click="nextPage">下一页</PclButton>
      </div>

      <div class="pcl-resource-download-window__footer">
        <PclCheckbox v-if="kind === 'mods'" v-model="includeOptional" label="安装可选依赖" />
        <span class="pcl-resource-download-window__count">已选择 {{ selectedList.length }} 项</span>
        <PclButton variant="highlight" icon="download" :disabled="!selectedList.length || downloading" @click="prepareDownload">
          {{ downloading ? '下载中…' : '检查依赖并下载' }}
        </PclButton>
        <PclButton variant="ghost" icon="x" @click="closeWindow">关闭</PclButton>
      </div>
    </div>

    <q-dialog v-model="detailOpen">
      <q-card class="pcl-dialog pcl-resource-detail">
        <q-card-section class="pcl-resource-detail__head">
          <img v-if="detail?.iconUrl" :src="detail.iconUrl" alt="" />
          <div><div class="pcl-kicker">资源详情</div><h3>{{ detail?.name || '加载中…' }}</h3><p>{{ detail?.authors?.map((a:any) => a.name).join(', ') || providerLabel(provider) }}</p></div>
          <PclIconButton icon="x" label="关闭" :size="28" @click="detailOpen = false" />
        </q-card-section>
        <q-card-section class="pcl-resource-detail__body">
          <div v-if="detailLoading" class="pcl-resource-detail__loading">正在读取项目详情…</div>
          <p v-else>{{ detail?.description || '没有项目说明。' }}</p>
          <a v-if="detail?.websiteUrl" :href="detail.websiteUrl" target="_blank" rel="noreferrer" @click.prevent="openExternal(detail.websiteUrl)">打开项目页面</a>
        </q-card-section>
      </q-card>
    </q-dialog>

    <q-dialog v-model="confirmOpen">
      <q-card class="pcl-dialog pcl-resource-confirm">
        <q-card-section class="pcl-resource-detail__head">
          <div><div class="pcl-kicker">下载确认</div><h3>确认下载 {{ confirmEntries.filter((entry:any) => entry.checked).length }} 项</h3><p>包含自动解析的必需依赖</p></div>
          <PclIconButton icon="x" label="关闭" :size="28" @click="confirmOpen = false" />
        </q-card-section>
        <q-card-section class="pcl-resource-confirm__body">
          <PclHint theme="blue">下载前可以取消不想安装的项目。依赖项由当前资源版本递归解析。</PclHint>
          <div class="pcl-resource-confirm__list">
            <label v-for="entry in confirmEntries" :key="entry.projectId" class="pcl-resource-confirm__row">
              <PclCheckbox v-model="entry.checked" />
              <span><strong>{{ entry.name }}</strong><small>{{ entry.dependency ? `${entry.dependencyType === 'optional' ? '可选依赖' : '必需依赖'} · 由 ${entry.requiredBy || '所选资源'} 需要` : '已选择' }}</small></span>
            </label>
          </div>
        </q-card-section>
        <q-card-actions align="right">
          <PclButton variant="ghost" @click="confirmOpen = false">取消</PclButton>
          <PclButton variant="highlight" icon="download" :disabled="!confirmEntries.some((entry:any) => entry.checked && entry.versionId)" @click="confirmDownload">开始下载</PclButton>
        </q-card-actions>
      </q-card>
    </q-dialog>
  </div>
</template>

<script setup lang="ts">
import { computed, nextTick, onMounted, reactive, ref } from 'vue';
import PclButton from '../../components/PclButton.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclSelect from '../../components/PclSelect.vue';
import PclWindowTitleBar from '../../components/PclWindowTitleBar.vue';
import { neoUiPreferences } from '../../../neo-ui-preferences';
import { launcher } from '../../../boot/launcher-api';
import { waitForTask } from '../../composables/useTaskEvents';
import { getCurrentWindow } from '@tauri-apps/api/window';

const props = defineProps<{ instanceId: string; instanceName?: string; kind: string }>();
const provider = ref('modrinth');
const providerCatalog = ref<any[]>([]);
const providerOptions = computed(() => providerCatalog.value
  .filter((entry: any) => Array.isArray(entry.kinds) && entry.kinds.includes(props.kind))
  .map((entry: any) => ({ label: providerLabel(entry.id), value: String(entry.id) })));
const query = ref('');
const searching = ref(false);
const downloading = ref(false);
const includeOptional = ref(true);
const error = ref('');
const projects = ref<any[]>([]);
const offset = ref(0);
const resultsList = ref<HTMLElement | null>(null);
const nextOffset = ref(0);
const hasNext = ref(false);
const pageNumber = computed(() => Math.floor(offset.value / 25) + 1);
const viewMode = ref<'grid' | 'list'>((() => { try { return localStorage.getItem('pcl-resource-download-view') === 'list' ? 'list' : 'grid'; } catch { return 'grid'; } })());
function setViewMode(mode: 'grid' | 'list') { viewMode.value = mode; try { localStorage.setItem('pcl-resource-download-view', mode); } catch { /* Storage may be unavailable. */ } }
const selected = reactive<Record<string, any>>({});
const detailOpen = ref(false), detail = ref<any>(null), detailLoading = ref(false);
const confirmOpen = ref(false), confirmEntries = ref<any[]>([]);
const instanceVersion = ref('');
const instanceLoaders = ref<string[]>([]);
const instanceLoaderLabel = ref('');

const kindLabel = computed(() => ({ mods: '模组', resourcepacks: '资源包', shaderpacks: '光影包', datapacks: '数据包', worlds: '世界', texturepacks: '材质包' } as Record<string, string>)[props.kind] || props.kind);
const selectedList = computed(() => Object.values(selected));

function scrollResultsToTop() {
  void nextTick(() => resultsList.value?.scrollTo({ top: 0, behavior: 'smooth' }));
}

function clearSelected() {
  for (const key of Object.keys(selected)) delete selected[key];
  confirmEntries.value = [];
  confirmOpen.value = false;
}

async function changeProvider() {
  // Project and version IDs are provider-scoped. Carrying selections across a
  // provider switch would send the old IDs to the new provider and install a
  // different resource or fail with a misleading version error.
  clearSelected();
  await search(true);
}

function providerLabel(id: string) {
  if (id === 'modrinth') return 'Modrinth';
  if (id === 'curseforge') return 'CurseForge';
  if (id === 'hangar') return 'Hangar';
  if (id.startsWith('js:')) return `JS · ${id.slice(3)}`;
  return id;
}
function providerMeta() {
  return providerCatalog.value.find((entry: any) => String(entry.id) === provider.value) || null;
}
function providerSupportsMinecraftFilter() {
  return providerMeta()?.minecraftSearchFilter !== false;
}
function requestParams() {
  const params: Record<string, unknown> = { provider: provider.value, kind: props.kind };
  if (instanceVersion.value && providerSupportsMinecraftFilter()) params.minecraftVersion = instanceVersion.value;
  if (props.kind === 'mods' && instanceLoaders.value.length && providerSupportsMinecraftFilter()) params.loaders = instanceLoaders.value;
  return params;
}

async function loadProviders() {
  try {
    const result: any = await execute('resource.providers');
    providerCatalog.value = Array.isArray(result) && result.length ? result : [
      { id: 'modrinth', kinds: ['mods', 'resourcepacks', 'shaderpacks', 'datapacks', 'modpacks'] },
      { id: 'curseforge', kinds: ['mods', 'resourcepacks', 'shaderpacks', 'datapacks', 'modpacks'] },
    ];
  } catch {
    providerCatalog.value = [
      { id: 'modrinth', kinds: ['mods', 'resourcepacks', 'shaderpacks', 'datapacks', 'modpacks'] },
      { id: 'curseforge', kinds: ['mods', 'resourcepacks', 'shaderpacks', 'datapacks', 'modpacks'] },
    ];
  }
  const available = providerOptions.value;
  if (available.length && !available.some(option => option.value === provider.value)) {
    provider.value = String(available[0].value);
  }
}

async function loadInstanceConstraints() {
  try {
    const info: any = await execute('instance.info', { instance: props.instanceId });
    const componentResult: any = await execute('instance.components.list', { instance: props.instanceId }).catch(() => null);
    const components = Array.isArray(componentResult?.components) ? componentResult.components : [];
    const minecraftComponent = components.find((component: any) => String(component?.id || component?.uid || '').toLowerCase() === 'net.minecraft');
    instanceVersion.value = String(info?.runtime?.minecraftVersion || minecraftComponent?.version || '');
    const map: Record<string, string> = {
      'net.fabricmc.fabric-loader': 'fabric',
      'org.quiltmc.quilt-loader': 'quilt',
      'net.minecraftforge': 'forge',
      'net.neoforged': 'neoforge',
      'com.cleanroommc.cleanroom': 'cleanroom',
      'com.mumfrey.liteloader': 'liteloader',
    };
    const loaders = new Set<string>();
    for (const component of components) {
      const id = String(component?.id || component?.uid || '').toLowerCase();
      const mapped = map[id];
      if (mapped) loaders.add(mapped);
    }
    instanceLoaders.value = [...loaders];
    instanceLoaderLabel.value = instanceLoaders.value.map(loader => loader[0].toUpperCase() + loader.slice(1)).join(' + ');
  } catch {
    // Keep the catalog usable if instance metadata is unavailable.
  }
}

async function execute(operation: string, parameters: Record<string, unknown> = {}) {
  const result = await launcher.execute(operation, parameters);
  if (result && result.ok === false) throw new Error(result.error || operation);
  return result?.data ?? result;
}

async function search(reset = true) {
  if (reset) {
    offset.value = 0;
    clearSelected();
  }
  searching.value = true;
  error.value = '';
  try {
    const result: any = await execute('resource.search', { ...requestParams(), query: query.value.trim(), offset: offset.value });
    const list = Array.isArray(result) ? result : result?.results || result?.projects || [];
    projects.value = list.map((project: any) => ({
      projectId: String(project.projectId || project.id || project.slug || ''),
      name: String(project.name || project.title || project.projectId || ''),
      description: String(project.description || project.synopsis || ''),
      iconUrl: String(project.iconUrl || project.icon || project.logoUrl || ''),
    })).filter((project: any) => project.projectId);
    nextOffset.value = Number(result?.nextOffset ?? (offset.value + projects.value.length)) || 0;
    hasNext.value = projects.value.length >= 25 && nextOffset.value > offset.value;
    scrollResultsToTop();
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    searching.value = false;
  }
}

async function nextPage() {
  if (!hasNext.value) return;
  offset.value = nextOffset.value;
  await search(false);
}
async function previousPage() {
  if (offset.value === 0) return;
  offset.value = Math.max(0, offset.value - 25);
  await search(false);
}
async function toggleProject(project: any, enabled: boolean) {
  if (!enabled) {
    delete selected[project.projectId];
    return;
  }
  selected[project.projectId] = {
    projectId: project.projectId,
    name: project.name,
    versionId: '',
    versionOptions: [],
    loadingVersions: true,
    state: '',
    status: '',
    taskId: '',
    progress: 0,
  };
  try {
    const result: any = await execute('resource.versions', { ...requestParams(), projectId: project.projectId });
    const versions = Array.isArray(result) ? result : result?.versions || [];
    const entry = selected[project.projectId];
    if (!entry) return;
    entry.versionOptions = versions.map((version: any) => ({
      label: `${version.versionNumber || version.name || version.versionId || '版本'} ${version.releaseType ? `· ${version.releaseType}` : ''}`,
      value: String(version.versionId || version.id || ''),
    }));
    entry.rawVersions = versions;
    entry.versionId = String(versions[0]?.versionId || versions[0]?.id || '');
  } catch (exception) {
    const entry = selected[project.projectId];
    if (entry) { entry.state = 'failed'; entry.status = exception instanceof Error ? exception.message : String(exception); }
  } finally {
    const entry = selected[project.projectId];
    if (entry) entry.loadingVersions = false;
  }
}

async function prepareDownload() {
  if (!selectedList.value.length) return;
  error.value = '';
  try {
    const entries = selectedList.value.map(entry => ({ ...entry, dependency: false, requiredBy: '', checked: true }));
    const seen = new Set(entries.map(entry => entry.projectId));
    if (props.kind !== 'worlds') {
      for (let index = 0; index < entries.length && entries.length < 48; index++) {
        const entry = entries[index];
        const rawVersions = entry.rawVersions || [];
        const selectedVersion = rawVersions.find((version: any) => String(version.versionId || version.id) === String(entry.versionId)) || rawVersions[0];
        for (const dependency of selectedVersion?.dependencies || []) {
          const type = String(dependency.type || '').toLowerCase();
          if (type !== 'required' && !(type === 'optional' && props.kind === 'mods' && includeOptional.value)) continue;
          const projectId = String(dependency.projectId || dependency.addonId || dependency.id || '');
          if (!projectId || seen.has(projectId)) continue;
          try {
            const versions: any = await execute('resource.versions', { ...requestParams(), projectId });
            const list = Array.isArray(versions) ? versions : versions?.versions || [];
            const wanted = String(dependency.versionId || '');
            let depVersion = wanted ? list.find((version: any) => String(version.versionId || version.id) === wanted) : undefined;
            if (!depVersion && !wanted && instanceVersion.value) {
              const resolved: any = await execute('resource.resolve-dependency', {
                ...requestParams(),
                projectId,
                minecraftVersion: instanceVersion.value,
              });
              depVersion = resolved;
            }
            depVersion ||= list[0];
            if (!depVersion) continue;
            let name = projectId;
            try {
              const project: any = await execute('resource.project', { ...requestParams(), projectId });
              name = String(project?.name || projectId);
            } catch { /* Keep project ID as the dependency name. */ }
            entries.push({
              projectId,
              name,
              versionId: String(depVersion.versionId || depVersion.id || ''),
              versionOptions: [],
              rawVersions: list,
              dependency: true,
              requiredBy: entry.name,
              dependencyType: type,
              state: '',
              status: '',
              taskId: '',
              checked: true,
            });
            seen.add(projectId);
          } catch {
            // A dependency that cannot be resolved is left to the installer's warning path.
          }
        }
      }
    }
    confirmEntries.value = entries;
    confirmOpen.value = true;
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

async function confirmDownload() {
  const entries = confirmEntries.value.filter(entry => entry.checked && entry.versionId);
  confirmOpen.value = false;
  downloading.value = true;
  const waits: Promise<void>[] = [];
  for (const entry of entries) {
    entry.state = 'queued';
    entry.status = '等待下载';
    try {
      const task: any = await execute('neo.task.start', {
        // Dependencies are represented as separate checked entries above. A
        // recursive install here would ignore the user's choices and install
        // unchecked dependencies again.
        operation: 'resource.install-version',
        priority: 'normal',
        parameters: {
          instance: props.instanceId,
          kind: props.kind,
          provider: provider.value,
          projectId: entry.projectId,
          versionId: entry.versionId,
          minecraftVersion: instanceVersion.value,
          loaders: props.kind === 'mods' ? instanceLoaders.value : [],
        },
      });
      entry.taskId = String(task?.taskId || '');
      entry.state = task?.state || 'running';
      const selectedEntry = selected[entry.projectId];
      if (selectedEntry) { selectedEntry.taskId = entry.taskId; selectedEntry.state = entry.state; }
      if (entry.taskId) {
        waits.push(waitForTask(entry.taskId, snapshot => {
          entry.state = String(snapshot?.state || entry.state);
          entry.status = String(snapshot?.status || entry.status);
          const total = Number(snapshot?.totalProgress || snapshot?.total || 0), current = Number(snapshot?.progress || 0);
          entry.progress = total > 0 ? current / total : entry.progress;
        }).then(snapshot => {
          entry.state = String(snapshot?.state || entry.state);
          entry.status = snapshot?.state === 'succeeded' ? '下载完成' : (snapshot?.error || '下载失败');
        }).catch(exception => {
          entry.state = 'failed';
          entry.status = exception instanceof Error ? exception.message : String(exception);
        }));
      }
    } catch (exception) {
      entry.state = 'failed';
      entry.status = exception instanceof Error ? exception.message : String(exception);
    }
  }
  await Promise.all(waits);
  downloading.value = false;
}

async function openDetails(project: any) {
  detail.value = { ...project };
  detailOpen.value = true;
  detailLoading.value = true;
  try {
    const result: any = await execute('resource.project', { ...requestParams(), projectId: project.projectId });
    detail.value = { ...detail.value, ...result };
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    detailLoading.value = false;
  }
}

function openExternal(url: string) {
  window.open(url, '_blank', 'noopener,noreferrer');
}

async function closeWindow() {
  try { await getCurrentWindow().close(); } catch { window.close(); }
}

onMounted(async () => {
  document.documentElement.dataset.neoShell = 'pcl';
  document.documentElement.dataset.neoTheme = 'dark';
  await Promise.all([loadProviders(), loadInstanceConstraints()]);
  if (neoUiPreferences.pclAutoRefreshOnPageOpen) void search();
});
</script>

<style scoped>
.pcl-resource-download-window { display: flex; flex-direction: column; width: 100vw; height: 100vh; overflow: hidden; background: var(--pcl-page-bg); color: var(--pcl-text); }
.pcl-resource-download-window__body { display: flex; flex: 1 1 auto; min-height: 0; flex-direction: column; gap: 10px; padding: 16px 18px; overflow: hidden; }
.pcl-resource-download-window__toolbar { display: grid; grid-template-columns: 160px minmax(0, 1fr) auto auto; gap: 8px; align-items: center; }
.pcl-resource-download-window__view { display: flex; gap: 2px; align-items: center; }
.pcl-icon-btn.active { color: var(--pcl-c3); background: var(--pcl-c6); }
.pcl-resource-download-window__constraints { display: flex; align-items: center; gap: 7px; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-resource-download-window__loader { padding: 2px 6px; border: 1px solid var(--pcl-border); border-radius: 3px; color: var(--pcl-c3); }
.pcl-resource-download-window__constraint-hint { margin-left: auto; opacity: .7; }

.pcl-resource-download-window__list { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; overflow-y: auto; min-height: 0; padding-right: 2px; }
.pcl-resource-download-window__list.is-list { grid-template-columns: minmax(0, 1fr); }
.pcl-resource-download-card { display: flex; align-items: flex-start; gap: 8px; min-width: 0; padding: 8px; border: 1px solid var(--pcl-border); border-radius: 4px; background: var(--pcl-card); }
.pcl-resource-download-card.selected { border-color: var(--pcl-c3); background: var(--pcl-c8); }
.pcl-resource-download-card img, .pcl-resource-download-card__fallback { width: 38px; height: 38px; flex: 0 0 38px; border-radius: 4px; object-fit: cover; }
.pcl-resource-download-card__fallback { display: flex; align-items: center; justify-content: center; color: var(--pcl-muted); background: var(--pcl-c7); }
.pcl-resource-download-card__copy { display: flex; flex-direction: column; gap: 3px; min-width: 0; flex: 1; }
.pcl-resource-download-card__copy strong { overflow: hidden; font-size: calc(11px * var(--pcl-font-scale, 1)); font-weight: 500; text-overflow: ellipsis; white-space: nowrap; }
.pcl-resource-download-card__copy small { display: -webkit-box; overflow: hidden; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); white-space: normal; -webkit-line-clamp: 2; -webkit-box-orient: vertical; }
.pcl-resource-download-card__state { flex: 0 0 auto; max-width: 110px; overflow: hidden; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); text-overflow: ellipsis; white-space: nowrap; }
.pcl-resource-download-card__state.succeeded { color: #34d399; }
.pcl-resource-download-card__state.failed { color: #fb7185; }
.pcl-resource-download-window__pager { display: flex; align-items: center; justify-content: center; gap: 10px; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-resource-download-window__pager > span { min-width: 58px; text-align: center; }

.pcl-resource-download-window__footer { display: flex; align-items: center; gap: 12px; padding-top: 4px; border-top: 1px solid var(--pcl-border); }
.pcl-resource-download-window__count { margin-left: auto; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); }
@media (max-width: 760px) { .pcl-resource-download-window__list { grid-template-columns: 1fr; } }

.pcl-resource-detail { width: 640px; max-width: 92vw; max-height: 84vh; overflow: hidden; }
.pcl-resource-detail__head { display: flex; align-items: flex-start; gap: 10px; padding-bottom: 8px; }
.pcl-resource-detail__head img { width: 48px; height: 48px; flex: 0 0 48px; border-radius: 4px; object-fit: cover; }
.pcl-resource-detail__head > div { flex: 1; min-width: 0; }
.pcl-resource-detail__head h3 { margin: 3px 0; color: var(--pcl-text); font-size: calc(17px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__head p { margin: 0; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__body { max-height: calc(84vh - 100px); overflow-y: auto; color: var(--pcl-text); font-size: calc(11px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__body p { white-space: pre-wrap; }
.pcl-resource-detail__body a { color: var(--pcl-c3); }
.pcl-resource-detail__loading { color: var(--pcl-muted); }
.pcl-resource-confirm { width: 620px; max-width: 92vw; max-height: 80vh; overflow: hidden; }
.pcl-resource-confirm__body { max-height: calc(80vh - 150px); overflow-y: auto; }
.pcl-resource-confirm__list { display: flex; flex-direction: column; gap: 4px; margin-top: 10px; }
.pcl-resource-confirm__row { display: flex; align-items: flex-start; gap: 7px; padding: 6px; border: 1px solid var(--pcl-border); border-radius: 3px; }
.pcl-resource-confirm__row > span { display: flex; flex-direction: column; min-width: 0; }
.pcl-resource-confirm__row strong { overflow: hidden; color: var(--pcl-text); font-size: calc(10px * var(--pcl-font-scale, 1)); font-weight: 500; text-overflow: ellipsis; white-space: nowrap; }
.pcl-resource-confirm__row small { color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); }
</style>
