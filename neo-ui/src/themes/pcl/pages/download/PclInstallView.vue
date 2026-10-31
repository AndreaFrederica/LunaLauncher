<template>
  <div class="pcl-install-view">
    <!-- Both steps stay mounted so switching does not recreate the installer
         tree or discard the selected metadata. Transition mode is intentionally
         not used here: it only accepts one child, while these two persistent
         v-show branches are separate components. -->
    <PclInstallVersionsStep v-show="step === 'versions'" />
    <PclInstallLoaderStep v-show="step === 'select'" />
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, provide, reactive, ref, toRef, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { PclInstallContextKey } from './PclInstallContext';

import PclInstallLoaderStep from './PclInstallLoaderStep.vue';
import PclInstallVersionsStep from './PclInstallVersionsStep.vue';
import { isTaskItemFinished } from '../../composables/useTaskProgress';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
type Version = { version: string; type?: string; time?: string; recommended?: boolean; requires?: any[]; minecraftVersion?: string; loaderVersion?: string; commitRef?: string; channel?: string; runtime?: any };
type LoaderCard = { id: string; label: string; icon: string; uid?: string; alias: string; special?: string; channel?: string; disabled?: boolean };

/**
 * The native API currently returns `{ versions: [] }`, while older sidecars
 * and a few web preview adapters return the array directly (or wrap it in
 * `data`). Keep the UI boundary tolerant of those transport shapes; the
 * version model itself remains the same after this point.
 */
function versionEntries(value: any): any[] {
  if (Array.isArray(value)) return value;
  if (Array.isArray(value?.versions)) return value.versions;
  if (Array.isArray(value?.items)) return value.items;
  if (Array.isArray(value?.data)) return value.data;
  if (Array.isArray(value?.data?.versions)) return value.data.versions;
  if (Array.isArray(value?.result?.versions)) return value.result.versions;
  return [];
}

function normalizeVersion(version: any): Version {
  return {
    version: String(version?.version || version?.id || version?.name || ''),
    type: version?.type || version?.channel,
    time: version?.time || version?.releaseTime || version?.date,
    recommended: Boolean(version?.recommended),
    requires: Array.isArray(version?.requires) ? version.requires : [],
    minecraftVersion: version?.minecraftVersion,
    loaderVersion: version?.loaderVersion,
    commitRef: version?.commitRef,
    channel: version?.channel,
    runtime: version?.runtime,
  };
}

const props = defineProps<{
  execute: Execute;
  busy: boolean;
  active?: boolean;
  instances?: any[];
  preset?: { label: string; version: string; alias: string; special?: string; channel?: string; loaderVersion?: string; commitRef?: string } | null;
  initialMinecraftVersion?: string;
}>();
const emit = defineEmits<{ installed: [result: any]; 'preset-consumed': []; 'install-start': [info: any]; 'install-progress': [task: any]; 'install-done': [info: any] }>();
const { t } = useI18n();

const step = ref<'versions' | 'select'>('versions');
const loading = ref(false), loadError = ref(''), installing = ref(false), error = ref('');
const loadErrorContext = ref<Record<string, unknown>>({});
const versions = ref<Version[]>([]);
const selectedVersion = ref<Version | null>(null);
const selectedLoader = ref<{ id: string; label: string; version: string; special?: string; channel?: string; loaderVersion?: string; commitRef?: string } | null>(null);
const openLoader = ref('');
const instanceName = ref('');
const nameTouched = ref(false);
const categoryOpen = reactive<Record<string, boolean>>({ latest: true, release: false, snapshot: false, old: false });
const loaderState = reactive<Record<string, { loading: boolean; error: string; versions: Version[] }>>({});

const loaderCards: LoaderCard[] = [
  { id: 'forge', label: 'Forge', icon: 'anvil', uid: 'net.minecraftforge', alias: 'forge' },
  { id: 'cleanroom', label: 'Cleanroom', icon: 'flask-conical', uid: 'com.cleanroommc.cleanroom', alias: 'cleanroom' },
  { id: 'neoforge', label: 'NeoForge', icon: 'cat', uid: 'net.neoforged', alias: 'neoforge' },
  { id: 'fabric', label: 'Fabric', icon: 'scroll', uid: 'net.fabricmc.fabric-loader', alias: 'fabric' },
  { id: 'quilt', label: 'Quilt', icon: 'layers', uid: 'org.quiltmc.quilt-loader', alias: 'quilt' },
  { id: 'liteloader', label: 'LiteLoader', icon: 'egg', uid: 'com.mumfrey.liteloader', alias: 'liteloader' },
  { id: 'legacyfabric', label: 'Legacy Fabric', icon: 'scroll', alias: 'legacyfabric', special: 'legacyfabric' },
  { id: 'labymod-production', label: 'LabyMod (Release)', icon: 'sparkles', alias: 'labymod-production', special: 'labymod-production', channel: 'production' },
  { id: 'labymod-snapshot', label: 'LabyMod (Snapshot)', icon: 'sparkles', alias: 'labymod-snapshot', special: 'labymod-snapshot', channel: 'snapshot' },
];

const sortedVersions = computed(() => [...versions.value].sort((a, b) => dateValue(b.time) - dateValue(a.time)));
const releaseVersions = computed(() => sortedVersions.value.filter(version => groupOf(version) === 'release'));
const snapshotVersions = computed(() => sortedVersions.value.filter(version => groupOf(version) === 'snapshot'));
const oldVersions = computed(() => sortedVersions.value.filter(version => groupOf(version) === 'old'));
const latestVersions = computed(() => {
  const latestRelease = releaseVersions.value[0];
  const latestSnapshot = snapshotVersions.value[0];
  const list = [latestRelease];
  if (latestSnapshot && (!latestRelease || dateValue(latestSnapshot.time) > dateValue(latestRelease.time))) list.push(latestSnapshot);
  const latest = list.filter(Boolean) as Version[];
  // A few metadata mirrors omit the type field. Keep the latest card useful
  // instead of silently rendering an empty card in that case.
  return latest.length ? latest : sortedVersions.value.slice(0, 2);
});
const versionCategories = computed(() => [
  { id: 'release', label: t('pcl.installPage.release'), versions: releaseVersions.value },
  { id: 'snapshot', label: t('pcl.installPage.snapshot'), versions: snapshotVersions.value },
  { id: 'old', label: t('pcl.installPage.old'), versions: oldVersions.value },
].filter(category => category.versions.length));

const nameError = computed(() => {
  const value = instanceName.value.trim();
  if (!value) return t('pcl.installPage.nameRequired');
  if (/[\\/:*?"<>|]/.test(value)) return t('pcl.installPage.nameInvalid');
  if ((props.instances || []).some(instance => String(instance.name || '').trim().toLowerCase() === value.toLowerCase())) return t('pcl.installPage.nameDuplicate');
  return '';
});
const canInstall = computed(() => Boolean(selectedVersion.value) && !nameError.value && !installing.value && !props.busy);

watch([() => selectedVersion.value?.version, () => selectedLoader.value?.id, () => selectedLoader.value?.version], () => {
  if (!nameTouched.value) instanceName.value = generatedName();
});
watch(() => props.preset, preset => {
  if (!preset || step.value !== 'versions') return;
  // The preset is applied after the user chooses a Minecraft version.
});
watch(() => props.initialMinecraftVersion, initial => {
  const version = String(initial || '').trim();
  if (!version || loading.value || !versions.value.length) return;
  const selected = versions.value.find(entry => entry.version === version);
  if (selected && selectedVersion.value?.version !== selected.version) selectMinecraft(selected);
});

function groupOf(version: Version): 'release' | 'snapshot' | 'old' {
  const type = String(version.type || '').toLowerCase();
  if (type.includes('release')) return 'release';
  if (type.includes('snapshot')) return 'snapshot';
  return 'old';
}
function dateValue(value?: string): number {
  const time = Date.parse(String(value || ''));
  return Number.isFinite(time) ? time : 0;
}
function versionInfo(version: Version): string {
  const date = version.time ? new Date(version.time).toLocaleDateString() : t('pcl.installPage.unknownDate');
  return `${date} | ${version.version}`;
}
function generatedName(): string {
  if (!selectedVersion.value) return '';
  const base = selectedVersion.value.version;
  return selectedLoader.value ? `${base}-${selectedLoader.value.label}_${selectedLoader.value.version}` : base;
}
function isLoaderCompatible(loader: LoaderCard): boolean {
  // Compatibility is authoritative in component.versions. Hard-coding
  // ranges here made the Neo UI diverge from the metadata providers.
  return !loader.disabled && Boolean(selectedVersion.value?.version);
}
function isLoaderDisabled(loader: LoaderCard): boolean {
  return !isLoaderCompatible(loader);
}
function loaderStateLabel(loader: LoaderCard): string {
  if (loader.disabled) return t('pcl.installPage.unsupported');
  return t('pcl.installPage.incompatible');
}

function clearLoaderState() {
  for (const key of Object.keys(loaderState)) delete loaderState[key];
}

async function loadMinecraft() {
  if (props.active === false || loading.value) return;
  loading.value = true;
  loadError.value = '';
  loadErrorContext.value = {};
  try {
    // Keep the same metadata loading contract as the pre-refactor installer.
    // The native launcher may need to resolve the component list online and
    // then populate its cache; forcing offline here makes a fresh install show
    // an empty version list because the data directory does not contain the
    // legacy `meta/<component>.json` files.
    const result = await props.execute('component.versions', { uid: 'net.minecraft' });
    const entries = versionEntries(result)
      .map(normalizeVersion)
      .filter((version: Version) => version.version);
    if (!entries.length) throw new Error(t('pcl.installPage.noVersions'));
    versions.value = entries;
    const initial = String(props.initialMinecraftVersion || '').trim();
    const selected = initial ? versions.value.find(version => version.version === initial) : undefined;
    if (selected) selectMinecraft(selected);
  } catch (exception) {
    loadError.value = exception instanceof Error ? exception.message : String(exception);
    // Include the backend's resolved data directory in the visible diagnostic.
    // This is the first thing to verify when metadata exists in one profile but
    // the Web UI is connected to another one.
    try {
      const runtime = await props.execute('runtime.info');
      loadErrorContext.value = {
        dataRoot: runtime?.dataRoot,
        applicationRoot: runtime?.root || runtime?.programRoot,
        proxy: runtime?.proxy,
      };
    } catch { /* Keep the original backend error when diagnostics are unavailable. */ }
  } finally {
    loading.value = false;
  }
}
function selectMinecraft(version: Version) {
  selectedVersion.value = version;
  selectedLoader.value = null;
  clearLoaderState();
  openLoader.value = '';
  step.value = 'select';
  const preset = props.preset;
  if (preset) {
    const specialCard = loaderCards.find(loader => loader.alias === preset.alias && loader.special);
    if (specialCard) {
      selectedLoader.value = {
        id: specialCard.id,
        label: specialCard.label,
        version: preset.version,
        special: specialCard.special,
        channel: preset.channel || specialCard.channel,
        loaderVersion: preset.loaderVersion || preset.version,
        commitRef: preset.commitRef,
      };
      openLoader.value = specialCard.id;
      void toggleLoader(specialCard, true);
    } else {
      selectedLoader.value = { id: preset.alias, label: preset.label, version: preset.version };
    }
    emit('preset-consumed');
  }
  if (!nameTouched.value) instanceName.value = generatedName();
}
function backToVersions() {
  step.value = 'versions';
  error.value = '';
}
async function toggleLoader(loader: LoaderCard, open: boolean) {
  if (!open) {
    openLoader.value = '';
    return;
  }
  if (isLoaderDisabled(loader)) return;
  openLoader.value = loader.id;
  if (!loaderState[loader.id]) loaderState[loader.id] = { loading: false, error: '', versions: [] };
  if (loaderState[loader.id].versions.length) return;
  loaderState[loader.id].loading = true;
  loaderState[loader.id].error = '';
  try {
    const result = loader.special
      ? await props.execute('special.loader.versions', { loader: loader.special, minecraftVersion: selectedVersion.value?.version })
      : await props.execute('component.versions', { uid: loader.uid, minecraftVersion: selectedVersion.value?.version });
    loaderState[loader.id].versions = versionEntries(result)
      .map(normalizeVersion)
      .filter((version: Version) => version.version);
  } catch (exception) {
    loaderState[loader.id].error = exception instanceof Error ? exception.message : String(exception);
  } finally {
    loaderState[loader.id].loading = false;
  }
}
function selectLoader(loader: LoaderCard, version: Version) {
  selectedLoader.value = {
    id: loader.id,
    label: loader.label,
    version: version.version,
    special: loader.special,
    channel: version.channel || loader.channel,
    loaderVersion: version.loaderVersion || version.version,
    commitRef: version.commitRef,
  };
  openLoader.value = '';
}
function clearLoader() {
  selectedLoader.value = null;
  instanceName.value = nameTouched.value ? instanceName.value : generatedName();
}
async function install() {
  if (!selectedVersion.value || !canInstall.value) return;
  installing.value = true;
  error.value = '';
  const installInfo = { title: selectedLoader.value ? t('pcl.installPage.installMinecraftLoader') : t('pcl.installPage.installMinecraft'), name: instanceName.value.trim() || selectedVersion.value.version };
  emit('install-start', installInfo);
  let failure = '';
  try {
    const parameters: Record<string, unknown> = {
      name: instanceName.value.trim(),
      version: selectedVersion.value.version,
    };
    if (selectedLoader.value && !selectedLoader.value.special) {
      parameters.loader = selectedLoader.value.id;
      parameters.loaderVersion = selectedLoader.value.version;
    }
    let task: any = await props.execute('neo.task.start', {
      operation: 'instance.create',
      priority: 'high',
      parameters,
    });
    while (task && !isTaskItemFinished(task)) {
      emit('install-progress', task);
      await new Promise(resolve => setTimeout(resolve, 300));
      task = await props.execute('neo.task.status', { taskId: task.taskId });
    }
    emit('install-progress', task);
    if (!task || task.state !== 'succeeded') throw new Error(task?.error || t('pcl.installPage.createFailed'));
    let createdId = task.createdInstanceId || '';
    if (selectedLoader.value?.special) {
      if (!createdId) throw new Error(t('pcl.installPage.noCreatedId'));
      task = await props.execute('neo.task.start', {
        operation: 'instance.special-loader.install',
        priority: 'high',
        parameters: {
          instance: createdId,
          loader: selectedLoader.value.special,
          minecraftVersion: selectedVersion.value.version,
          ...(selectedLoader.value.loaderVersion ? { loaderVersion: selectedLoader.value.loaderVersion } : {}),
          ...(selectedLoader.value.commitRef ? { commitRef: selectedLoader.value.commitRef } : {}),
        },
      });
      while (task && !isTaskItemFinished(task)) {
        emit('install-progress', task);
        await new Promise(resolve => setTimeout(resolve, 300));
        task = await props.execute('neo.task.status', { taskId: task.taskId });
      }
      emit('install-progress', task);
      if (!task || task.state !== 'succeeded') throw new Error(task?.error || t('pcl.installPage.specialFailed'));
    }
    emit('installed', { id: createdId, name: instanceName.value.trim() || selectedVersion.value.version });
  } catch (exception) {
    failure = exception instanceof Error ? exception.message : String(exception);
    error.value = failure;
  } finally {
    installing.value = false;
    emit('install-done', failure ? { ...installInfo, error: failure } : installInfo);
  }
}

onMounted(() => { if (props.active !== false) loadMinecraft(); });
watch(() => props.active, active => { if (active && !versions.value.length) void loadMinecraft(); });

provide(PclInstallContextKey, { t, props, emit, busy: toRef(props, 'busy'), execute: props.execute, instances: props.instances, preset: props.preset, initialMinecraftVersion: props.initialMinecraftVersion, step, loading, loadError, loadErrorContext, installing, error, versions, selectedVersion, selectedLoader, openLoader, instanceName, nameTouched, categoryOpen, loaderState, loaderCards, sortedVersions, releaseVersions, snapshotVersions, oldVersions, latestVersions, versionCategories, nameError, canInstall, groupOf, versionInfo, generatedName, isLoaderCompatible, isLoaderDisabled, loaderStateLabel, clearLoaderState, loadMinecraft, selectMinecraft, backToVersions, toggleLoader, selectLoader, clearLoader, install });
</script>

<style>
.pcl-install-view {
  position: relative;
  min-height: 100%;
}

.pcl-install-step {
  padding-bottom: 96px;
}

.pcl-version-list {
  max-height: 360px;
  overflow-y: auto;
}

.pcl-install-header {
  margin-bottom: 15px;
}

.pcl-install-header__row {
  display: flex;
  align-items: center;
  gap: 12px;
  min-height: 62px;
}

.pcl-install-header__logo {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 38px;
  height: 38px;
  color: var(--pcl-c3);
}

.pcl-install-header__copy {
  flex: 1 1 auto;
  min-width: 0;
}

.pcl-install-header__eyebrow {
  margin-bottom: 3px;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-install-name {
  height: 30px !important;
  border-color: transparent !important;
  background: transparent !important;
  padding-left: 0 !important;
  font-size: calc(15px * var(--pcl-font-scale, 1)) !important;
  font-weight: 700;
}

.pcl-install-name:hover,
.pcl-install-name:focus {
  border-color: var(--pcl-c5) !important;
  background: var(--pcl-half-white) !important;
  padding-left: 8px !important;
}

.pcl-install-header__hint {
  flex: 0 0 auto;
  color: var(--pcl-c3);
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-loader-title {
  display: inline-flex;
  align-items: center;
  gap: 8px;
}

.pcl-loader-selected {
  max-width: 150px;
  overflow: hidden;
  color: var(--pcl-c3);
  font-weight: 400;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-loader-unavailable {
  color: var(--pcl-gray4);
  font-weight: 400;
}

.pcl-loader-add {
  color: var(--pcl-muted);
  font-weight: 400;
}

.pcl-loader-list {
  max-height: 260px;
  overflow-y: auto;
}

.pcl-install-footer {
  position: sticky;
  bottom: 0;
  z-index: 4;
  display: flex;
  justify-content: center;
  padding: 18px 0 16px;
  background: linear-gradient(180deg, transparent, var(--pcl-page-bg) 38%);
}

@media (max-height: 620px) {
  .pcl-version-list,
  .pcl-loader-list {
    max-height: 240px;
  }
}
</style>
