<template>
  <div class="pcl-resource-download-window">
    <PclWindowTitleBar :title="`下载${kindLabel}`" :subtitle="instanceName || instanceId" />
    <div class="pcl-resource-download-window__body">
      <div class="pcl-resource-download-window__toolbar">
        <PclSelect v-model="provider" :options="providers" @update:model-value="search" />
        <PclInput v-model="query" :placeholder="`搜索${kindLabel}`" @enter="search" />
        <PclButton variant="highlight" icon="search" :disabled="searching" @click="search">{{ searching ? '搜索中…' : '搜索' }}</PclButton>
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

      <div class="pcl-resource-download-window__list" :class="`is-${viewMode}`">
        <article v-for="project in projects" :key="project.projectId" class="pcl-resource-download-card" :class="{ selected: selected[project.projectId] }">
          <PclCheckbox :model-value="Boolean(selected[project.projectId])" @update:model-value="toggleProject(project, $event)" />
          <img v-if="project.iconUrl" :src="project.iconUrl" alt="" />
          <span v-else class="pcl-resource-download-card__fallback"><PclIcon name="box" :size="20" /></span>
          <div class="pcl-resource-download-card__copy">
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

      <div class="pcl-resource-download-window__footer">
        <PclCheckbox v-if="kind === 'mods'" v-model="includeOptional" label="安装可选依赖" />
        <span class="pcl-resource-download-window__count">已选择 {{ selectedList.length }} 项</span>
        <PclButton variant="highlight" icon="download" :disabled="!selectedList.length || downloading" @click="download">
          {{ downloading ? '下载中…' : `下载 ${selectedList.length || ''}` }}
        </PclButton>
        <PclButton variant="ghost" icon="x" @click="closeWindow">关闭</PclButton>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, reactive, ref } from 'vue';
import PclButton from '../../components/PclButton.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclSelect from '../../components/PclSelect.vue';
import PclWindowTitleBar from '../../components/PclWindowTitleBar.vue';
import { launcher } from '../../../boot/launcher-api';
import { getCurrentWindow } from '@tauri-apps/api/window';

const props = defineProps<{ instanceId: string; instanceName?: string; kind: string }>();
const provider = ref('modrinth');
const providers = [{ label: 'Modrinth', value: 'modrinth' }, { label: 'CurseForge', value: 'curseforge' }];
const query = ref('');
const searching = ref(false);
const downloading = ref(false);
const includeOptional = ref(true);
const error = ref('');
const projects = ref<any[]>([]);
const viewMode = ref<'grid' | 'list'>((() => { try { return localStorage.getItem('pcl-resource-download-view') === 'list' ? 'list' : 'grid'; } catch { return 'grid'; } })());
function setViewMode(mode: 'grid' | 'list') { viewMode.value = mode; try { localStorage.setItem('pcl-resource-download-view', mode); } catch { /* Storage may be unavailable. */ } }
const selected = reactive<Record<string, any>>({});
const instanceVersion = ref('');
const instanceLoaders = ref<string[]>([]);
const instanceLoaderLabel = ref('');

const kindLabel = computed(() => ({ mods: '模组', resourcepacks: '资源包', shaderpacks: '光影包', datapacks: '数据包', worlds: '世界', texturepacks: '材质包' } as Record<string, string>)[props.kind] || props.kind);
const selectedList = computed(() => Object.values(selected));

function requestParams() {
  const params: Record<string, unknown> = { provider: provider.value, kind: props.kind };
  if (instanceVersion.value) params.minecraftVersion = instanceVersion.value;
  if (props.kind === 'mods' && instanceLoaders.value.length) params.loaders = instanceLoaders.value;
  return params;
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
      'com.cleanroommc.cleanroom': 'forge',
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

async function search() {
  searching.value = true;
  error.value = '';
  try {
    const result: any = await execute('resource.search', { ...requestParams(), query: query.value.trim(), offset: 0 });
    const list = Array.isArray(result) ? result : result?.results || result?.projects || [];
    projects.value = list.map((project: any) => ({
      projectId: String(project.projectId || project.id || project.slug || ''),
      name: String(project.name || project.title || project.projectId || ''),
      description: String(project.description || project.synopsis || ''),
      iconUrl: String(project.iconUrl || project.icon || project.logoUrl || ''),
    })).filter((project: any) => project.projectId);
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    searching.value = false;
  }
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
    entry.versionId = String(versions[0]?.versionId || versions[0]?.id || '');
  } catch (exception) {
    const entry = selected[project.projectId];
    if (entry) { entry.state = 'failed'; entry.status = exception instanceof Error ? exception.message : String(exception); }
  } finally {
    const entry = selected[project.projectId];
    if (entry) entry.loadingVersions = false;
  }
}

async function download() {
  downloading.value = true;
  error.value = '';
  const active = selectedList.value.filter(entry => entry.versionId);
  for (const entry of active) {
    entry.state = 'queued';
    entry.status = '等待下载';
    try {
      const task: any = await execute('neo.task.start', {
        operation: 'resource.install-with-dependencies',
        priority: 'normal',
        parameters: {
          instance: props.instanceId,
          kind: props.kind,
          provider: provider.value,
          projectId: entry.projectId,
          versionId: entry.versionId,
          includeOptional: props.kind === 'mods' ? includeOptional.value : false,
          minecraftVersion: instanceVersion.value,
          loaders: props.kind === 'mods' ? instanceLoaders.value : [],
        },
      });
      entry.taskId = String(task?.taskId || '');
      entry.state = task?.state || 'running';
    } catch (exception) {
      entry.state = 'failed';
      entry.status = exception instanceof Error ? exception.message : String(exception);
    }
  }
  await poll();
}

async function poll() {
  let pending = true;
  while (pending) {
    pending = false;
    for (const entry of selectedList.value) {
      if (!entry.taskId || ['succeeded', 'failed', 'cancelled'].includes(entry.state)) continue;
      try {
        const task: any = await execute('neo.task.status', { taskId: entry.taskId });
        entry.state = String(task?.state || entry.state);
        entry.status = String(task?.status || entry.status);
        const total = Number(task?.totalProgress || task?.total || 0), current = Number(task?.progress || 0);
        entry.progress = total > 0 ? current / total : entry.progress;
        if (task?.finished) {
          if (task.state === 'succeeded') entry.status = '下载完成';
          else entry.status = task.error || '下载失败';
        } else pending = true;
      } catch (exception) {
        entry.state = 'failed';
        entry.status = exception instanceof Error ? exception.message : String(exception);
      }
    }
    if (pending) await new Promise(resolve => setTimeout(resolve, 450));
  }
  downloading.value = false;
}

async function closeWindow() {
  try { await getCurrentWindow().close(); } catch { window.close(); }
}

onMounted(async () => {
  document.documentElement.dataset.neoShell = 'pcl';
  document.documentElement.dataset.neoTheme = 'dark';
  await loadInstanceConstraints();
  void search();
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
.pcl-resource-download-window__footer { display: flex; align-items: center; gap: 12px; padding-top: 4px; border-top: 1px solid var(--pcl-border); }
.pcl-resource-download-window__count { margin-left: auto; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); }
@media (max-width: 760px) { .pcl-resource-download-window__list { grid-template-columns: 1fr; } }
</style>
