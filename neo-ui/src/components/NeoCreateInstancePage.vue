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
        <q-input v-model="draft.version" dark outlined :label="t('app.serverVersion')" class="q-mb-md" />
        <q-input v-model="draft.source" dark outlined :label="t('app.serverSource')" class="q-mb-md" />
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

export interface NeoCreationDraft { name: string; group: string; version: string; loader: string; loaderVersion: string; source: string; }
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
watch(() => props.modelValue, value => Object.assign(draft, value), { deep: true });
watch(draft, value => emit('update:modelValue', { ...value }), { deep: true });
const canCreate = computed(() => {
  if (props.busy || !draft.name.trim()) return false;
  if (mode.value === 'vanilla') return Boolean(draft.version && (!draft.loader || draft.loaderVersion));
  return mode.value !== 'import' || Boolean(draft.source.trim());
});
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
function submit() { if (!canCreate.value) return; const value: NeoCreationDraft & { type?: string } = { ...draft }; if (mode.value === 'import') value.type = 'import'; else if (mode.value === 'server') value.type = 'server'; emit('create', value); }
</script>

<style scoped>
.neo-create-instance { max-width: 720px; }
.neo-create-instance__tabs { margin-bottom: 4px; }
</style>
