<template>
  <PclCard :title="kindLabel" class="pcl-resource-view">
    <div class="pcl-resource-toolbar">
      <PclInput v-model="query" :placeholder="t('resource.filter')" class="pcl-resource-query" @enter="load" />
      <div class="pcl-resource-view-toggle">
        <PclIconButton icon="grid-2x2" :label="t('resource.grid')" :size="28" :class="{ active: viewMode === 'grid' }" @click="setView('grid')" />
        <PclIconButton icon="layout-list" :label="t('resource.list')" :size="28" :class="{ active: viewMode === 'list' }" @click="setView('list')" />
      </div>
      <PclButton v-if="catalogKind" size="sm" variant="highlight" icon="download" :disabled="busy" @click="openDownload">{{ t('resource.download', { kind: kindLabel }) }}</PclButton>
      <PclButton size="sm" icon="refresh-cw" :disabled="busy || loading" @click="load">{{ t('resource.refresh') }}</PclButton>
      <PclButton size="sm" icon="rotate-ccw" :disabled="busy || loading" @click="update">{{ t('resource.rescan') }}</PclButton>
      <PclButton size="sm" icon="folder-open" :disabled="busy" @click="openFolder">{{ t('resource.openFolder') }}</PclButton>
    </div>

    <div class="pcl-resource-install">
      <PclInput v-model="installSource" :placeholder="t('resource.source')" @enter="install" />
      <PclIconButton icon="folder-open" :label="t('resource.chooseFile')" :size="28" :disabled="busy || localBusy" @click="chooseInstallFile" />
      <PclButton variant="highlight" icon="plus" :disabled="busy || localBusy || !installSource.trim()" @click="install">{{ t('resource.install') }}</PclButton>
    </div>

    <div v-if="selectedCount" class="pcl-resource-batch">
      <span>{{ t('resource.selected', { count: selectedCount }) }}</span>
      <PclButton size="sm" :disabled="busy || localBusy" @click="setSelected(true)">{{ t('resource.enable') }}</PclButton>
      <PclButton size="sm" :disabled="busy || localBusy" @click="setSelected(false)">{{ t('resource.disable') }}</PclButton>
      <PclButton size="sm" variant="red" :disabled="busy || localBusy" @click="removeSelected">{{ t('resource.delete') }}</PclButton>
      <PclButton size="sm" variant="ghost" @click="clearSelected">{{ t('resource.clearSelection') }}</PclButton>
    </div>

    <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>
    <PclLoading v-if="loading || localBusy" :text="t('resource.loading')" inline />

    <div v-if="viewMode === 'grid'" class="pcl-resource-grid">
      <article
        v-for="item in filtered"
        :key="item.id || item.path || item.name"
        class="pcl-resource-card"
        :class="{ disabled: item.enabled === false, selected: selected.has(resourceRef(item)) }"
      >
        <PclCheckbox
          class="pcl-resource-check"
          :model-value="selected.has(resourceRef(item))"
          @update:model-value="toggleSelected(item)"
        />
        <button class="pcl-resource-icon" :title="t('resource.inspect', { name: item.name || item.fileName || item.path })" @click="inspect(item)">
          <img v-if="item.icon" :src="item.icon" alt="" />
          <PclIcon v-else :name="icon" :size="24" />
        </button>
        <div class="pcl-resource-copy" @click="inspect(item)">
          <strong>{{ item.name || item.fileName || item.path || t('resource.unnamed') }}</strong>
          <small v-if="item.version || item.metadata?.version">{{ item.version || item.metadata?.version }}</small>
          <small>{{ item.enabled === false ? t('resource.disabled') : item.size ? formatSize(item.size) : statusLabel(item) }}</small>
        </div>
        <div class="pcl-resource-actions">
          <PclIconButton
            :icon="item.enabled === false ? 'circle-check' : 'power'"
            :label="item.enabled === false ? t('resource.enable') : t('resource.disable')"
            :size="26"
            :disabled="busy || localBusy"
            @click="setEnabled(item, item.enabled === false)"
          />
          <PclIconButton icon="trash-2" :label="t('resource.delete')" :size="26" danger :disabled="busy || localBusy" @click="remove(item)" />
        </div>
      </article>

      <div v-if="!filtered.length && !loading" class="pcl-empty pcl-resource-empty">
        <PclIcon name="package" :size="38" />
        <strong>{{ query ? t('resource.noMatch') : t('resource.noResource') }}</strong>
        <span>{{ query ? t('resource.adjustFilter') : t('resource.installHint') }}</span>
      </div>
    </div>

    <div v-else class="pcl-resource-table">
      <div class="pcl-resource-table__head">
        <span />
        <span>{{ t('resource.name') }}</span>
        <span>{{ t('resource.version') }}</span>
        <span>{{ t('resource.size') }}</span>
        <span>{{ t('resource.status') }}</span>
        <span />
      </div>
      <div
        v-for="item in filtered"
        :key="item.id || item.path || item.name"
        class="pcl-resource-row"
        :class="{ disabled: item.enabled === false, selected: selected.has(resourceRef(item)) }"
      >
        <PclCheckbox :model-value="selected.has(resourceRef(item))" @update:model-value="toggleSelected(item)" />
        <button class="pcl-resource-row__name" type="button" @click="inspect(item)">
          <span class="pcl-resource-row__icon">
            <img v-if="item.icon" :src="item.icon" alt="" />
            <PclIcon v-else :name="icon" :size="18" />
          </span>
          <span class="pcl-resource-row__copy">
            <strong>{{ item.name || item.fileName || item.path || t('resource.unnamed') }}</strong>
          </span>
        </button>
        <span class="pcl-resource-row__meta">{{ item.version || item.metadata?.version || '—' }}</span>
        <span class="pcl-resource-row__meta">{{ item.size ? formatSize(item.size) : '—' }}</span>
        <span class="pcl-resource-row__status" :class="{ off: item.enabled === false }">
          {{ item.enabled === false ? t('account.disabled') : t('resource.enabled') }}
        </span>
        <span class="pcl-resource-row__actions">
          <PclIconButton
            :icon="item.enabled === false ? 'circle-check' : 'power'"
            :label="item.enabled === false ? t('resource.enable') : t('resource.disable')"
            :size="24"
            :disabled="busy || localBusy"
            @click="setEnabled(item, item.enabled === false)"
          />
          <PclIconButton icon="trash-2" :label="t('resource.delete')" :size="24" danger :disabled="busy || localBusy" @click="remove(item)" />
        </span>
      </div>
      <div v-if="!filtered.length && !loading" class="pcl-empty pcl-resource-empty">
        <PclIcon name="package" :size="38" />
        <strong>{{ query ? t('resource.noMatch') : t('resource.noResource') }}</strong>
        <span>{{ query ? t('resource.adjustFilter') : t('resource.installHint') }}</span>
      </div>
    </div>
  </PclCard>

  <q-dialog v-model="detailOpen">
    <q-card class="pcl-dialog pcl-resource-detail">
      <q-card-section class="pcl-resource-detail__head">
        <div>
          <div class="pcl-kicker">{{ t('resource.detail') }}</div>
          <h3>{{ detail?.name || detail?.fileName || detail?.path || t('resource.detail') }}</h3>
        </div>
        <PclIconButton icon="x" :label="t('resource.close')" :size="28" @click="detailOpen = false" />
      </q-card-section>
      <q-card-section v-if="detailLoading"><PclLoading :text="t('resource.loadingDetail')" inline /></q-card-section>
      <PclHint v-else-if="detailError" theme="red">{{ detailError }}</PclHint>
      <q-card-section v-else class="pcl-resource-detail__body">
        <img v-if="detail?.icon" class="pcl-resource-detail__icon" :src="detail.icon" alt="" />
        <div class="pcl-resource-detail__meta">
          <template v-for="entry in detailEntries" :key="entry[0]">
            <span>{{ entry[0] }}</span><code>{{ entry[1] }}</code>
          </template>
        </div>
        <pre v-if="detail?.description" class="pcl-resource-detail__description">{{ detail.description }}</pre>
      </q-card-section>
      <q-card-actions align="right">
        <PclButton variant="ghost" @click="detailOpen = false">{{ t('resource.close') }}</PclButton>
        <PclButton variant="highlight" icon="refresh-cw" :disabled="busy || localBusy" @click="update">{{ t('resource.rescan') }}</PclButton>
      </q-card-actions>
    </q-card>
  </q-dialog>
  <q-dialog v-model="downloadOpen">
    <q-card class="pcl-dialog pcl-resource-download">
      <q-card-section class="pcl-resource-download__head">
        <div>
          <div class="pcl-kicker">{{ t('resource.downloadTitle') }}</div>
          <h3>{{ t('resource.download', { kind: kindLabel }) }}</h3>
        </div>
        <PclIconButton icon="x" :label="t('resource.close')" :size="28" @click="downloadOpen = false" />
      </q-card-section>
      <q-card-section class="pcl-resource-download__body">
        <PclCatalogBrowser
          :instance="instance"
          :kind="catalogKind"
          :busy="busy"
          :execute="execute"
          @installed="onDownloadInstalled"
          @install-start="emit('install-start', $event)"
          @install-progress="emit('install-progress', $event)"
          @install-done="emit('install-done', $event)"
        />
      </q-card-section>
    </q-card>
  </q-dialog>

</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclCatalogBrowser from '../download/PclCatalogBrowser.vue';
import { launcher } from '../../../boot/launcher-api';
import { pickPclFile } from '../../composables/usePclFileDialog';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ instance: string; kind: string; busy: boolean; execute: Execute }>();
const { t } = useI18n();
const emit = defineEmits<{ 'install-start': [info: any]; 'install-progress': [task: any]; 'install-done': [info: any] }>();
const query = ref(''), installSource = ref(''), resources = ref<any[]>([]), loading = ref(false), localBusy = ref(false), error = ref('');
type ResourceView = 'grid' | 'list';
const viewMode = ref<ResourceView>((() => { try { return localStorage.getItem('pcl-resource-view') === 'list' ? 'list' : 'grid'; } catch { return 'grid'; } })());
const folderKind = computed(() => ({
  mods: 'mods', coremods: 'coremods', nilmods: 'nilmods',
  resourcepacks: 'resourcepacks', texturepacks: 'texturepacks', shaderpacks: 'shaderpacks',
  datapacks: 'datapacks', yesstevemodels: 'yesstevemodels',
  customplayermodels: 'customplayermodels', schematics: 'schematics', screenshots: 'screenshots',
} as Record<string, string>)[props.kind] || 'root');
// The catalog API installs resource files into an instance resource folder.
// World saves use a separate folder layout and are intentionally handled by
// the file manager instead of being sent as a normal resource kind.
const catalogKinds = new Set(['mods', 'resourcepacks', 'shaderpacks', 'datapacks', 'texturepacks']);
const catalogKind = computed(() => catalogKinds.has(props.kind) ? ({ texturepacks: 'resourcepacks' } as Record<string, string>)[props.kind] || props.kind : '');
const downloadOpen = ref(false);
let resourceSubscription = '';
let unlistenResourceStream: (() => void) | undefined;
let resourceReloadTimer: ReturnType<typeof setTimeout> | undefined;
let resourceRequestToken = 0;
async function openDownload() {
  try {
    await launcher.openResourceDownload(props.instance, catalogKind.value, kindLabel.value);
  } catch (exception) {
    // The dedicated window is the normal desktop path. Keep the embedded
    // catalog only for a browser preview where Tauri is unavailable.
    if (!(window as any).__TAURI__) downloadOpen.value = true;
    else error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

function scheduleResourceReload() {
  if (resourceReloadTimer) clearTimeout(resourceReloadTimer);
  resourceReloadTimer = setTimeout(() => void load(), 250);
}

async function subscribeResources() {
  try {
    const subscription: any = await props.execute('instance.resources.subscribe', { instance: props.instance });
    resourceSubscription = String(subscription?.subscriptionId || '');
    if (!resourceSubscription) return;
    unlistenResourceStream = await launcher.onStream((batch: any) => {
      if (!batch || batch.subscriptionId !== resourceSubscription) return;
      for (const event of batch.events || []) {
        if (event.kind === 'resource.changed') scheduleResourceReload();
      }
    });
  } catch {
    // Resource changes are an enhancement; manual refresh still works.
  }
}

async function unsubscribeResources() {
  unlistenResourceStream?.();
  unlistenResourceStream = undefined;
  if (!resourceSubscription) return;
  const subscriptionId = resourceSubscription;
  resourceSubscription = '';
  try { await props.execute('event.unsubscribe', { subscriptionId }); } catch { /* Already gone. */ }
}

function onDownloadInstalled() {
  downloadOpen.value = false;
  void load();
}


async function openFolder() {
  try {
    const result = await props.execute('instance.folder-path', { instance: props.instance, kind: folderKind.value });
    if (result?.path) await launcher.openPath(result.path);
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

function setView(mode: ResourceView) {
  viewMode.value = mode;
  try { localStorage.setItem('pcl-resource-view', mode); } catch { /* storage may be unavailable */ }
}
const selected = ref(new Set<string>()), detailOpen = ref(false), detail = ref<any>(null), detailLoading = ref(false), detailError = ref('');

const labels: Record<string, string> = {
  mods: 'mods', coremods: 'coremods', nilmods: 'nilmods', resourcepacks: 'resourcepacks', texturepacks: 'texturepacks',
  shaderpacks: 'shaderpacks', datapacks: 'datapacks', yesstevemodels: 'yesstevemodels', customplayermodels: 'customplayermodels',
  schematics: 'schematics', plugins: 'plugins',
};
const icons: Record<string, string> = {
  mods: 'puzzle', coremods: 'package', nilmods: 'package-plus', resourcepacks: 'palette', texturepacks: 'image',
  shaderpacks: 'sparkles', datapacks: 'layers', yesstevemodels: 'shirt', customplayermodels: 'user',
  schematics: 'grid-2x2', plugins: 'puzzle',
};
const kindLabel = computed(() => {
  const key = labels[props.kind];
  return key ? t(`resource.kinds.${key}`) : props.kind;
});
const icon = computed(() => icons[props.kind] || 'package');
const filtered = computed(() => {
  const value = query.value.trim().toLowerCase();
  return resources.value.filter(item => !value || `${item.name || ''} ${item.path || ''} ${item.fileName || ''} ${item.version || ''}`.toLowerCase().includes(value));
});
const selectedCount = computed(() => selected.value.size);
const detailEntries = computed(() => {
  const item = detail.value || {};
  return [
    [t('resource.detailFile'), item.path || item.fileName || '—'],
    [t('resource.detailVersion'), item.version || item.metadata?.version || '—'],
    [t('resource.detailStatus'), statusLabel(item)],
    [t('resource.detailType'), item.type || '—'],
    [t('resource.detailLoader'), Array.isArray(item.loaders) ? item.loaders.join(', ') : (item.loaders || '—')],
    [t('resource.detailMinecraft'), Array.isArray(item.minecraftVersions) ? item.minecraftVersions.join(', ') : (item.minecraftVersion || '—')],
  ];
});

function resourceRef(item: any) {
  return String(item.id || item.path || item.fileName || item.name || '');
}

async function loadResourceIcons(requestToken: number) {
  // Only mods carry an embedded icon in the current resource model. Keep the
  // list request cheap and hydrate those icons after the rows are visible.
  if (props.kind !== 'mods') return;
  const items = resources.value.slice();
  let next = 0;
  const worker = async () => {
    while (next < items.length) {
      const item = items[next++];
      for (let attempt = 0; attempt < 4; attempt++) {
        try {
          const result = await props.execute('resource.icon', {
            instance: props.instance,
            kind: props.kind,
            resource: resourceRef(item),
          });
          if (requestToken !== resourceRequestToken) return;
          if (result?.icon) {
            const current = resources.value.find(candidate => resourceRef(candidate) === resourceRef(item));
            if (current) current.icon = result.icon;
            break;
          }
          if (!result?.pending) break;
          await new Promise(resolve => setTimeout(resolve, 650));
        } catch {
          // An icon is optional. Keep the type placeholder when an archive has
          // no readable icon or the individual request fails.
          break;
        }
      }
    }
  };
  await Promise.all(Array.from({ length: Math.min(6, items.length) }, worker));
}
function statusLabel(item: any) {
  if (item.status === 'no-metadata') return t('resource.statusNoMetadata');
  if (item.status === 'installed') return t('resource.statusInstalled');
  return t('resource.statusInstalledResource');
}
function formatSize(value: unknown) {
  const size = Number(value || 0);
  if (size < 1024) return `${size} B`;
  if (size < 1048576) return `${(size / 1024).toFixed(1)} KiB`;
  return `${(size / 1048576).toFixed(1)} MiB`;
}
async function load() {
  const requestToken = ++resourceRequestToken;
  loading.value = true;
  error.value = '';
  try {
    const result = await props.execute('resource.list', { instance: props.instance, kind: props.kind, includeIcons: false });
    const list = Array.isArray(result) ? result : (result?.resources || result?.items || result?.data || []);
    if (requestToken !== resourceRequestToken) return;
    resources.value = list.map((item: any) => ({ ...item, enabled: item.enabled !== false }));
    selected.value.clear();
    void loadResourceIcons(requestToken);
  } catch (exception) {
    if (requestToken === resourceRequestToken) error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    if (requestToken === resourceRequestToken) loading.value = false;
  }
}
async function update() {
  localBusy.value = true;
  error.value = '';
  try {
    await props.execute('resource.update', { instance: props.instance, kind: props.kind });
    await load();
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    localBusy.value = false;
  }
}
async function chooseInstallFile() {
  const selected = await pickPclFile({ title: t('resource.chooseInstallTitle') });
  if (selected) installSource.value = selected;
}

async function install() {
  const source = installSource.value.trim();
  if (!source) return;
  localBusy.value = true;
  error.value = '';
  const info = { title: t('resource.installing'), name: source.split(/[\\/]/).pop() || source };
  emit('install-start', info);
  try {
    let task: any = await props.execute('neo.task.start', {
      operation: 'resource.install',
      priority: 'high',
      parameters: { instance: props.instance, kind: props.kind, source },
    });
    while (task && !task.finished) {
      emit('install-progress', task);
      await new Promise(resolve => setTimeout(resolve, 350));
      task = await props.execute('neo.task.status', { taskId: task.taskId });
    }
    emit('install-progress', task);
    if (!task || task.state !== 'succeeded') throw new Error(task?.error || t('resource.installFailed'));
    installSource.value = '';
    await load();
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    localBusy.value = false;
    emit('install-done', info);
  }
}
async function inspect(item: any) {
  detailOpen.value = true;
  detailLoading.value = true;
  detailError.value = '';
  detail.value = item;
  try {
    detail.value = { ...item, ...(await props.execute('resource.inspect', { instance: props.instance, kind: props.kind, resource: resourceRef(item) })) };
  } catch (exception) {
    detailError.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    detailLoading.value = false;
  }
}
async function setEnabled(item: any, enabled: boolean) {
  localBusy.value = true;
  error.value = '';
  try {
    await props.execute(enabled ? 'resource.enable' : 'resource.disable', { instance: props.instance, kind: props.kind, resource: resourceRef(item) });
    item.enabled = enabled;
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    localBusy.value = false;
  }
}
async function remove(item: any) {
  if (!window.confirm(t('resource.confirmDelete', { name: item.name || item.fileName || item.path }))) return;
  localBusy.value = true;
  error.value = '';
  try {
    await props.execute('resource.remove', { instance: props.instance, kind: props.kind, resource: resourceRef(item), confirm: true });
    await load();
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    localBusy.value = false;
  }
}
function toggleSelected(item: any) {
  const next = new Set(selected.value), key = resourceRef(item);
  if (next.has(key)) next.delete(key);
  else next.add(key);
  selected.value = next;
}
async function setSelected(enabled: boolean) {
  const items = resources.value.filter(item => selected.value.has(resourceRef(item)));
  localBusy.value = true;
  try {
    for (const item of items) await setEnabled(item, enabled);
    clearSelected();
  } finally {
    localBusy.value = false;
  }
}
function clearSelected() {
  selected.value = new Set<string>();
}
async function removeSelected() {
  const items = resources.value.filter(item => selected.value.has(resourceRef(item)));
  if (!items.length || !window.confirm(t('resource.confirmDeleteSelected', { count: items.length }))) return;
  localBusy.value = true;
  try {
    for (const item of items) await props.execute('resource.remove', { instance: props.instance, kind: props.kind, resource: resourceRef(item), confirm: true });
    await load();
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    localBusy.value = false;
  }
}
watch(() => [props.instance, props.kind], load);
watch(() => props.instance, () => { void unsubscribeResources().then(subscribeResources); });
onMounted(() => { void load(); void subscribeResources(); });
onBeforeUnmount(() => {
  resourceRequestToken++;
  if (resourceReloadTimer) clearTimeout(resourceReloadTimer);
  void unsubscribeResources();
});
</script>

<style scoped>
.pcl-resource-view {
  margin-top: 0;
}

.pcl-resource-toolbar {
  display: flex;
  align-items: center;
  gap: 7px;
  margin-bottom: 10px;
}

.pcl-resource-query {
  width: 240px;
}

.pcl-resource-view-toggle {
  display: flex;
  align-items: center;
  gap: 2px;
}

.pcl-resource-view-toggle .active {
  color: var(--pcl-c3);
  background: var(--pcl-c6);
}

.pcl-resource-install {
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto;
  gap: 7px;
  margin-bottom: 10px;
}

.pcl-resource-batch {
  display: flex;
  align-items: center;
  gap: 6px;
  margin: 10px 0;
  padding: 7px 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c7);
  color: var(--pcl-muted);
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-resource-batch span {
  margin-right: auto;
}

.pcl-resource-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
  gap: 8px;
  margin-top: 10px;
}

.pcl-resource-card {
  display: flex;
  align-items: center;
  gap: 7px;
  min-width: 0;
  min-height: 58px;
  padding: 7px 6px 7px 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c7);
  transition: border-color .12s ease, background .12s ease;
}

.pcl-resource-card:hover,
.pcl-resource-card.selected {
  border-color: var(--pcl-c5);
  background: var(--pcl-c6);
}

.pcl-resource-card.disabled {
  opacity: .62;
}

.pcl-resource-check {
  flex: 0 0 auto;
}

.pcl-resource-icon {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 40px;
  width: 40px;
  height: 40px;
  padding: 0;
  overflow: hidden;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c8);
  color: var(--pcl-c2);
  cursor: pointer;
}

.pcl-resource-icon img {
  width: 100%;
  height: 100%;
  object-fit: cover;
  image-rendering: pixelated;
}

.pcl-resource-copy {
  display: flex;
  flex-direction: column;
  min-width: 0;
  flex: 1;
  cursor: pointer;
}

.pcl-resource-copy strong,
.pcl-resource-copy small {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-resource-copy strong {
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 400;
}

.pcl-resource-copy small {
  margin-top: 2px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-resource-actions {
  display: flex;
  gap: 1px;
  opacity: .3;
  transition: opacity .12s ease;
}

.pcl-resource-card:hover .pcl-resource-actions,
.pcl-resource-card.selected .pcl-resource-actions {
  opacity: 1;
}

.pcl-resource-empty {
  grid-column: 1 / -1;
  min-height: 180px;
}

.pcl-resource-detail {
  width: 620px;
  max-width: 94vw;
}

.pcl-resource-detail__head {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-resource-detail__body {
  display: grid;
  grid-template-columns: 80px minmax(0, 1fr);
  gap: 14px;
}

.pcl-resource-detail__icon {
  width: 72px;
  height: 72px;
  border-radius: 4px;
  object-fit: cover;
  image-rendering: pixelated;
}

.pcl-resource-detail__meta {
  display: grid;
  grid-template-columns: 86px minmax(0, 1fr);
  gap: 5px 9px;
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-resource-detail__meta span {
  color: var(--pcl-muted);
}

.pcl-resource-detail__meta code {
  overflow-wrap: anywhere;
  color: var(--pcl-text);
}

.pcl-resource-detail__description {
  grid-column: 1 / -1;
  max-height: 220px;
  margin: 0;
  overflow: auto;
  padding: 9px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c8);
  color: var(--pcl-text);
  white-space: pre-wrap;
  font: calc(12px * var(--pcl-font-scale, 1))/1.6 var(--pcl-font);
}

.pcl-resource-table {
  margin-top: 10px;
  overflow: hidden;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
}

.pcl-resource-table__head,
.pcl-resource-row {
  display: grid;
  grid-template-columns: 30px minmax(0, 1fr) 120px 74px 66px 70px;
  align-items: center;
  gap: 7px;
}

.pcl-resource-table__head {
  height: 28px;
  padding: 0 8px;
  border-bottom: 1px solid var(--pcl-border);
  background: var(--pcl-c6);
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-resource-row {
  min-height: 45px;
  padding: 4px 8px;
  border-bottom: 1px solid var(--pcl-border);
  background: var(--pcl-c7);
}

.pcl-resource-row:last-child {
  border-bottom: 0;
}

.pcl-resource-row:hover,
.pcl-resource-row.selected {
  background: var(--pcl-c6);
}

.pcl-resource-row.disabled {
  opacity: .6;
}

.pcl-resource-row__name {
  display: flex;
  align-items: center;
  gap: 8px;
  min-width: 0;
  padding: 0;
  border: 0;
  background: transparent;
  color: var(--pcl-text);
  text-align: left;
  cursor: pointer;
}

.pcl-resource-row__icon {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 26px;
  width: 26px;
  height: 26px;
  overflow: hidden;
  border-radius: 3px;
  background: var(--pcl-c6);
  color: var(--pcl-c2);
}

.pcl-resource-row__icon img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.pcl-resource-row__copy {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.pcl-resource-row__copy strong,
.pcl-resource-row__copy small {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-resource-row__copy strong {
  font-size: calc(11px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-resource-row__copy small {
  margin-top: 2px;
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
}

.pcl-resource-row__meta {
  overflow: hidden;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-resource-row__status {
  color: var(--pcl-c2);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-resource-row__status.off {
  color: var(--pcl-muted);
}

.pcl-resource-row__actions {
  display: flex;
  justify-content: flex-end;
  gap: 1px;
}

@media (max-width: 900px) {
  .pcl-resource-table__head,
  .pcl-resource-row {
    grid-template-columns: 30px minmax(0, 1fr) 66px 70px;
  }

  .pcl-resource-table__head > :nth-child(4),
  .pcl-resource-table__head > :nth-child(5),
  .pcl-resource-row__meta:nth-of-type(2),
  .pcl-resource-row__status {
    display: none;
  }
}

.pcl-resource-download {
  width: 900px;
  max-width: 96vw;
  max-height: 90vh;
  overflow: hidden;
}

.pcl-resource-download__head {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 10px;
  padding-bottom: 8px;
}

.pcl-resource-download__head h3 {
  margin: 3px 0;
  color: var(--pcl-text);
  font-size: calc(17px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-resource-download__body {
  max-height: calc(90vh - 85px);
  overflow-y: auto;
  padding-top: 0;
}
</style>
