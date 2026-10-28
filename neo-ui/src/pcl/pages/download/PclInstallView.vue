<template>
  <div class="pcl-install-view">
    <Transition name="pcl-install-step" mode="out-in">
      <div v-if="step === 'versions'" key="versions" class="pcl-install-step">
        <PclCard v-if="loading">
          <PclLoading :text="t('pcl.installPage.loadingVersions')" />
        </PclCard>

        <PclCard v-else-if="loadError">
          <PclHint theme="red">{{ loadError }}</PclHint>
          <div class="pcl-actions">
            <PclButton variant="highlight" icon="refresh-cw" @click="loadMinecraft">{{ t('common.retry') }}</PclButton>
          </div>
        </PclCard>

        <template v-else>
          <PclCard v-model:open="categoryOpen.latest" :title="t('pcl.installPage.latest')">
            <div class="pcl-version-list">
              <PclListItem
                v-for="version in latestVersions"
                :key="version.version"
                :title="version.version"
                :info="versionInfo(version)"
                :tags="version.recommended ? [t('download.recommended')] : []"
                @click="selectMinecraft(version)"
              >
                <template #icon><PclIcon name="boxes" :size="21" /></template>
              </PclListItem>
            </div>
          </PclCard>

          <PclCard
            v-for="category in versionCategories"
            :key="category.id"
            v-model:open="categoryOpen[category.id]"
            collapsible
            :title="`${category.label} (${category.versions.length})`"
          >
            <div class="pcl-version-list">
              <PclListItem
                v-for="version in category.versions"
                :key="version.version"
                :title="version.version"
                :info="versionInfo(version)"
                :tags="version.recommended ? [t('download.recommended')] : []"
                @click="selectMinecraft(version)"
              >
                <template #icon><PclIcon name="boxes" :size="21" /></template>
              </PclListItem>
            </div>
          </PclCard>
        </template>
      </div>

      <div v-else key="select" class="pcl-install-step">
        <PclCard class="pcl-install-header">
          <div class="pcl-install-header__row">
            <PclIconButton icon="arrow-left" :label="t('pcl.installPage.backVersions')" :size="26" @click="backToVersions" />
            <span class="pcl-install-header__logo"><PclIcon name="boxes" :size="32" /></span>
            <div class="pcl-install-header__copy">
              <div class="pcl-install-header__eyebrow">Minecraft {{ selectedVersion?.version }}</div>
              <PclInput
                v-model="instanceName"
                :placeholder="t('app.instanceName')"
                class="pcl-install-name"
                @update:model-value="nameTouched = true"
              />
            </div>
            <span class="pcl-install-header__hint">{{ selectedLoader ? selectedLoader.label : t('pcl.installPage.vanilla') }}</span>
          </div>
        </PclCard>

        <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>
        <PclHint v-if="nameError" theme="red">{{ nameError }}</PclHint>
        <PclHint v-if="selectedLoader?.id === 'fabric'" theme="blue">
          {{ t('pcl.installPage.fabricHint') }}
        </PclHint>
        <PclHint v-if="selectedLoader?.id === 'forge'" theme="yellow">
          {{ t('pcl.installPage.forgeHint') }}
        </PclHint>
        <PclHint theme="blue">
          {{ t('pcl.installPage.loaderHint') }}
        </PclHint>

        <PclCard
          v-for="loader in loaderCards"
          :key="loader.id"
          collapsible
          :disabled="isLoaderDisabled(loader)"
          :open="openLoader === loader.id"
          @update:open="toggleLoader(loader, $event)"
        >
          <template #title>
            <span class="pcl-loader-title">
              <PclIcon :name="loader.icon" :size="18" />
              <span>{{ loader.label }}</span>
            </span>
          </template>
          <template #actions>
            <span v-if="selectedLoader?.id === loader.id" class="pcl-loader-selected">{{ selectedLoader.version }}</span>
            <span v-else-if="isLoaderDisabled(loader)" class="pcl-loader-unavailable">{{ loaderStateLabel(loader) }}</span>
            <span v-else class="pcl-loader-add">{{ t('pcl.installPage.addLoader') }}</span>
            <PclIconButton
              v-if="selectedLoader?.id === loader.id"
              icon="x"
              :label="t('pcl.installPage.clearLoader')"
              :size="22"
              @click.stop="clearLoader"
            />
          </template>

          <PclLoading v-if="loaderState[loader.id]?.loading" :text="t('pcl.installPage.readingVersions')" inline />
          <PclHint v-else-if="loaderState[loader.id]?.error" theme="red">{{ loaderState[loader.id].error }}</PclHint>
          <div v-else class="pcl-loader-list">
            <PclListItem
              v-for="version in loaderState[loader.id]?.versions || []"
              :key="version.version"
              :title="version.version"
              :info="versionInfo(version)"
              :selected="selectedLoader?.id === loader.id && selectedLoader.version === version.version"
              @click="selectLoader(loader, version)"
            >
              <template #icon><PclIcon :name="loader.icon" :size="19" /></template>
            </PclListItem>
            <div v-if="!(loaderState[loader.id]?.versions || []).length" class="pcl-empty">
              {{ t('pcl.installPage.noCompatible', { version: selectedVersion?.version }) }}
            </div>
          </div>
        </PclCard>

        <div class="pcl-install-footer">
          <PclButton variant="accent" size="xl" icon="download" :disabled="!canInstall || installing" @click="install">
            {{ installing ? t('pcl.installPage.creating') : t('pcl.installPage.startDownload') }}
          </PclButton>
        </div>
      </div>
    </Transition>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, reactive, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import PclCard from '../../components/PclCard.vue';
import PclButton from '../../components/PclButton.vue';
import PclHint from '../../components/PclHint.vue';
import PclInput from '../../components/PclInput.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
type Version = { version: string; type?: string; time?: string; recommended?: boolean; requires?: any[]; minecraftVersion?: string; loaderVersion?: string; commitRef?: string; channel?: string; runtime?: any };
type LoaderCard = { id: string; label: string; icon: string; uid?: string; alias: string; special?: string; channel?: string; disabled?: boolean };

const props = defineProps<{
  execute: Execute;
  busy: boolean;
  instances?: any[];
  preset?: { label: string; version: string; alias: string; special?: string; channel?: string; loaderVersion?: string; commitRef?: string } | null;
  initialMinecraftVersion?: string;
}>();
const emit = defineEmits<{ installed: [result: any]; 'preset-consumed': []; 'install-start': [info: any]; 'install-progress': [task: any]; 'install-done': [info: any] }>();
const { t } = useI18n();

const step = ref<'versions' | 'select'>('versions');
const loading = ref(false), loadError = ref(''), installing = ref(false), error = ref('');
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
  return list.filter(Boolean) as Version[];
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
  if (loading.value) return;
  loading.value = true;
  loadError.value = '';
  try {
    const result = await props.execute('component.versions', { uid: 'net.minecraft' });
    versions.value = (result?.versions || []).map((version: any) => ({
      version: String(version.version || ''),
      type: version.type,
      time: version.time,
      recommended: Boolean(version.recommended),
      requires: version.requires || [],
    })).filter((version: Version) => version.version);
    const initial = String(props.initialMinecraftVersion || '').trim();
    const selected = initial ? versions.value.find(version => version.version === initial) : undefined;
    if (selected) selectMinecraft(selected);
  } catch (exception) {
    loadError.value = exception instanceof Error ? exception.message : String(exception);
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
    loaderState[loader.id].versions = (result?.versions || []).map((version: any) => ({
      version: String(version.version || ''),
      type: version.type,
      time: version.time,
      recommended: Boolean(version.recommended),
      minecraftVersion: version.minecraftVersion,
      loaderVersion: version.loaderVersion,
      commitRef: version.commitRef,
      channel: version.channel,
      runtime: version.runtime,
    })).filter((version: Version) => version.version);
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
    while (task && !task.finished) {
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
      while (task && !task.finished) {
        emit('install-progress', task);
        await new Promise(resolve => setTimeout(resolve, 300));
        task = await props.execute('neo.task.status', { taskId: task.taskId });
      }
      emit('install-progress', task);
      if (!task || task.state !== 'succeeded') throw new Error(task?.error || t('pcl.installPage.specialFailed'));
    }
    emit('installed', { id: createdId, name: instanceName.value.trim() || selectedVersion.value.version });
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    installing.value = false;
    emit('install-done', installInfo);
  }
}

onMounted(loadMinecraft);
</script>

<style scoped>
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

.pcl-install-step-enter-active {
  transition: opacity .1s ease, transform .16s cubic-bezier(.16, 1, .3, 1);
}

.pcl-install-step-leave-active {
  transition: opacity .07s ease, transform .09s ease;
}

.pcl-install-step-enter-from {
  opacity: 0;
  transform: translateX(40px);
}

.pcl-install-step-leave-to {
  opacity: 0;
  transform: translateX(-50px);
}

@media (max-height: 620px) {
  .pcl-version-list,
  .pcl-loader-list {
    max-height: 240px;
  }
}
</style>
