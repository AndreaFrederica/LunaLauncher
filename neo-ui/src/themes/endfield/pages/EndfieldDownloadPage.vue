<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef-download">
    <nav v-if="!compact" class="ef-tabs">
      <button type="button" :class="{ current: mode === 'game' }" @click="mode = 'game'"><small>// 01</small>游戏</button>
      <button type="button" :class="{ current: mode === 'modpacks' }" @click="mode = 'modpacks'"><small>// 02</small>整合包</button>
      <button type="button" :class="{ current: mode === 'resource' }" @click="mode = 'resource'"><small>// 03</small>资源</button>
    </nav>

    <section v-if="mode === 'game'" class="ef-download__game">
      <EfCard title="安装 Minecraft">
        <div class="ef-form">
          <label>版本<EfSelect v-model="game.version" :options="gameVersionOptions" placeholder="选择 Minecraft 版本" /></label>
          <label>加载器<EfSelect v-model="game.loader" :options="loaderOptions" /></label>
          <label v-if="game.loader">加载器版本<EfSelect v-model="game.loaderVersion" :options="gameLoaderVersionOptions" placeholder="选择加载器版本" /></label>
          <label>实例名称<EfInput v-model="game.name" placeholder="自动生成" /></label>
        </div>
        <div class="ef-download__actions">
          <EfButton icon="upload_file" :disabled="installing" @click="importInstance">导入实例包</EfButton>
          <EfButton variant="accent" icon="download" :busy="installing" :disabled="!game.version" @click="installGame">安装</EfButton>
        </div>
      </EfCard>
    </section>

    <section v-else class="ef-download__resource">
      <div class="ef-download__toolbar">
        <EfSelect v-model="provider" :options="providerOptions" />
        <EfSelect v-if="mode === 'resource'" v-model="kind" :options="kindOptions" />
        <EfSearchBox v-model="query" placeholder="搜索项目" @enter="search" />
        <EfButton icon="search" :busy="searching" @click="search">搜索</EfButton>
      </div>

      <div class="ef-download__grid">
        <div class="ef-download__results">
          <div v-if="searching" class="ef-download__loading">正在搜索…</div>
          <button
            v-for="item in results"
            :key="item.projectId"
            type="button"
            class="ef-project"
            :class="{ current: project?.projectId === item.projectId }"
            @click="selectProject(item)"
          >
            <img v-if="item.iconUrl" :src="item.iconUrl" alt="" />
            <span v-else class="material-icons">widgets</span>
            <span class="ef-project__copy">
              <strong>{{ item.name }}</strong>
              <small>{{ item.description }}</small>
              <em>{{ item.downloads !== undefined ? `${item.downloads} 下载` : item.provider }}</em>
            </span>
          </button>
          <EfEmpty v-if="!results.length && !searching" title="没有搜索结果" description="换一个关键词，或切换来源。" icon="search" />
        </div>

        <aside class="ef-download__detail">
          <template v-if="project">
            <h2>{{ project.name }}</h2>
            <p class="ef-download__desc">{{ project.description }}</p>
            <div class="ef-download__fields">
              <EfField label="来源" :value="provider" />
              <EfField label="项目" :value="project.projectId" />
            </div>
            <label class="ef-download__label">版本<EfSelect v-model="version" :options="versionOptions" placeholder="选择版本" /></label>
            <label v-if="mode === 'modpacks'" class="ef-download__label">实例名称<EfInput v-model="packName" :placeholder="project.name" /></label>
            <label v-else-if="selected" class="ef-download__label">目标实例<EfInput :model-value="selected.name" disabled /></label>
            <p v-else class="ef-download__warn">先在实例页选择一个实例，再安装资源。</p>
            <EfButton
              variant="accent"
              icon="download"
              :busy="installing"
              :disabled="!version || (mode === 'resource' && !selected)"
              @click="installProject"
            >
              安装
            </EfButton>
          </template>
          <EfEmpty v-else title="选择一个项目" description="从左侧搜索结果中选择项目查看版本。" icon="ads_click" />
        </aside>
      </div>
    </section>

    <div v-if="progressOpen" class="ef-download-progress">
      <section class="ef-download-progress__card" role="dialog" aria-modal="true" aria-label="下载进度">
        <header class="ef-download-progress__head">
          <div>
            <p class="ef-eyebrow">// TASK</p>
            <h3>{{ progress.state === 'succeeded' ? '安装完成' : progress.state === 'failed' ? '安装失败' : progress.state === 'cancelled' ? '已取消' : '正在安装' }}</h3>
          </div>
          <button type="button" class="ef-download-progress__close" aria-label="关闭" @click="progressOpen = false">×</button>
        </header>
        <strong class="ef-download-progress__name">{{ progress.name }}</strong>
        <div class="ef-download-progress__bar" :class="progress.state">
          <span :style="{ width: `${Math.round(progress.percent)}%` }" />
        </div>
        <div class="ef-download-progress__meta">
          <span>{{ Math.round(progress.percent) }}%</span>
          <span>{{ progress.status }}</span>
        </div>
        <footer class="ef-download-progress__actions">
          <EfButton v-if="progress.state === 'running'" icon="minimize" @click="progressOpen = false">后台运行</EfButton>
          <EfButton v-if="progress.state === 'running'" variant="red" icon="close" @click="cancelProgressTask">取消</EfButton>
          <EfButton v-else variant="accent" icon="check" @click="progressOpen = false">完成</EfButton>
        </footer>
      </section>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import EfButton from '../components/EfButton.vue';
import EfCard from '../components/EfCard.vue';
import EfEmpty from '../components/feedback/EfEmpty.vue';
import EfField from '../components/data/EfField.vue';
import EfInput from '../components/EfInput.vue';
import EfSearchBox from '../components/controls/EfSearchBox.vue';
import EfSelect from '../components/EfSelect.vue';
import { notifyResourceChanged } from '../../../common/runtime/resource-events';

const props = defineProps<{
  busy: boolean;
  execute: (op: string, p?: Record<string, unknown>) => Promise<any>;
  instances: any[];
  selected?: any;
  initialMode?: 'game' | 'modpacks' | 'resource';
  initialKind?: string;
  compact?: boolean;
}>();
const emit = defineEmits<{
  installed: [];
  toast: [type: 'success' | 'error' | 'info', title: string, message?: string];
}>();

const mode = ref<'game' | 'modpacks' | 'resource'>(props.initialMode || 'game');
const installing = ref(false);
const progressOpen = ref(false);
const progress = ref({ name: '', percent: 0, status: '准备中…', state: 'running', taskId: '' });
const game = ref({ version: '', loader: '', loaderVersion: '', name: '' });
const gameVersions = ref<any[]>([]);
const gameLoaderVersions = ref<any[]>([]);
const provider = ref('');
const kind = ref(props.initialKind || 'mods');
const query = ref('');
const searching = ref(false);
const results = ref<any[]>([]);
const project = ref<any>(null);
const versions = ref<any[]>([]);
const version = ref('');
const packName = ref('');
const providers = ref<any[]>([]);

const loaderOptions = [
  { label: '原版', value: '' },
  { label: 'Fabric', value: 'fabric' },
  { label: 'Quilt', value: 'quilt' },
  { label: 'Forge', value: 'forge' },
  { label: 'NeoForge', value: 'neoforge' },
];
const loaderUid: Record<string, string> = {
  fabric: 'net.fabricmc.fabric-loader',
  quilt: 'org.quiltmc.quilt-loader',
  forge: 'net.minecraftforge',
  neoforge: 'net.neoforged',
};
const kindOptions = [
  { label: '模组', value: 'mods' },
  { label: '资源包', value: 'resourcepacks' },
  { label: '光影', value: 'shaderpacks' },
  { label: '世界', value: 'worlds' },
];
const gameVersionOptions = computed(() => gameVersions.value.map(entry => ({ label: versionOf(entry), value: versionOf(entry) })));
const gameLoaderVersionOptions = computed(() => gameLoaderVersions.value.map(entry => ({ label: versionOf(entry), value: versionOf(entry) })));
const versionOptions = computed(() => versions.value.map(entry => ({ label: versionOf(entry), value: versionOf(entry) })));
const providerOptions = computed(() => providers.value.map((item: any) => ({
  label: item.name || item.label || item.id,
  value: String(item.id),
})));

function versionOf(entry: any) {
  return String(entry?.version || entry?.versionId || entry?.id || entry?.name || '');
}

async function loadGameVersions() {
  if (gameVersions.value.length) return;
  try {
    const result: any = await props.execute('component.versions', { uid: 'net.minecraft' });
    gameVersions.value = Array.isArray(result) ? result : (result?.versions || []);
    const latest = gameVersions.value[gameVersions.value.length - 1];
    if (latest && !game.value.version) game.value.version = versionOf(latest);
  } catch { /* shell toast */ }
}

async function loadLoaderVersions() {
  gameLoaderVersions.value = [];
  game.value.loaderVersion = '';
  if (!game.value.loader || !game.value.version) return;
  try {
    const result: any = await props.execute('component.versions', { uid: loaderUid[game.value.loader], minecraftVersion: game.value.version });
    gameLoaderVersions.value = Array.isArray(result) ? result : (result?.versions || []);
    const latest = gameLoaderVersions.value[gameLoaderVersions.value.length - 1];
    if (latest) game.value.loaderVersion = versionOf(latest);
  } catch { /* ignore */ }
}

function taskFinished(task: any) {
  return Boolean(task?.finished) || ['succeeded', 'failed', 'cancelled', 'interrupted'].includes(String(task?.state || '').toLowerCase());
}

function updateProgress(task: any) {
  if (!task) return;
  if (task.taskId) progress.value.taskId = String(task.taskId);
  if (task.status) progress.value.status = String(task.status);
  if (['succeeded', 'failed', 'cancelled', 'interrupted'].includes(String(task.state || '').toLowerCase())) {
    progress.value.state = String(task.state);
  }
  const total = Number(task.totalProgress || task.total || 0);
  const current = Number(task.progress || 0);
  if (total > 0) progress.value.percent = Math.max(0, Math.min(100, current / total * 100));
  else if (Number.isFinite(Number(task.percent))) progress.value.percent = Math.max(0, Math.min(100, Number(task.percent)));
}

async function runTask(operation: string, parameters: Record<string, unknown>, name: string, successTitle: string) {
  installing.value = true;
  progressOpen.value = true;
  progress.value = { name, percent: 0, status: '准备中…', state: 'running', taskId: '' };
  try {
    let task: any = await props.execute('neo.task.start', { operation, priority: 'high', parameters });
    updateProgress(task);
    while (task && !taskFinished(task)) {
      await new Promise(resolve => setTimeout(resolve, 300));
      task = await props.execute('neo.task.status', { taskId: task.taskId });
      updateProgress(task);
    }
    if (String(task?.state || '').toLowerCase() !== 'succeeded') throw new Error(task?.error || '安装失败');
    progress.value.state = 'succeeded';
    progress.value.percent = 100;
    progress.value.status = '完成';
    if (operation === 'resource.install-version') {
      await notifyResourceChanged({ instanceId: String(parameters.instance || ''), kind: String(parameters.kind || ''), source: 'endfield-resource-download' });
    }
    emit('toast', 'success', successTitle, name);
    emit('installed');
  } catch (error: any) {
    if (progress.value.state !== 'cancelled') {
      progress.value.state = 'failed';
      progress.value.status = error?.message || String(error);
      emit('toast', 'error', '安装失败', error?.message || String(error));
    }
  } finally {
    installing.value = false;
  }
}

async function cancelProgressTask() {
  if (!progress.value.taskId) return;
  try {
    await props.execute('neo.task.cancel', { taskId: progress.value.taskId });
    progress.value.state = 'cancelled';
    progress.value.status = '已取消';
  } catch { /* shell toast */ }
}

async function installGame() {
  if (!game.value.version) return;
  const parameters: Record<string, unknown> = {
    name: game.value.name.trim() || `${game.value.version} ${game.value.loader || 'Vanilla'}`.trim(),
    version: game.value.version,
  };
  if (game.value.loader) {
    parameters.loader = game.value.loader;
    parameters.loaderVersion = game.value.loaderVersion;
  }
  await runTask('instance.create', parameters, String(parameters.name), 'Minecraft 已安装');
}

async function importInstance() {
  const path = window.prompt('整合包或实例压缩包路径');
  if (!path) return;
  await runTask('instance.import', { path }, path.split(/[\\/]/).pop() || path, '导入完成');
}

async function loadProviders() {
  if (providers.value.length) return;
  try {
    const result: any = await props.execute('resource.providers');
    providers.value = Array.isArray(result) ? result : [];
    if (!provider.value && providers.value.length) provider.value = String(providers.value[0].id);
  } catch { /* ignore */ }
}

async function search() {
  if (searching.value) return;
  searching.value = true;
  project.value = null;
  versions.value = [];
  try {
    const operation = mode.value === 'modpacks' ? 'modpack.search' : 'resource.search';
    const parameters: Record<string, unknown> = { provider: provider.value, query: query.value };
    if (mode.value === 'resource') parameters.kind = kind.value;
    if (props.selected?.version) parameters.minecraftVersion = props.selected.version;
    const result: any = await props.execute(operation, parameters);
    const list = Array.isArray(result) ? result : (result?.results || result?.projects || result?.modpacks || []);
    results.value = list.map((entry: any) => ({
      ...entry,
      projectId: String(entry.projectId || entry.slug || entry.id || entry.url || ''),
      name: entry.displayName || entry.name || entry.title || entry.display_name || entry.projectId,
      description: entry.description || entry.synopsis || entry.summary || '',
      iconUrl: entry.iconUrl || entry.icon || entry.logoUrl || entry.logo?.url || '',
    }));
    if (!results.value.length) emit('toast', 'info', '没有结果', '换一个关键词或来源。');
  } catch (error: any) {
    emit('toast', 'error', '搜索失败', error?.message || String(error));
  } finally {
    searching.value = false;
  }
}

async function selectProject(item: any) {
  project.value = item;
  versions.value = [];
  version.value = '';
  packName.value = item.name;
  try {
    const operation = mode.value === 'modpacks' ? 'modpack.versions' : 'resource.versions';
    const result: any = await props.execute(operation, { provider: provider.value, projectId: item.projectId });
    const list = Array.isArray(result) ? result : (result?.versions || []);
    versions.value = list.map((entry: any) => ({ ...entry, versionId: versionOf(entry) })).filter((entry: any) => entry.versionId);
    version.value = versions.value[0]?.versionId || '';
  } catch { /* ignore */ }
}

async function installProject() {
  if (!project.value || !version.value) return;
  if (mode.value !== 'modpacks' && !props.selected?.id) {
    emit('toast', 'error', '没有目标实例', '请先在实例页选择一个实例。');
    return;
  }
  const parameters: Record<string, unknown> = {
    provider: provider.value,
    projectId: project.value.projectId,
    versionId: version.value,
  };
  let operation = 'resource.install-version';
  if (mode.value === 'modpacks') {
    operation = 'modpack.install';
    parameters.name = packName.value || project.value.name;
  } else {
    parameters.instance = props.selected.id;
    parameters.kind = kind.value;
  }
  await runTask(operation, parameters, project.value.name, mode.value === 'modpacks' ? '整合包已安装' : '资源已安装');
}

watch(mode, value => {
  project.value = null;
  versions.value = [];
  results.value = [];
  if (value === 'game') void loadGameVersions();
  else void loadProviders();
}, { immediate: true });
watch(() => game.value.loader, () => { void loadLoaderVersions(); });
watch(() => game.value.version, () => { if (game.value.loader) void loadLoaderVersions(); });
</script>

<style scoped>
.ef-download { width: 100%; }
.ef-tabs { display: flex; border-bottom: 1px solid var(--ef-ink); margin-bottom: 12px; }
.ef-tabs button {
  display: flex;
  align-items: baseline;
  gap: 5px;
  padding: 6px 14px;
  border: 0;
  border-right: 1px solid var(--ef-rule);
  background: transparent;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 11px;
  font-weight: 600;
  cursor: pointer;
}
.ef-tabs button small { font-size: 8px; letter-spacing: .1em; color: var(--ef-ink-3, #8a8a86); }
.ef-tabs button.current { background: var(--ef-signal); color: var(--ef-on-signal); }
.ef-tabs button.current small { color: #4a4700; }
.ef-form { display: flex; flex-direction: column; gap: 10px; max-width: 440px; }
.ef-form label,
.ef-download__label {
  display: flex;
  flex-direction: column;
  gap: 4px;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .1em;
  text-transform: uppercase;
}
.ef-download__actions { display: flex; justify-content: flex-end; margin-top: 12px; }
.ef-download__toolbar { display: flex; gap: 8px; align-items: center; margin-bottom: 12px; flex-wrap: wrap; }
.ef-download__toolbar > :nth-child(1),
.ef-download__toolbar > :nth-child(2) { width: 150px; }
.ef-download__toolbar > :nth-child(3) { flex: 1; min-width: 160px; }
.ef-download__grid { display: grid; grid-template-columns: minmax(260px, 1fr) 340px; gap: 12px; }
.ef-download__results { display: flex; flex-direction: column; border-top: 1px solid var(--ef-ink); }
.ef-download__loading { padding: 18px; color: var(--ef-ink-2); font-size: 11px; }
.ef-project {
  display: flex;
  align-items: flex-start;
  gap: 9px;
  padding: 8px 4px;
  border: 0;
  border-bottom: 1px solid var(--ef-rule);
  background: transparent;
  color: var(--ef-ink);
  text-align: left;
  cursor: pointer;
}
.ef-project:hover { background: var(--ef-chip); }
.ef-project.current { background: var(--ef-signal); color: var(--ef-on-signal); }
.ef-project img { width: 34px; height: 34px; object-fit: cover; border: 1px solid var(--ef-rule); }
.ef-project > .material-icons { font-size: 20px; color: var(--ef-ink-2); }
.ef-project__copy { flex: 1; min-width: 0; }
.ef-project__copy strong {
  display: block;
  overflow: hidden;
  font-family: var(--ef-font-ui);
  font-size: 12px;
  font-weight: 700;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-project__copy small {
  display: block;
  overflow: hidden;
  color: var(--ef-ink-2);
  font-size: 9px;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-project.current small,
.ef-project.current em { color: #4a4700; }
.ef-project__copy em { display: block; margin-top: 2px; color: var(--ef-ink-3, #8a8a86); font-size: 8px; font-style: normal; }
.ef-download__detail {
  align-self: start;
  padding: 12px;
  border: 1px solid var(--ef-rule);
  background: var(--ef-panel);
}
.ef-download__detail h2 {
  margin: 0 0 5px;
  font-family: var(--ef-font-ui);
  font-size: 16px;
  font-weight: 800;
}
.ef-download__desc {
  margin: 0 0 8px;
  color: var(--ef-ink-2);
  font-size: 10px;
  line-height: 1.5;
}
.ef-download__fields { display: flex; gap: 8px; flex-wrap: wrap; margin-bottom: 10px; }
.ef-download__label { margin-bottom: 9px; }
.ef-download__warn {
  margin: 0 0 9px;
  padding: 6px 8px;
  border-left: 2px solid var(--ef-signal);
  background: var(--ef-paper);
  color: var(--ef-ink);
  font-size: 10px;
}
@media (max-width: 900px) {
  .ef-download__grid { grid-template-columns: 1fr; }
}
.ef-download-progress {
  position: fixed;
  inset: 0;
  z-index: 9000;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 18px;
  background: color-mix(in srgb, var(--ef-ink) 42%, transparent);
}
.ef-download-progress__card {
  width: min(460px, 94vw);
  padding: 16px;
  border: 1px solid var(--ef-ink);
  background: var(--ef-panel);
  box-shadow: 8px 8px 0 color-mix(in srgb, var(--ef-ink) 22%, transparent);
  color: var(--ef-ink);
}
.ef-download-progress__head {
  display: flex;
  align-items: flex-start;
  gap: 10px;
  margin-bottom: 10px;
  padding-bottom: 8px;
  border-bottom: 1px solid var(--ef-rule);
}
.ef-download-progress__head > div { flex: 1; min-width: 0; }
.ef-download-progress__head h3 {
  margin: 2px 0 0;
  font-family: var(--ef-font-ui);
  font-size: 18px;
  font-weight: 800;
}
.ef-download-progress__close {
  width: 26px;
  height: 26px;
  border: 1px solid var(--ef-rule);
  background: transparent;
  color: var(--ef-ink);
  font-size: 18px;
  line-height: 1;
  cursor: pointer;
}
.ef-download-progress__close:hover { background: var(--ef-signal); }
.ef-download-progress__name {
  display: block;
  overflow: hidden;
  margin-bottom: 8px;
  font-size: 12px;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-download-progress__bar {
  height: 8px;
  overflow: hidden;
  border: 1px solid var(--ef-ink);
  background: var(--ef-paper);
}
.ef-download-progress__bar span {
  display: block;
  height: 100%;
  background: var(--ef-signal);
  transition: width .25s var(--ef-mech);
}
.ef-download-progress__bar.succeeded span { background: var(--ef-ink); }
.ef-download-progress__bar.failed span { background: var(--ef-danger, #b3261e); }
.ef-download-progress__meta {
  display: flex;
  justify-content: space-between;
  gap: 10px;
  margin-top: 6px;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .04em;
}
.ef-download-progress__actions {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  margin-top: 14px;
  padding-top: 10px;
  border-top: 1px solid var(--ef-rule);
}
</style>
