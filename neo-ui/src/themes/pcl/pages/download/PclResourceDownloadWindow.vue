<template>
  <div class="pcl-resource-download-window">
    <PclWindowTitleBar :title="t('resource.download', { kind: kindLabel })" :subtitle="instanceName || instanceId" />
    <div class="pcl-resource-download-window__body">
      <div class="pcl-resource-download-window__toolbar">
        <PclSelect v-model="provider" :options="providerOptions" @update:model-value="changeProvider" />
        <PclInput v-model="query" :placeholder="t('pcl.resourceDownload.searchPlaceholder', { kind: kindLabel })" @enter="search(true)" />
        <PclButton variant="highlight" icon="search" :disabled="searching" @click="search(true)">{{ searching ? t('pcl.resourceDownload.searching') : t('search.placeholder') }}</PclButton>
        <div class="pcl-resource-download-window__view">
          <PclIconButton icon="grid-2x2" :label="t('resource.grid')" :size="32" :class="{ active: viewMode === 'grid' }" @click="setViewMode('grid')" />
          <PclIconButton icon="layout-list" :label="t('resource.list')" :size="32" :class="{ active: viewMode === 'list' }" @click="setViewMode('list')" />
        </div>
      </div>

      <div class="pcl-resource-download-window__constraints">
        <PclIcon name="gauge" :size="13" />
        <span v-if="instanceVersion">Minecraft {{ instanceVersion }}</span>
        <span v-if="instanceLoaderLabel" class="pcl-resource-download-window__loader">{{ instanceLoaderLabel }}</span>
        <span class="pcl-resource-download-window__constraint-hint">{{ t('pcl.resourceDownload.instanceFilter') }}</span>
      </div>

      <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

      <div v-if="!projects.length && !searching" class="pcl-empty">
        <PclIcon name="download" :size="36" />
        <strong>{{ t('pcl.resourceDownload.searchKind', { kind: kindLabel }) }}</strong>
        <span>{{ t('pcl.resourceDownload.searchHint') }}</span>
      </div>

      <div ref="resultsList" class="pcl-resource-download-window__list" :class="`is-${viewMode}`">
        <article v-for="project in projects" :key="projectKey(project)" class="pcl-resource-download-card" :class="{ selected: selected[projectKey(project)] }">
          <PclCheckbox :model-value="Boolean(selected[projectKey(project)])" @update:model-value="toggleProject(project, $event)" />
          <img v-if="project.iconUrl" :src="project.iconUrl" alt="" @click="openDetails(project)" />
          <span v-else class="pcl-resource-download-card__fallback"><PclIcon name="box" :size="20" /></span>
          <div class="pcl-resource-download-card__copy" @click="openDetails(project)">
            <strong>{{ project.name }}<span v-if="project.mcmod?.name" class="pcl-resource-download-card__localized-name"> · {{ project.mcmod.name }}</span></strong>
            <small class="pcl-resource-download-card__provider">
              <template v-for="link in platformLinks(project)" :key="`${link.label}:${link.url}`">
                <a class="pcl-resource-download-link" :href="link.url" target="_blank" rel="noreferrer" @click.stop.prevent="openExternal(link.url)">{{ link.label }}</a>
              </template>
              <span v-if="!platformLinks(project).length">{{ providerLabel(project.provider || provider) }}</span>
              <span v-if="project.projectId"> · {{ project.projectId }}</span>
              <a v-if="project.mcmod?.url || project.mcmod?.searchUrl" class="pcl-resource-download-link" :href="project.mcmod.url || project.mcmod.searchUrl" target="_blank" rel="noreferrer" @click.stop.prevent="openExternal(project.mcmod.url || project.mcmod.searchUrl)">{{ t('resource.mcmodLink') }}</a>
              <a v-if="project.sourceUrl" class="pcl-resource-download-link" :href="project.sourceUrl" target="_blank" rel="noreferrer" @click.stop.prevent="openExternal(project.sourceUrl)">{{ t('resource.sourceCode') }}</a>
              <a v-if="project.websiteUrl && project.websiteUrl !== (project.providerUrl || providerUrl(project))" class="pcl-resource-download-link" :href="project.websiteUrl" target="_blank" rel="noreferrer" @click.stop.prevent="openExternal(project.websiteUrl)">{{ t('resource.projectPage') }}</a>
            </small>
            <small>{{ project.description }}</small>
            <PclSelect
              v-if="selected[projectKey(project)]"
              v-model="selected[projectKey(project)].versionId"
              :options="selected[projectKey(project)].versionOptions"
              :disabled="selected[projectKey(project)].loadingVersions"
            />
          </div>
          <span v-if="selected[projectKey(project)]?.state" class="pcl-resource-download-card__state" :class="selected[projectKey(project)].state">
            {{ selected[projectKey(project)].status || selected[projectKey(project)].state }}
          </span>
        </article>
      </div>

      <div v-if="offset > 0 || hasNext" class="pcl-resource-download-window__pager">
        <PclButton size="sm" variant="ghost" icon="arrow-left" :disabled="offset === 0 || searching" @click="previousPage">{{ t('pcl.resourceDownload.previous') }}</PclButton>
        <span>{{ t('pcl.resourceDownload.page', { page: pageNumber }) }}</span>
        <PclButton size="sm" variant="ghost" icon="arrow-right" :disabled="!hasNext || searching" @click="nextPage">{{ t('pcl.resourceDownload.next') }}</PclButton>
      </div>

      <div class="pcl-resource-download-window__footer">
        <PclCheckbox v-if="kind === 'mods'" v-model="includeOptional" :label="t('pcl.resourceDownload.optionalDeps')" />
        <span class="pcl-resource-download-window__count">{{ t('resource.selected', { count: selectedList.length }) }}</span>
        <PclButton variant="highlight" icon="download" :disabled="!canPrepareDownload && !downloading" @click="downloading ? (downloadOpen = true) : prepareDownload()">
          {{ downloading ? t('pcl.resourceDownload.downloading') : t('pcl.resourceDownload.checkDownload') }}
        </PclButton>
        <PclButton variant="ghost" icon="x" @click="closeWindow">{{ t('window.close') }}</PclButton>
      </div>
    </div>

    <Transition name="pcl-progress">
      <div v-if="downloadOpen" class="pcl-download-progress">
        <section class="pcl-download-progress__card" role="dialog" aria-modal="true" :aria-label="t('pcl.resourceDownload.queue')">
          <header class="pcl-download-progress__head">
            <div>
              <div class="pcl-kicker">// DOWNLOAD</div>
              <h3>{{ downloadFinished ? (downloadHasFailures ? t('pcl.resourceDownload.downloadFailed') : t('pcl.resourceDownload.queueDone')) : t('pcl.resourceDownload.downloading') }}</h3>
              <p>{{ t('pcl.resourceDownload.queueHint') }}</p>
            </div>
            <PclIconButton icon="x" :label="t('window.close')" :size="28" @click="downloadOpen = false" />
          </header>

          <div class="pcl-download-progress__summary">
            <span>{{ t('pcl.resourceDownload.queueCount', { done: downloadCompleted, total: downloadEntries.length }) }}</span>
            <strong>{{ downloadPercent }}%</strong>
          </div>
          <div class="pcl-download-progress__bar" :class="{ done: downloadFinished && !downloadHasFailures, failed: downloadFinished && downloadHasFailures }">
            <span :style="{ width: `${downloadPercent}%` }" />
          </div>

          <div class="pcl-download-progress__rows">
            <div v-for="entry in downloadEntries" :key="entry.projectId" class="pcl-download-progress__row" :class="entry.state">
              <div class="pcl-download-progress__row-head">
                <strong>{{ entry.name }}</strong>
                <span>{{ entryStateLabel(entry) }}</span>
              </div>
              <div class="pcl-download-progress__row-bar">
                <span :style="{ width: `${Math.round(entryProgress(entry) * 100)}%` }" />
              </div>
            </div>
          </div>

          <footer class="pcl-download-progress__actions">
            <PclButton v-if="downloading" variant="ghost" icon="minus" @click="downloadOpen = false">{{ t('pcl.resourceDownload.background') }}</PclButton>
            <PclButton v-if="downloading" variant="red" icon="x" @click="cancelDownloads">{{ t('pcl.resourceDownload.cancelAll') }}</PclButton>
            <PclButton v-else variant="highlight" icon="check" @click="downloadOpen = false">{{ t('pcl.resourceDownload.finish') }}</PclButton>
          </footer>
        </section>
      </div>
    </Transition>

        <PclResourceDownloadDetailDialog />

        <PclResourceDownloadConfirmDialog />
  </div>
</template>

<script setup lang="ts">
import { computed, nextTick, onMounted, provide, reactive, ref } from 'vue';
import { useI18n } from 'vue-i18n';
import PclButton from '../../components/PclButton.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import { PclResourceDownloadContextKey } from './PclResourceDownloadContext';
import PclInput from '../../components/PclInput.vue';
import PclSelect from '../../components/PclSelect.vue';
import PclWindowTitleBar from '../../components/PclWindowTitleBar.vue';
import { neoUiPreferences } from '../../../../neo-ui-preferences';
import { notifyResourceChanged } from '../../../../common/runtime/resource-events';
import { launcher } from '../../../../boot/launcher-api';
import { waitForTask } from '../../composables/useTaskEvents';
import { getCurrentWindow } from '@tauri-apps/api/window';

import PclResourceDownloadConfirmDialog from './PclResourceDownloadConfirmDialog.vue';
import PclResourceDownloadDetailDialog from './PclResourceDownloadDetailDialog.vue';

const props = defineProps<{ instanceId: string; instanceName?: string; kind: string }>();
const { t } = useI18n();
const provider = ref<any>('modrinth');
const providerCatalog = ref<any[]>([]);
const providerOptions = computed(() => providerCatalog.value
  .filter((entry: any) => Array.isArray(entry.kinds) && entry.kinds.includes(props.kind))
  .map((entry: any) => ({ label: providerLabel(entry.id), value: String(entry.id) })));
const query = ref<any>('');
const searching = ref(false);
const downloading = ref(false);
const downloadOpen = ref(false);
const includeOptional = ref(true);
const error = ref<any>('');
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
const instanceVersion = ref<any>('');
const instanceLoaders = ref<string[]>([]);
const instanceLoaderLabel = ref<any>('');

const kindLabel = computed(() => t(`resource.kinds.${props.kind}`, props.kind));
const selectedList = computed(() => Object.values(selected));
// Version resolution can still be in flight when the user clicks the batch
// action. prepareDownload waits for those requests instead of leaving the
// action permanently disabled.
const canPrepareDownload = computed(() => selectedList.value.length > 0 && !downloading.value);
const downloadEntries = computed(() => confirmEntries.value.filter(entry => entry.checked && entry.versionId));
const downloadCompleted = computed(() => downloadEntries.value.filter(isEntryFinished).length);
const downloadPercent = computed(() => {
  const entries = downloadEntries.value;
  if (!entries.length) return 0;
  const total = entries.reduce((sum, entry) => sum + entryProgress(entry), 0);
  return Math.round((total / entries.length) * 100);
});
const downloadFinished = computed(() => !downloading.value && downloadEntries.value.length > 0 && downloadCompleted.value === downloadEntries.value.length);
const downloadHasFailures = computed(() => downloadEntries.value.some(entry => ['failed', 'interrupted', 'cancelled'].includes(String(entry.state || '').toLowerCase())));

function isEntryFinished(entry: any) {
  return ['succeeded', 'failed', 'cancelled', 'interrupted'].includes(String(entry.state || '').toLowerCase());
}
function entryProgress(entry: any) {
  if (isEntryFinished(entry)) return 1;
  return Math.max(0, Math.min(1, Number(entry.progress) || 0));
}
function entryStateLabel(entry: any) {
  const state = String(entry.state || '').toLowerCase();
  if (state === 'succeeded') return t('pcl.resourceDownload.downloaded');
  if (state === 'failed' || state === 'interrupted') return entry.status || t('pcl.resourceDownload.downloadFailed');
  if (state === 'cancelled') return t('pcl.resourceDownload.cancelled');
  return entry.status || t('pcl.resourceDownload.waitingDownload');
}

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
function providerUrl(project: any) {
  if (project?.providerUrl) return String(project.providerUrl);
  const source = String(project?.provider || provider.value);
  const id = String(project?.projectId || project?.slug || '').trim();
  if (!id) return '';
  if (source === 'modrinth') return `https://modrinth.com/mod/${project?.slug || id}`;
  if (source === 'curseforge') return `https://www.curseforge.com/projects/${id}`;
  return '';
}
function platformLinks(project: any) {
  const links: Array<{ label: string; url: string }> = [];
  const add = (label: string, url: unknown) => {
    const value = String(url || '').trim();
    if (value && !links.some(link => link.url === value)) links.push({ label, url: value });
  };
  const isOfficialProjectUrl = (source: string, url: string) => {
    const value = url.toLowerCase();
    return (source === 'modrinth' && value.includes('modrinth.com/')) || (source === 'curseforge' && value.includes('curseforge.com/'));
  };
  const metadata = project?.providerMeta || {};
  for (const [source, raw] of Object.entries(metadata)) {
    const data: any = typeof raw === 'string' ? { providerUrl: raw } : raw || {};
    const ownProjectUrl = String(data.projectUrl || '').trim();
    const canonicalUrl = String(data.providerUrl || '').trim();
    const platform = ownProjectUrl && isOfficialProjectUrl(source, ownProjectUrl) ? ownProjectUrl : (canonicalUrl || ownProjectUrl);
    add(providerLabel(source), platform);
    add(t('resource.sourceCode'), data.sourceUrl);
    if (ownProjectUrl && ownProjectUrl !== platform) add(t('resource.projectPage'), ownProjectUrl);
  }
  const source = String(project?.provider || provider.value);
  add(providerLabel(source), project?.providerUrl || providerUrl(project));
  const providerLinks = project?.providerLinks || {};
  add('Modrinth', typeof providerLinks.modrinth === 'string' ? providerLinks.modrinth : providerLinks.modrinth?.url);
  add('CurseForge', typeof providerLinks.curseforge === 'string' ? providerLinks.curseforge : providerLinks.curseforge?.url);
  add('Modrinth', project?.mcmod?.modrinthId && `https://modrinth.com/mod/${project.mcmod.modrinthId}`);
  add('CurseForge', project?.mcmod?.curseForgeId && `https://www.curseforge.com/projects/${project.mcmod.curseForgeId}`);
  return links;
}
function projectKey(project: any) {
  return `${String(project?.provider || provider.value)}:${String(project?.projectId || '')}`;
}
function providerMeta(source = provider.value) {
  return providerCatalog.value.find((entry: any) => String(entry.id) === source) || null;
}
function providerSupportsMinecraftFilter(source = provider.value) {
  return providerMeta(source)?.minecraftSearchFilter !== false;
}
function providerLoaderFilters(source = provider.value) {
  const loaders = [...instanceLoaders.value];
  // Cleanroom is a Forge 1.12 fork. Modrinth and CurseForge expose Forge
  // filters only, while the UI should continue showing the real Cleanroom
  // instance label to the user.
  if ((source === 'modrinth' || source === 'curseforge') && loaders.includes('cleanroom')) {
    if (!loaders.includes('forge')) loaders.push('forge');
    return loaders.filter(loader => loader !== 'cleanroom');
  }
  return loaders;
}
function requestParams(source = provider.value) {
  const params: Record<string, unknown> = { provider: source, kind: props.kind };
  if (instanceVersion.value && providerSupportsMinecraftFilter(source)) params.minecraftVersion = instanceVersion.value;
  const loaders = providerLoaderFilters(source);
  if (props.kind === 'mods' && loaders.length && providerSupportsMinecraftFilter(source)) params.loaders = loaders;
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
    const aggregateMods = props.kind === 'mods';
    const searchParams: Record<string, unknown> = aggregateMods
      ? { kind: props.kind, providers: ['modrinth', 'curseforge'], query: query.value.trim(), offset: offset.value }
      : { ...requestParams(), query: query.value.trim(), offset: offset.value };
    if (aggregateMods && instanceVersion.value) searchParams.minecraftVersion = instanceVersion.value;
    if (aggregateMods) {
      const loaders = providerLoaderFilters('modrinth');
      if (loaders.length) searchParams.loaders = loaders;
    }
    const result: any = await execute(aggregateMods ? 'resource.search.v2' : 'resource.search', searchParams);
    const list = Array.isArray(result) ? result : result?.results || result?.projects || [];
    projects.value = list.map((project: any) => ({
      projectId: String(project.projectId || project.id || project.slug || ''),
      name: String(project.name || project.title || project.projectId || ''),
      description: String(project.description || project.synopsis || ''),
      iconUrl: String(project.iconUrl || project.icon || project.logoUrl || ''),
      provider: String(project.provider || provider.value),
      providerUrl: String(project.providerUrl || ''),
      providerLinks: project.providerLinks || {},
      providerMeta: project.providerMeta || {},
      slug: String(project.slug || ''),
      websiteUrl: String(project.websiteUrl || ''),
      sourceUrl: String(project.sourceUrl || ''),
      issuesUrl: String(project.issuesUrl || ''),
      wikiUrl: String(project.wikiUrl || ''),
      mcmod: project.mcmod || null,
    })).filter((project: any) => project.projectId);
    nextOffset.value = Number(result?.nextOffset ?? (offset.value + projects.value.length)) || 0;
    hasNext.value = Boolean(result?.mayHaveMore ?? (projects.value.length >= 25 && nextOffset.value > offset.value));
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

async function loadVersionsForEntry(entry: any) {
  if (entry.versionId && Array.isArray(entry.rawVersions) && entry.rawVersions.length) return;
  if (entry.versionRequest) return entry.versionRequest;
  entry.loadingVersions = true;
  entry.state = '';
  entry.status = '';
  const source = String(entry.provider || provider.value);
  entry.versionRequest = (async () => {
    try {
      const result: any = await execute('resource.versions', { ...requestParams(source), projectId: entry.projectId });
      const versions = Array.isArray(result) ? result : result?.versions || [];
      entry.versionOptions = versions.map((version: any) => ({
        label: `${version.versionNumber || version.name || version.versionId || t('resource.version')} ${version.releaseType ? `· ${version.releaseType}` : ''}`,
        value: String(version.versionId || version.id || ''),
      })).filter((version: any) => version.value);
      entry.rawVersions = versions;
      entry.versionId = String(versions[0]?.versionId || versions[0]?.id || '');
      if (!entry.versionId) entry.status = t('pcl.resourceDownload.noVersions');
    } catch (exception) {
      entry.state = 'failed';
      entry.status = exception instanceof Error ? exception.message : String(exception);
    } finally {
      entry.loadingVersions = false;
      delete entry.versionRequest;
    }
  })();
  return entry.versionRequest;
}

async function toggleProject(project: any, enabled: boolean) {
  const key = projectKey(project);
  if (!enabled) {
    delete selected[key];
    return;
  }
  const entry = selected[key] = {
    projectId: project.projectId,
    name: project.name,
    provider: project.provider || provider.value,
    versionId: '',
    versionOptions: [],
    loadingVersions: true,
    state: '',
    status: '',
    taskId: '',
    progress: 0,
  };
  await loadVersionsForEntry(entry);
}

async function prepareDownload() {
  if (!selectedList.value.length) return;
  error.value = '';
  try {
    await Promise.all(selectedList.value.map(entry => loadVersionsForEntry(entry)));
    const unresolved = selectedList.value.filter(entry => entry.state === 'failed' || !entry.versionId);
    if (unresolved.length) {
      error.value = unresolved.map(entry => entry.status || entry.name).join('；');
      return;
    }
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
            const source = String(entry.provider || provider.value);
            const versions: any = await execute('resource.versions', { ...requestParams(source), projectId });
            const list = Array.isArray(versions) ? versions : versions?.versions || [];
            const wanted = String(dependency.versionId || '');
            let depVersion = wanted ? list.find((version: any) => String(version.versionId || version.id) === wanted) : undefined;
            if (!depVersion && !wanted && instanceVersion.value) {
              const resolved: any = await execute('resource.resolve-dependency', {
                ...requestParams(source),
                projectId,
                minecraftVersion: instanceVersion.value,
              });
              depVersion = resolved;
            }
            depVersion ||= list[0];
            if (!depVersion) continue;
            let name = projectId;
            try {
              const project: any = await execute('resource.project', { ...requestParams(source), projectId });
              name = String(project?.name || projectId);
            } catch { /* Keep project ID as the dependency name. */ }
            entries.push({
              projectId,
              provider: source,
              name,
              versionId: String(depVersion.versionId || depVersion.id || ''),
              versionOptions: [],
              rawVersions: list.some((version: any) => String(version.versionId || version.id || '') === String(depVersion.versionId || depVersion.id || ''))
                ? list
                : [depVersion, ...list],
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
  downloadOpen.value = true;
  const waits: Promise<void>[] = [];
  for (const entry of entries) {
    entry.state = 'queued';
    entry.status = t('pcl.resourceDownload.waitingDownload');
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
          provider: entry.provider || provider.value,
          projectId: entry.projectId,
          versionId: entry.versionId,
          minecraftVersion: instanceVersion.value,
          loaders: props.kind === 'mods' ? providerLoaderFilters(String(entry.provider || provider.value)) : [],
        },
      });
      entry.taskId = String(task?.taskId || '');
      entry.state = task?.state || 'running';
      const selectedEntry = selected[projectKey(entry)];
      if (selectedEntry) { selectedEntry.taskId = entry.taskId; selectedEntry.state = entry.state; }
      if (entry.taskId) {
        waits.push(waitForTask(entry.taskId, snapshot => {
          entry.state = String(snapshot?.state || entry.state);
          entry.status = String(snapshot?.status || entry.status);
          const total = Number(snapshot?.totalProgress || snapshot?.total || 0), current = Number(snapshot?.progress || 0);
          const percent = Number(snapshot?.percent ?? snapshot?.progressPercent);
          if (total > 0) entry.progress = Math.max(0, Math.min(1, current / total));
          else if (Number.isFinite(percent) && percent > 0) entry.progress = Math.max(0, Math.min(1, percent / 100));
        }).then(snapshot => {
          entry.state = String(snapshot?.state || entry.state);
          entry.status = snapshot?.state === 'succeeded' ? t('pcl.resourceDownload.downloaded') : (snapshot?.error || t('pcl.resourceDownload.downloadFailed'));
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
  if (entries.some(entry => String(entry.state || '').toLowerCase() === 'succeeded')) {
    await notifyResourceChanged({ instanceId: props.instanceId, kind: props.kind, source: 'pcl-resource-download' });
  }
}

async function cancelDownloads() {
  const active = downloadEntries.value.filter(entry => entry.taskId && !isEntryFinished(entry));
  await Promise.all(active.map(async entry => {
    try {
      await execute('neo.task.cancel', { taskId: entry.taskId });
      entry.state = 'cancelled';
      entry.status = t('pcl.resourceDownload.cancelled');
    } catch (exception) {
      entry.status = exception instanceof Error ? exception.message : String(exception);
    }
  }));
}

async function openDetails(project: any) {
  detail.value = { ...project };
  detailOpen.value = true;
  detailLoading.value = true;
  try {
    const result: any = await execute('resource.project', { ...requestParams(String(project.provider || provider.value)), projectId: project.projectId });
    detail.value = { ...detail.value, ...result };
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    detailLoading.value = false;
  }
}

const detailBody = computed(() => {
  const value = String(detail.value?.body || detail.value?.description || '').trim();
  if (!value) return '';
  // Project bodies are provider-controlled Markdown/HTML. Escape first, then
  // add only the small set of formatting used by the standalone view.
  const escaped = value.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\"/g, '&quot;');
  return escaped
    .replace(/^#{1,6}\s+(.+)$/gm, '<h4>$1</h4>')
    .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
    .replace(/\[([^\]]+)\]\((https?:\/\/[^)\s]+)\)/g, '<a href="$2" target="_blank" rel="noreferrer">$1</a>')
    .split(/\n{2,}/)
    .map(block => `<p>${block.replace(/\n/g, '<br>')}</p>`)
    .join('');
});

async function openExternal(url: string) {
  const value = String(url || '').trim();
  if (!value) return;
  try {
    await launcher.execute('desktop.open-url', { url: value });
  } catch {
    window.open(value, '_blank', 'noopener,noreferrer');
  }
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

provide(PclResourceDownloadContextKey, { t, props, provider, providerLabel, detailOpen, detail, detailBody, detailLoading, openExternal, confirmOpen, confirmEntries, confirmDownload, selectedList, closeWindow, kindLabel });
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

.pcl-resource-download-window__list { display: grid; flex: 1 1 auto; grid-template-columns: 1fr 1fr; gap: 8px; overflow-y: auto; min-height: 0; padding-right: 2px; }
.pcl-resource-download-window__list.is-list { grid-template-columns: minmax(0, 1fr); }
.pcl-resource-download-card { display: flex; align-items: flex-start; gap: 8px; min-width: 0; padding: 8px; border: 1px solid var(--pcl-border); border-radius: 4px; background: var(--pcl-card); }
.pcl-resource-download-card.selected { border-color: var(--pcl-c3); background: var(--pcl-c8); }
.pcl-resource-download-card img, .pcl-resource-download-card__fallback { width: 38px; height: 38px; flex: 0 0 38px; border-radius: 4px; object-fit: cover; }
.pcl-resource-download-card__fallback { display: flex; align-items: center; justify-content: center; color: var(--pcl-muted); background: var(--pcl-c7); }
.pcl-resource-download-card__copy { display: flex; flex-direction: column; gap: 3px; min-width: 0; flex: 1; }
.pcl-resource-download-card__copy strong { overflow: hidden; font-size: calc(11px * var(--pcl-font-scale, 1)); font-weight: 500; text-overflow: ellipsis; white-space: nowrap; }
.pcl-resource-download-card__localized-name { color: var(--pcl-muted); font-size: .9em; font-weight: 400; }
.pcl-resource-download-card__copy small { display: -webkit-box; overflow: hidden; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); white-space: normal; -webkit-line-clamp: 2; -webkit-box-orient: vertical; }
.pcl-resource-download-card__mcmod { display: flex; align-items: center; gap: 7px; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-resource-download-card__mcmod a { color: var(--pcl-c3); }
.pcl-resource-download-card__provider { display: flex; flex-wrap: wrap; align-items: center; gap: 4px; color: var(--pcl-c3); }
.pcl-resource-download-link { display: inline-flex; align-items: center; min-height: 18px; padding: 1px 6px; border: 1px solid currentColor; border-radius: 4px; color: inherit; text-decoration: none; line-height: 1.2; }
.pcl-resource-download-link:hover { background: color-mix(in srgb, currentColor 12%, transparent); text-decoration: none; }
.pcl-resource-download-card__state { flex: 0 0 auto; max-width: 110px; overflow: hidden; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); text-overflow: ellipsis; white-space: nowrap; }
.pcl-resource-download-card__state.succeeded { color: #34d399; }
.pcl-resource-download-card__state.failed { color: #fb7185; }
.pcl-resource-download-window__pager { display: flex; align-items: center; justify-content: center; gap: 10px; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-resource-download-window__pager > span { min-width: 58px; text-align: center; }

.pcl-resource-download-window__footer { display: flex; align-items: center; gap: 12px; padding-top: 4px; border-top: 1px solid var(--pcl-border); }
.pcl-resource-download-window__count { margin-left: auto; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); }
@media (max-width: 760px) { .pcl-resource-download-window__list { grid-template-columns: 1fr; } }

.pcl-download-progress {
  position: fixed;
  inset: 0;
  z-index: 8000;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 18px;
  background: rgba(8, 16, 28, .62);
  backdrop-filter: blur(2px);
}
.pcl-download-progress__card {
  display: flex;
  flex-direction: column;
  width: min(560px, 94vw);
  max-height: 82vh;
  overflow: hidden;
  border: 1px solid var(--pcl-card-border);
  border-radius: var(--pcl-radius);
  background: var(--pcl-card);
  box-shadow: var(--pcl-shadow-hover);
  color: var(--pcl-text);
}
.pcl-download-progress__head {
  display: flex;
  align-items: flex-start;
  gap: 10px;
  padding: 15px 16px 10px;
}
.pcl-download-progress__head > div { flex: 1; min-width: 0; }
.pcl-download-progress__head h3 { margin: 2px 0 3px; font-size: calc(16px * var(--pcl-font-scale, 1)); }
.pcl-download-progress__head p { margin: 0; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); line-height: 1.45; }
.pcl-download-progress__summary {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  gap: 10px;
  padding: 0 16px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}
.pcl-download-progress__summary strong { color: var(--pcl-c3); font-size: calc(15px * var(--pcl-font-scale, 1)); }
.pcl-download-progress__bar,
.pcl-download-progress__row-bar {
  overflow: hidden;
  border-radius: 99px;
  background: var(--pcl-c7);
}
.pcl-download-progress__bar { height: 7px; margin: 7px 16px 12px; }
.pcl-download-progress__bar span,
.pcl-download-progress__row-bar span {
  display: block;
  height: 100%;
  border-radius: inherit;
  background: var(--pcl-c3);
  transition: width .25s ease;
}
.pcl-download-progress__bar.done span { background: #34d399; }
.pcl-download-progress__bar.failed span { background: #fb7185; }
.pcl-download-progress__rows {
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  gap: 6px;
  min-height: 0;
  overflow-y: auto;
  padding: 0 16px 12px;
}
.pcl-download-progress__row {
  padding: 7px 8px;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-half-white);
}
.pcl-download-progress__row-head {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 8px;
  margin-bottom: 5px;
}
.pcl-download-progress__row-head strong {
  overflow: hidden;
  font-size: calc(11px * var(--pcl-font-scale, 1));
  font-weight: 500;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.pcl-download-progress__row-head span {
  flex: 0 0 auto;
  max-width: 48%;
  overflow: hidden;
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}
.pcl-download-progress__row-bar { height: 4px; }
.pcl-download-progress__row.failed .pcl-download-progress__row-bar span,
.pcl-download-progress__row.interrupted .pcl-download-progress__row-bar span { background: #fb7185; }
.pcl-download-progress__row.succeeded .pcl-download-progress__row-bar span { background: #34d399; }
.pcl-download-progress__row.failed .pcl-download-progress__row-head span,
.pcl-download-progress__row.interrupted .pcl-download-progress__row-head span { color: #fb7185; }
.pcl-download-progress__actions {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  padding: 10px 16px 14px;
  border-top: 1px solid var(--pcl-border);
}
.pcl-progress-enter-active,
.pcl-progress-leave-active { transition: opacity .16s ease; }
.pcl-progress-enter-from,
.pcl-progress-leave-to { opacity: 0; }

.pcl-resource-detail { width: 640px; max-width: 92vw; max-height: 84vh; overflow: hidden; }
.pcl-resource-detail__head { display: flex; align-items: flex-start; gap: 10px; padding-bottom: 8px; }
.pcl-resource-detail__head img { width: 48px; height: 48px; flex: 0 0 48px; border-radius: 4px; object-fit: cover; }
.pcl-resource-detail__head > div { flex: 1; min-width: 0; }
.pcl-resource-detail__head h3 { margin: 3px 0; color: var(--pcl-text); font-size: calc(17px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__head p { margin: 0; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__body { max-height: calc(84vh - 100px); overflow-y: auto; color: var(--pcl-text); font-size: calc(11px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__body p { white-space: pre-wrap; line-height: 1.55; }
.pcl-resource-detail__body h4 { margin: 12px 0 5px; color: var(--pcl-text); font-size: calc(14px * var(--pcl-font-scale, 1)); }
.pcl-resource-detail__body strong { color: var(--pcl-text); }
.pcl-resource-detail__description { color: var(--pcl-text); line-height: 1.55; }
.pcl-resource-detail__description p { margin: 0 0 10px; }
.pcl-resource-detail__description h4 { margin: 12px 0 5px; }
.pcl-resource-detail__description a { color: var(--pcl-c3); }
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

