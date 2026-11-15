<template>
  <section class="library-patches">
    <q-toggle :model-value="enabled" :disable="busy" :label="t('libraryPatches.enable')" @update:model-value="emit('update:enabled', Boolean($event))" />
    <p>{{ t('libraryPatches.hint') }}</p>
    <div class="row q-gutter-sm">
      <q-btn outline no-caps :disable="busy || loading" :label="t('libraryPatches.openBase')" @click="open(false)" />
      <q-btn outline no-caps :disable="busy || loading" :label="t('libraryPatches.openOverlay')" @click="open(true)" />
      <q-btn flat no-caps :disable="busy || loading" :label="t('libraryPatches.reload')" @click="load" />
    </div>
    <div v-if="error || status?.errors?.length" class="library-patches__errors" role="alert">{{ error || status.errors.join('\n') }}</div>
    <div v-if="status" class="library-patches__paths">
      <div>{{ t('libraryPatches.base') }}: {{ status.basePath }}</div>
      <div>{{ t('libraryPatches.overlay') }}: {{ status.overlayPath }}</div>
      <details><summary>{{ t('libraryPatches.rules', { count: status.rules.length }) }}</summary><pre tabindex="0">{{ JSON.stringify(status.rules, null, 2) }}</pre></details>
    </div>
  </section>
</template>

<script setup lang="ts">
import { onMounted, ref } from 'vue';
import { useI18n } from 'vue-i18n';
const props = defineProps<{ enabled: boolean; busy?: boolean; execute: (operation: string, parameters?: Record<string, unknown>) => Promise<any> }>();
const emit = defineEmits<{ 'update:enabled': [value: boolean] }>();
const { t } = useI18n();
const status = ref<any>(null), error = ref(''), loading = ref(false);
async function load() {
  loading.value = true; error.value = '';
  try { status.value = await props.execute('resource.library-patches', {}); }
  catch (e) { error.value = e instanceof Error ? e.message : String(e); }
  finally { loading.value = false; }
}
async function open(overlay: boolean) {
  loading.value = true; error.value = '';
  try { status.value = await props.execute('resource.library-patches.open', { overlay }); }
  catch (e) { error.value = e instanceof Error ? e.message : String(e); }
  finally { loading.value = false; }
}
onMounted(load);
</script>

<style scoped>
.library-patches { min-width: 0; }
.library-patches p, .library-patches__paths { font-size: 12px; overflow-wrap: anywhere; }
.library-patches__paths { margin-top: 12px; }
.library-patches__paths pre, .library-patches__errors { max-height: 180px; overflow: auto; white-space: pre-wrap; overflow-wrap: anywhere; }
.library-patches__errors { color: var(--pcl-hint-red-text, #fb7185); margin-top: 10px; }
.library-patches summary { cursor: pointer; margin-top: 8px; }
</style>
