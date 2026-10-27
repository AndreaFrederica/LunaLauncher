<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <q-card class="extras-card bg-grey-9 q-mt-lg">
    <q-card-section>
      <q-tabs v-model="tab" align="left" outside-arrows mobile-arrows>
        <q-tab name="worlds" icon="public" :label="t('neo.auto2.kcf1656101e')" />
        <q-tab name="screenshots" icon="photo_library" :label="t('neo.auto2.k49830b4f74')" />
      </q-tabs>

      <q-tab-panels v-model="tab" animated class="bg-transparent text-white">
        <q-tab-panel name="worlds" class="q-px-none">
          <div class="row items-center q-col-gutter-sm q-mb-md">
            <div class="col"><div class="text-subtitle1">{{ t('neo.auto2.kbe750f6d25') }}</div><div class="text-caption text-grey-5">{{ t('neo.auto2.kd0447d91a6') }}</div></div>
            <q-btn flat icon="refresh" :label="t('neo.auto2.k38108eaa1d')" :disable="busy" @click="loadWorlds" />
          </div>
          <div class="row q-col-gutter-sm q-mb-md">
            <q-input v-model="worldSource" class="col" dark outlined dense :label="t('neo.auto2.kdc846310e9')" />
            <q-input v-model="worldName" class="col-12 col-sm-3" dark outlined dense :label="t('neo.auto2.kd5ad695cfd')" />
            <q-btn color="primary" icon="upload" :label="t('neo.auto2.k60e2bcad85')" :disable="busy || !worldSource.trim()" @click="importWorld" />
          </div>
          <q-banner v-if="worldError" class="bg-red-9 q-mb-md">{{ worldError }}</q-banner>
          <q-list v-if="worlds.length" bordered separator>
              <q-item v-for="world in worlds" :key="world.folder || world.name">
              <q-item-section avatar><q-icon name="public" color="primary" /></q-item-section>
              <q-item-section>
                <q-item-label>{{ world.name || world.folder }}</q-item-label>
                <q-item-label caption class="text-grey-5">{{ world.folder }} · {{ formatWorldDate(world.lastPlayed) }} · {{ formatSize(world.size) }}</q-item-label>
              </q-item-section>
              <q-item-section side>
                <div class="row q-gutter-xs">
                  <q-btn flat dense icon="drive_file_rename_outline" :aria-label="t('neo.auto2.k1cd80fd7a8')" :disable="busy" @click="renameWorld(world)" />
                  <q-btn flat dense icon="content_copy" :aria-label="t('neo.auto2.k4edd1d0087')" :disable="busy" @click="copyWorld(world)" />
                  <q-btn flat dense icon="archive" :aria-label="t('neo.auto2.k188896795f')" :disable="busy" @click="exportWorld(world)" />
                  <q-btn flat dense icon="image" :aria-label="t('neo.auto2.k2ea88e854c')" :disable="busy" @click="resetWorldIcon(world)" />
                  <q-btn flat dense icon="delete" color="red-4" :aria-label="t('neo.auto2.k3755f56f2f')" :disable="busy" @click="deleteWorld(world)" />
                </div>
              </q-item-section>
            </q-item>
          </q-list>
          <div v-else-if="!loading" class="text-grey-5 q-pa-md">{{ t('neo.auto2.kec33685a62') }}</div>
          <q-linear-progress v-if="loading" indeterminate class="q-mt-md" />
        </q-tab-panel>

        <q-tab-panel name="screenshots" class="q-px-none">
          <div class="row items-center q-col-gutter-sm q-mb-md">
            <div class="col"><div class="text-subtitle1">{{ t('neo.auto2.k49830b4f74') }}</div><div class="text-caption text-grey-5">{{ t('neo.auto2.k97b0e60f34') }}</div></div>
            <q-btn flat icon="refresh" :label="t('neo.auto2.k38108eaa1d')" :disable="busy" @click="loadScreenshots" />
          </div>
          <div class="row q-col-gutter-sm q-mb-md">
            <q-input v-model="screenshotSource" class="col" dark outlined dense :label="t('neo.auto2.k8f6255b7d0')" />
            <q-input v-model="screenshotName" class="col-12 col-sm-3" dark outlined dense :label="t('neo.auto2.kb975db2142')" />
            <q-btn color="primary" icon="upload" :label="t('neo.auto2.k60e2bcad85')" :disable="busy || !screenshotSource.trim()" @click="importScreenshot" />
          </div>
          <q-banner v-if="screenshotError" class="bg-red-9 q-mb-md">{{ screenshotError }}</q-banner>
          <div v-if="preview" class="preview-wrap q-mb-md"><img :src="preview" :alt="t('neo.auto2.kc98a70535b')" /><q-btn round dense flat icon="close" :aria-label="t('neo.auto2.kbf76308794')" @click="preview=''" /></div>
          <div v-if="screenshots.length" class="row q-col-gutter-md">
            <div v-for="shot in screenshots" :key="shot.name" class="col-12 col-sm-6 col-md-4">
              <q-card class="screenshot-card">
              <q-img :src="shot.thumbnail" ratio="16/9" placeholder-src="" @click="readScreenshot(shot)" />
                <q-card-section class="q-pa-sm"><div class="ellipsis">{{ shot.name }}</div><div class="text-caption text-grey-5">{{ formatSize(shot.size) }} · {{ formatWorldDate(shot.modified) }}</div></q-card-section>
                <q-card-actions align="right" class="q-pa-xs"><q-btn flat dense :label="t('neo.auto2.kf7acefd2d4')" :disable="busy" @click="readScreenshot(shot)" /><q-btn flat dense icon="drive_file_rename_outline" :aria-label="t('neo.auto2.k1cd80fd7a8')" :disable="busy" @click="renameScreenshot(shot)" /><q-btn flat dense :label="t('neo.auto2.ka977650db1')" :disable="busy" @click="uploadScreenshot(shot)" /><q-btn flat dense color="red-4" :label="t('neo.auto2.k3755f56f2f')" :disable="busy" @click="deleteScreenshot(shot)" /></q-card-actions>
              </q-card>
            </div>
          </div>
          <div v-else-if="!loading" class="text-grey-5 q-pa-md">{{ t('neo.auto2.k592c3084fb') }}</div>
          <q-linear-progress v-if="loading" indeterminate class="q-mt-md" />
        </q-tab-panel>
      </q-tab-panels>
    </q-card-section>
  </q-card>
</template>

<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { onMounted, ref, watch } from 'vue';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ instance: string; busy: boolean; execute: Execute }>();
const emit = defineEmits<{ changed: [] }>();

const tab = ref('worlds');
const loading = ref(false);
const worldError = ref('');
const screenshotError = ref('');
const worlds = ref<any[]>([]);
const screenshots = ref<any[]>([]);
const worldSource = ref('');
const worldName = ref('');
const screenshotSource = ref('');
const screenshotName = ref('');
const preview = ref('');

function formatSize(value: unknown) {
  const size = Number(value || 0);
  if (!size) return t('neo.auto2.k4f6367e8d5');
  if (size < 1024) return `${size} B`;
  if (size < 1024 * 1024) return `${(size / 1024).toFixed(1)} KiB`;
  return `${(size / 1024 / 1024).toFixed(1)} MiB`;
}

function formatWorldDate(value: unknown) {
  if (!value) return t('neo.auto2.k39ee16e762');
  const date = new Date(String(value));
  return Number.isNaN(date.getTime()) ? String(value) : date.toLocaleString();
}

async function loadWorlds() {
  loading.value = true; worldError.value = '';
  try { const result = await props.execute('instance.world.list', { instance: props.instance }); worlds.value = Array.isArray(result) ? result : []; }
  catch (error) { worldError.value = error instanceof Error ? error.message : String(error); }
  finally { loading.value = false; }
}

async function loadScreenshots() {
  loading.value = true; screenshotError.value = '';
  try {
    const result = await props.execute('instance.screenshot.list', { instance: props.instance });
    const list = Array.isArray(result) ? result : [];
    screenshots.value = list.map((shot: any) => ({ ...shot, thumbnail: '' }));
    // PCL shows a real thumbnail in every card. Read the bounded image payload
    // through the screenshot API so the native frontend and web preview share
    // exactly the same path and size checks.
    await Promise.all(screenshots.value.map(async (shot: any) => {
      try { const image = await props.execute('instance.screenshot.read', { instance: props.instance, file: shot.name }); shot.thumbnail = `data:image/${image.format || 'png'};base64,${image.content}`; } catch { /* an unreadable image remains a placeholder */ }
    }));
  } catch (error) { screenshotError.value = error instanceof Error ? error.message : String(error); }
  finally { loading.value = false; }
}

async function renameWorld(world: any) {
  const name = window.prompt(t('neo.auto2.kb70c26e182'), world.name || world.folder);
  if (!name || name === (world.name || world.folder)) return;
  await props.execute('instance.world.rename', { instance: props.instance, world: world.folder, name }); emit('changed'); await loadWorlds();
}

async function copyWorld(world: any) {
  const name = window.prompt(t('neo.auto2.k7846e75b59'), `${world.name || world.folder} ${t('neo.extra.copySuffix')}`);
  if (!name) return;
  await props.execute('instance.world.copy', { instance: props.instance, world: world.folder, name }); emit('changed'); await loadWorlds();
}

async function deleteWorld(world: any) {
  if (!window.confirm(t('neo.extra.deleteWorld', { name: world.name || world.folder }))) return;
  await props.execute('instance.world.delete', { instance: props.instance, world: world.folder, confirm: true }); emit('changed'); await loadWorlds();
}

async function importWorld() {
  await props.execute('instance.world.import', { instance: props.instance, source: worldSource.value.trim(), ...(worldName.value.trim() ? { name: worldName.value.trim() } : {}) });
  worldSource.value = ''; worldName.value = ''; emit('changed'); await loadWorlds();
}

async function exportWorld(world: any) {
  const output = window.prompt(t('neo.auto2.k5fc0b37580'), `${world.name || world.folder}.zip`);
  if (!output) return;
  await props.execute('instance.world.export', { instance: props.instance, world: world.folder, output, overwrite: true });
}

async function resetWorldIcon(world: any) {
  if (!window.confirm(t('neo.extra.removeWorldIcon', { name: world.name || world.folder }))) return;
  await props.execute('instance.world.reset-icon', { instance: props.instance, world: world.folder });
  emit('changed'); await loadWorlds();
}

async function importScreenshot() {
  const source = screenshotSource.value.trim(); if (!source) return;
  const name = screenshotName.value.trim() || source.split(/[\\/]/).pop() || 'screenshot.png';
  await props.execute('instance.screenshot.import', { instance: props.instance, source, name });
  screenshotSource.value = ''; screenshotName.value = ''; emit('changed'); await loadScreenshots();
}

async function readScreenshot(shot: any) {
  try {
    const result = await props.execute('instance.screenshot.read', { instance: props.instance, file: shot.name });
    preview.value = `data:image/${result.format || 'png'};base64,${result.content}`;
  } catch (error) { screenshotError.value = error instanceof Error ? error.message : String(error); }
}

async function uploadScreenshot(shot: any) { await props.execute('instance.screenshot.upload', { instance: props.instance, file: shot.name }); }
async function renameScreenshot(shot: any) {
  const name = window.prompt(t('neo.auto2.k0297e5903f'), shot.name);
  if (!name || name === shot.name) return;
  await props.execute('instance.screenshot.rename', { instance: props.instance, file: shot.name, name });
  emit('changed'); await loadScreenshots();
}
async function deleteScreenshot(shot: any) {
  if (!window.confirm(t('neo.extra.deleteScreenshot', { name: shot.name }))) return;
  await props.execute('instance.screenshot.delete', { instance: props.instance, file: shot.name, confirm: true });
  emit('changed'); await loadScreenshots();
}

watch(() => props.instance, () => { preview.value = ''; void loadWorlds(); void loadScreenshots(); });
watch(tab, value => { if (value === 'worlds' && !worlds.value.length) void loadWorlds(); if (value === 'screenshots' && !screenshots.value.length) void loadScreenshots(); });
onMounted(() => { void loadWorlds(); });
</script>

<style scoped>
.extras-card{border:1px solid rgba(255,255,255,.08);border-radius:16px}.screenshot-card{background:#121927;border:1px solid rgba(255,255,255,.08);border-radius:12px;overflow:hidden}.preview-wrap{position:relative;max-width:900px;margin:auto;background:#080b12;border-radius:12px;overflow:hidden}.preview-wrap img{display:block;width:100%;max-height:520px;object-fit:contain}.preview-wrap .q-btn{position:absolute;right:8px;top:8px;background:rgba(0,0,0,.55)}
</style>
