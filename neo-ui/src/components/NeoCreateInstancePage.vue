<template>
  <section class="neo-create-instance form">
    <h5>{{ t('app.addInstance') }}</h5>
    <q-tabs v-model="mode" class="neo-create-instance__tabs">
      <q-tab name="vanilla" label="Minecraft" />
      <q-tab name="server" :label="t('app.server')" />
      <q-tab name="import" :label="t('app.importPack')" />
      <q-tab name="catalog" :label="t('app.downloadPack')" />
    </q-tabs>
    <CatalogBrowser v-if="mode === 'catalog'" packs :busy="busy" :execute="execute" @installed="emit('installed')" />
    <template v-else>
      <q-input v-model="draft.name" dark outlined :label="t('app.instanceName')" class="q-my-md" />
      <q-input v-model="draft.group" dark outlined :label="t('app.groupOptional')" class="q-mb-md" />
      <template v-if="mode === 'vanilla'">
        <div class="row q-gutter-sm q-mb-md">
          <q-select v-model="draft.version" class="col" dark outlined :options="minecraftVersions" use-input input-debounce="0" :label="t('app.minecraftVersion')" @filter="filterMinecraft" @update:model-value="resetLoaderVersions" />
          <q-btn :label="t('app.loadVersions')" :disable="busy || loadingVersions" @click="loadVersions(false)" />
        </div>
        <q-select v-model="draft.loader" dark outlined :options="loaderOptions" :label="t('app.loaderOptional')" class="q-mb-md" @update:model-value="resetLoaderVersions" />
        <div v-if="draft.loader" class="row q-gutter-sm q-mb-md">
          <q-select v-model="draft.loaderVersion" class="col" dark outlined :options="loaderVersions" :label="t('app.loaderVersion')" />
          <q-btn :label="t('app.loadCompatible')" :disable="busy || loadingVersions || !draft.version" @click="loadVersions(true)" />
        </div>
      </template>
      <template v-else-if="mode === 'server'">
        <div class="row q-gutter-sm q-mb-md">
          <q-select v-model="draft.serverProvider" class="col" dark outlined emit-value map-options :options="serverProviderOptions" option-label="name" option-value="id" label="服务端来源" @update:model-value="selectServerProvider" />
          <q-btn label="刷新来源" :disable="busy || serverLoading" @click="loadServerCatalog" />
        </div>
        <template v-if="draft.serverProvider === 'fastmirror'">
          <q-select v-model="draft.serverCore" dark outlined emit-value map-options :options="serverCores" option-label="name" option-value="id" label="服务端核心" class="q-mb-md" @update:model-value="loadServerVersions" />
          <div class="row q-gutter-sm q-mb-md">
            <q-select v-model="draft.version" class="col" dark outlined emit-value map-options :options="serverVersions" option-label="name" option-value="id" :label="t('app.serverVersion')" @update:model-value="loadServerBuilds" />
            <q-btn label="刷新版本" :disable="busy || serverLoading || !draft.serverCore" @click="loadServerVersions" />
          </div>
          <q-select v-model="draft.serverCoreVersion" dark outlined emit-value map-options :options="serverBuilds" option-label="name" option-value="id" label="构建版本" class="q-mb-md" />
        </template>
        <q-input v-model="draft.source" dark outlined :label="t('app.serverSource')" class="q-mb-md" />
        <q-banner v-if="serverError" class="bg-red-9 text-white q-mb-md" rounded>{{ serverError }}</q-banner>
      </template>
      <q-input v-else v-model="draft.source" dark outlined :label="t('app.packSource')" class="q-mb-md" />
      <q-banner v-if="error" class="bg-red-9 text-white q-mb-md" rounded>{{ error }}</q-banner>
      <q-btn color="primary" :label="t('app.createInstance')" :disable="!canCreate" @click="submit" />
    </template>
  </section>
</template>

<script setup lang="ts">
import { computed, reactive, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import CatalogBrowser from './CatalogBrowser.vue';

export interface NeoCreationDraft { name: string; group: string; version: string; loader: string; loaderVersion: string; source: string; serverProvider?: string; serverCore?: string; serverCoreVersion?: string; }
type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ modelValue: NeoCreationDraft; mode: string; busy: boolean; execute: Execute }>();
const emit = defineEmits<{ 'update:modelValue': [value: NeoCreationDraft]; 'update:mode': [value: string]; create: [value: NeoCreationDraft & { type?: string }]; installed: [] }>();
const { t } = useI18n();
const mode = computed({ get: () => props.mode, set: value => emit('update:mode', value) });
const draft = reactive<any>({ ...props.modelValue });
const error = ref<any>('');
const loadingVersions = ref(false);
const allMinecraftVersions = ref<string[]>([]);
const minecraftVersions = ref<string[]>([]);
const loaderVersions = ref<string[]>([]);
const loaderOptions = ['', 'fabric', 'forge', 'neoforge', 'quilt'];
const serverLoading = ref(false), serverError = ref('');
const serverProviderOptions = ref<any[]>([]), serverCores = ref<any[]>([]), serverVersions = ref<any[]>([]), serverBuilds = ref<any[]>([]);
watch(() => props.modelValue, value => Object.assign(draft, value), { deep: true });
watch(draft, value => emit('update:modelValue', { ...value }), { deep: true });
const canCreate = computed(() => {
  if (props.busy || !draft.name.trim()) return false;
  if (mode.value === 'vanilla') return Boolean(draft.version && (!draft.loader || draft.loaderVersion));
  if (mode.value === 'server' && draft.serverProvider === 'fastmirror' && !draft.source.trim()) return Boolean(draft.serverCore && draft.version && draft.serverCoreVersion);
  return mode.value !== 'import' || Boolean(draft.source.trim());
});
watch(mode, value => { if (value === 'server' && !serverProviderOptions.value.length) void loadServerCatalog(); });
function filterMinecraft(value: string, update: (callback: () => void) => void) { update(() => { const query = value.toLowerCase(); minecraftVersions.value = allMinecraftVersions.value.filter(version => version.toLowerCase().includes(query)); }); }
function resetLoaderVersions() { draft.loaderVersion = ''; loaderVersions.value = []; }
async function loadVersions(loader: boolean) {
  if (props.busy || loadingVersions.value) return;
  const uids: Record<string, string> = { fabric: 'net.fabricmc.fabric-loader', forge: 'net.minecraftforge', neoforge: 'net.neoforged', quilt: 'org.quiltmc.quilt-loader' };
  loadingVersions.value = true; error.value = '';
  try {
    const result = await props.execute('component.versions', loader ? { uid: uids[draft.loader], minecraftVersion: draft.version } : { uid: 'net.minecraft' });
    const versions = Array.isArray(result?.versions) ? result.versions.map((item: any) => String(item.version || item)).filter(Boolean) : [];
    if (loader) loaderVersions.value = versions; else { allMinecraftVersions.value = versions; minecraftVersions.value = versions; }
  } catch (cause) { error.value = cause instanceof Error ? cause.message : String(cause); }
  finally { loadingVersions.value = false; }
}
async function loadServerCatalog() {
  if (serverLoading.value || props.busy) return;
  serverLoading.value = true; serverError.value = '';
  try {
    const result = await props.execute('server.distribution.catalog');
    serverProviderOptions.value = Array.isArray(result?.providers) ? result.providers : [];
    if (!draft.serverProvider || !serverProviderOptions.value.some(item => item.id === draft.serverProvider)) draft.serverProvider = serverProviderOptions.value.some(item => item.id === 'fastmirror') ? 'fastmirror' : String(serverProviderOptions.value[0]?.id || '');
    if (draft.serverProvider === 'fastmirror') await loadServerCores();
  } catch (cause) { serverError.value = cause instanceof Error ? cause.message : String(cause); }
  finally { serverLoading.value = false; }
}
async function loadServerCores() {
  serverError.value = '';
  const result = await props.execute('server.distribution.versions', { provider: 'fastmirror' });
  serverCores.value = Array.isArray(result?.versions) ? result.versions : [];
  if (!draft.serverCore || !serverCores.value.some(item => item.id === draft.serverCore)) draft.serverCore = String(serverCores.value[0]?.id || '');
  if (draft.serverCore) await loadServerVersions();
}
async function loadServerVersions() {
  if (draft.serverProvider !== 'fastmirror' || !draft.serverCore) return;
  serverError.value = '';
  try {
    const result = await props.execute('server.distribution.versions', { provider: 'fastmirror', core: draft.serverCore });
    serverVersions.value = Array.isArray(result?.versions) ? result.versions : [];
    if (!draft.version || !serverVersions.value.some(item => item.id === draft.version)) draft.version = String(serverVersions.value[0]?.id || '');
    await loadServerBuilds();
  } catch (cause) { serverError.value = cause instanceof Error ? cause.message : String(cause); }
}
async function loadServerBuilds() {
  if (draft.serverProvider !== 'fastmirror' || !draft.serverCore || !draft.version) return;
  serverError.value = '';
  try {
    const result = await props.execute('server.distribution.versions', { provider: 'fastmirror', core: draft.serverCore, version: draft.version });
    serverBuilds.value = Array.isArray(result?.builds) ? result.builds : [];
    if (!draft.serverCoreVersion || !serverBuilds.value.some(item => item.id === draft.serverCoreVersion)) draft.serverCoreVersion = String(serverBuilds.value[0]?.id || '');
  } catch (cause) { serverError.value = cause instanceof Error ? cause.message : String(cause); }
}
function selectServerProvider() { serverCores.value = []; serverVersions.value = []; serverBuilds.value = []; if (draft.serverProvider === 'fastmirror') void loadServerCores(); }
async function submit() {
  if (!canCreate.value) return;
  const value: NeoCreationDraft & { type?: string; url?: string; fileName?: string; sha1?: string } = { ...draft };
  if (mode.value === 'import') value.type = 'import';
  else if (mode.value === 'server') {
    value.type = 'server';
    if (draft.serverProvider === 'fastmirror' && !draft.source.trim()) {
      try {
        const resolved = await props.execute('server.distribution.resolve', { provider: 'fastmirror', core: draft.serverCore, version: draft.version, coreVersion: draft.serverCoreVersion });
        Object.assign(value, { url: resolved?.url, fileName: resolved?.fileName, sha1: resolved?.sha1, minecraftVersion: draft.version });
      } catch (cause) { serverError.value = cause instanceof Error ? cause.message : String(cause); return; }
    }
  }
  emit('create', value);
}
</script>

<style scoped>
.neo-create-instance { max-width: 720px; }
.neo-create-instance__tabs { margin-bottom: 4px; }
</style>
