<template>
  <div class="pcl-install-view">
    <Transition name="pcl-install-step" mode="out-in">
      <div v-if="step === 'versions'" key="versions" class="pcl-install-step">
        <PclCard v-if="loading">
          <PclLoading text="正在加载 Minecraft 版本列表" />
        </PclCard>

        <PclCard v-else-if="loadError">
          <PclHint theme="red">{{ loadError }}</PclHint>
          <div class="pcl-actions">
            <PclButton variant="highlight" icon="refresh-cw" @click="loadMinecraft">重试</PclButton>
          </div>
        </PclCard>

        <template v-else>
          <PclCard v-model:open="categoryOpen.latest" title="最新版本">
            <div class="pcl-version-list">
              <PclListItem
                v-for="version in latestVersions"
                :key="version.version"
                :title="version.version"
                :info="versionInfo(version)"
                :tags="version.recommended ? ['推荐'] : []"
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
                :tags="version.recommended ? ['推荐'] : []"
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
            <PclIconButton icon="arrow-left" label="返回版本选择" :size="26" @click="backToVersions" />
            <span class="pcl-install-header__logo"><PclIcon name="boxes" :size="32" /></span>
            <div class="pcl-install-header__copy">
              <div class="pcl-install-header__eyebrow">Minecraft {{ selectedVersion?.version }}</div>
              <PclInput
                v-model="instanceName"
                placeholder="实例名称"
                class="pcl-install-name"
                @update:model-value="nameTouched = true"
              />
            </div>
            <span class="pcl-install-header__hint">{{ selectedLoader ? selectedLoader.label : '原版' }}</span>
          </div>
        </PclCard>

        <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>
        <PclHint v-if="nameError" theme="red">{{ nameError }}</PclHint>
        <PclHint v-if="selectedLoader?.id === 'fabric'" theme="blue">
          Fabric API 不包含在客户端安装中，可在实例创建后从资源页安装。
        </PclHint>
        <PclHint v-if="selectedLoader?.id === 'forge'" theme="yellow">
          OptiFine 与 Forge 的组合需要额外补丁，Luna 暂不支持自动安装。
        </PclHint>
        <PclHint theme="yellow">
          Luna 当前支持 Forge、NeoForge、Fabric 与 Cleanroom。OptiFine、LabyMod、LiteLoader、Legacy Fabric 及加载器组合暂未接入。
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
            <span v-else class="pcl-loader-add">添加</span>
            <PclIconButton
              v-if="selectedLoader?.id === loader.id"
              icon="x"
              label="清除加载器"
              :size="22"
              @click.stop="clearLoader"
            />
          </template>

          <PclLoading v-if="loaderState[loader.id]?.loading" text="正在读取版本" inline />
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
              没有与 Minecraft {{ selectedVersion?.version }} 兼容的版本。
            </div>
          </div>
        </PclCard>

        <div class="pcl-install-footer">
          <PclButton variant="accent" size="xl" icon="download" :disabled="!canInstall || installing" @click="install">
            {{ installing ? '正在创建实例' : '开始下载' }}
          </PclButton>
        </div>
      </div>
    </Transition>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, reactive, ref, watch } from 'vue';
import PclCard from '../../components/PclCard.vue';
import PclButton from '../../components/PclButton.vue';
import PclHint from '../../components/PclHint.vue';
import PclInput from '../../components/PclInput.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
type Version = { version: string; type?: string; time?: string; recommended?: boolean; requires?: any[] };
type LoaderCard = { id: string; label: string; icon: string; uid: string; alias: string; disabled?: boolean };

const props = defineProps<{
  execute: Execute;
  busy: boolean;
  instances?: any[];
  preset?: { label: string; version: string; alias: string } | null;
}>();
const emit = defineEmits<{ installed: [result: any]; 'preset-consumed': []; 'install-start': [info: any]; 'install-progress': [task: any]; 'install-done': [info: any] }>();

const step = ref<'versions' | 'select'>('versions');
const loading = ref(false), loadError = ref(''), installing = ref(false), error = ref('');
const versions = ref<Version[]>([]);
const selectedVersion = ref<Version | null>(null);
const selectedLoader = ref<{ id: string; label: string; version: string } | null>(null);
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
  { id: 'legacy-fabric', label: 'Legacy Fabric', icon: 'scroll', uid: 'net.legacyfabric.loader', alias: 'legacyfabric', disabled: true },
  { id: 'fabric-api', label: 'Fabric API', icon: 'package-plus', uid: '', alias: '', disabled: true },
  { id: 'legacy-fabric-api', label: 'Legacy Fabric API', icon: 'package-plus', uid: '', alias: '', disabled: true },
  { id: 'labymod', label: 'LabyMod', icon: 'box', uid: '', alias: '', disabled: true },
  { id: 'optifine', label: 'OptiFine', icon: 'gauge', uid: '', alias: '', disabled: true },
  { id: 'optifabric', label: 'OptiFabric', icon: 'sparkles', uid: '', alias: '', disabled: true },
  { id: 'liteloader', label: 'LiteLoader', icon: 'egg', uid: '', alias: 'liteloader', disabled: true },
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
  { id: 'release', label: '正式版', versions: releaseVersions.value },
  { id: 'snapshot', label: '快照', versions: snapshotVersions.value },
  { id: 'old', label: '旧版本', versions: oldVersions.value },
].filter(category => category.versions.length));

const nameError = computed(() => {
  const value = instanceName.value.trim();
  if (!value) return '请输入实例名称。';
  if (/[\\/:*?"<>|]/.test(value)) return '实例名称不能包含 \\ / : * ? " < > |。';
  if ((props.instances || []).some(instance => String(instance.name || '').trim().toLowerCase() === value.toLowerCase())) return '已存在同名实例，请换一个名称。';
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
  const date = version.time ? new Date(version.time).toLocaleDateString('zh-CN') : '未知日期';
  return `${date} | ${version.version}`;
}
function generatedName(): string {
  if (!selectedVersion.value) return '';
  const base = selectedVersion.value.version;
  return selectedLoader.value ? `${base}-${selectedLoader.value.label}_${selectedLoader.value.version}` : base;
}
function compareMinecraft(left: string, right: string): number {
  const a = left.split(/[^0-9]+/).filter(Boolean).map(Number);
  const b = right.split(/[^0-9]+/).filter(Boolean).map(Number);
  for (let index = 0; index < Math.max(a.length, b.length); index += 1) {
    const difference = (a[index] || 0) - (b[index] || 0);
    if (difference) return difference;
  }
  return 0;
}
function isLoaderCompatible(loader: LoaderCard): boolean {
  const version = selectedVersion.value?.version || '';
  if (loader.disabled) return false;
  if (loader.id === 'cleanroom') return version === '1.12.2';
  if (loader.id === 'neoforge') return compareMinecraft(version, '1.20.1') >= 0;
  if (loader.id === 'fabric') return compareMinecraft(version, '1.14') >= 0;
  return true;
}
function isLoaderDisabled(loader: LoaderCard): boolean {
  return !isLoaderCompatible(loader);
}
function loaderStateLabel(loader: LoaderCard): string {
  if (loader.disabled) return '暂不支持';
  return '不兼容';
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
  } catch (exception) {
    loadError.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    loading.value = false;
  }
}
function selectMinecraft(version: Version) {
  selectedVersion.value = version;
  selectedLoader.value = null;
  openLoader.value = '';
  step.value = 'select';
  if (props.preset) {
    selectedLoader.value = { id: props.preset.alias, label: props.preset.label, version: props.preset.version };
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
    const result = await props.execute('component.versions', {
      uid: loader.uid,
      minecraftVersion: selectedVersion.value?.version,
    });
    loaderState[loader.id].versions = (result?.versions || []).map((version: any) => ({
      version: String(version.version || ''),
      type: version.type,
      time: version.time,
      recommended: Boolean(version.recommended),
    })).filter((version: Version) => version.version);
  } catch (exception) {
    loaderState[loader.id].error = exception instanceof Error ? exception.message : String(exception);
  } finally {
    loaderState[loader.id].loading = false;
  }
}
function selectLoader(loader: LoaderCard, version: Version) {
  selectedLoader.value = { id: loader.id, label: loader.label, version: version.version };
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
  const installInfo = { title: selectedLoader.value ? '正在安装 Minecraft 与加载器' : '正在安装 Minecraft', name: instanceName.value.trim() || selectedVersion.value.version };
  emit('install-start', installInfo);
  try {
    const parameters: Record<string, unknown> = {
      name: instanceName.value.trim(),
      version: selectedVersion.value.version,
    };
    if (selectedLoader.value) {
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
    if (!task || task.state !== 'succeeded') throw new Error(task?.error || '实例创建失败');
    emit('installed', { id: task.createdInstanceId || '', name: instanceName.value.trim() || selectedVersion.value.version });
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
  font-size: 11px;
}

.pcl-install-name {
  height: 30px !important;
  border-color: transparent !important;
  background: transparent !important;
  padding-left: 0 !important;
  font-size: 15px !important;
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
  font-size: 12px;
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
