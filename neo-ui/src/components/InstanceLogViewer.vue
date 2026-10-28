<template>
  <section class="instance-logs" :class="{ 'instance-logs--pcl': pcl }" :aria-label="t('neo.auto2.k8acb5aa50b')">
    <div class="instance-logs__grid">
      <aside class="instance-logs__files">
        <header><strong>{{ t('neo.auto2.k01516b6cd4') }}</strong><span>{{ files.length }}</span></header>
        <div class="instance-logs__list" :aria-label="t('neo.auto2.kf961b06e6c')">
          <button v-for="file in files" :key="file.path" type="button" :title="file.path"
            :class="{ selected: selectedPath === file.path }" :aria-pressed="selectedPath === file.path" @click="read(file.path)">
            <span class="instance-logs__filename">{{ file.name }}</span>
            <small>{{ formatDate(file.modified) }}</small>
          </button>
          <p v-if="!files.length" class="instance-logs__hint">{{ t('neo.auto2.kff9d2397a0') }}</p>
        </div>
      </aside>
      <div class="instance-logs__detail">
        <header class="instance-logs__toolbar">
          <strong :title="selectedPath">{{ selectedFile?.name || t('neo.auto2.k5f5f8b8a86') }}</strong>
          <div class="instance-logs__actions">
            <label><input v-model="wrap" type="checkbox" />{{ t('neo.auto2.kfce1b42c8b') }}</label>
            <button type="button" :disabled="!selectedPath || loading" @click="read(selectedPath)">{{ t('neo.auto2.k7784972fc8') }}</button>
            <LogUploadButton :instance="instance" :name="selectedFile?.name || t('neo.auto2.k4de50894b8')" :file="selectedPath" :pcl="pcl"
              :execute="execute" :disabled="!selectedPath || loading || !!error || !content" />
          </div>
        </header>
        <p v-if="truncated" class="instance-logs__hint">{{ t('neo.auto2.k9c0a9e9b8d') }}</p>
        <p v-if="error" class="instance-logs__error" role="alert">{{ error }}</p>
        <pre ref="viewer" class="instance-logs__text" :class="{ wrap }" tabindex="0" :aria-label="t('neo.auto2.k5f5f8b8a86')" :aria-busy="loading">{{ loading ? t('neo.auto2.k79aa9eabfa') : content || (selectedPath ? t('neo.auto2.k3a03512b57') : t('neo.auto2.kb9d0ef7801')) }}</pre>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { computed, nextTick, onBeforeUnmount, ref, watch } from 'vue';
import LogUploadButton from './LogUploadButton.vue';

const props = defineProps<{
  instance: string; logFiles: any[]; pcl?: boolean;
  execute: (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
}>();
const files = computed(() => props.logFiles.map(file => ({
  path: String(file.path || file.name || file), name: String(file.name || file.path || file), modified: String(file.modified || ''),
})).sort((a, b) => b.modified.localeCompare(a.modified) || a.name.localeCompare(b.name)));
const selectedPath = ref(''), content = ref(''), error = ref('');
const loading = ref(false), truncated = ref(false), wrap = ref(false);
const viewer = ref<HTMLElement | null>(null);
const selectedFile = computed(() => files.value.find(file => file.path === selectedPath.value));
let generation = 0;
function formatDate(value: string) { return value ? new Date(value).toLocaleString() : ''; }
async function read(path: string) {
  const request = ++generation;
  const instance = props.instance;
  selectedPath.value = path; content.value = ''; error.value = ''; truncated.value = false; loading.value = true;
  try {
    const result = await props.execute('instance.log.read', { instance, file: path, maxBytes: 1048576 });
    if (request !== generation || instance !== props.instance) return;
    content.value = String(result?.content || '');
    truncated.value = Boolean(result?.truncated);
    await nextTick();
    viewer.value?.scrollTo(0, 0);
  } catch (exception) {
    if (request === generation) error.value = String(exception instanceof Error ? exception.message : exception);
  } finally { if (request === generation) loading.value = false; }
}
watch(() => props.instance, () => {
  generation++; selectedPath.value = ''; content.value = ''; error.value = ''; truncated.value = false; loading.value = false;
}, { flush: 'sync' });
watch([() => props.instance, files], () => {
  if (!selectedFile.value) {
    if (files.value.length) void read(files.value[0].path);
    else { generation++; selectedPath.value = ''; content.value = ''; error.value = ''; loading.value = false; truncated.value = false; }
  }
}, { immediate: true });
onBeforeUnmount(() => { generation++; });
</script>

<style scoped>
.instance-logs { --log-panel: #202733; --log-bg: #111720; --log-text: #e5edf8; --log-muted: #a0aec0; --log-border: #364252; --log-accent: #92baff; min-width: 0; margin-top: 14px; container-type: inline-size; }
.instance-logs--pcl { --log-panel: var(--pcl-card-bg, var(--pcl-c7)); --log-bg: var(--pcl-c8); --log-text: var(--pcl-text); --log-muted: var(--pcl-muted); --log-border: var(--pcl-border); --log-accent: var(--pcl-c2); }
.instance-logs__grid { display: grid; grid-template-columns: minmax(170px, 25%) minmax(0, 1fr); gap: 12px; height: clamp(320px, calc(100vh - 220px), 900px); }
.instance-logs__files, .instance-logs__detail { display: flex; flex-direction: column; min-width: 0; min-height: 0; overflow: hidden; background: var(--log-panel); color: var(--log-text); border: 1px solid var(--log-border); border-radius: 6px; }
header { display: flex; flex: 0 0 auto; align-items: center; justify-content: space-between; gap: 10px; padding: 12px; border-bottom: 1px solid var(--log-border); }
header > span { color: var(--log-muted); font-size: 12px; }
.instance-logs__list { flex: 1; min-height: 0; overflow: auto; padding: 5px; }
.instance-logs__list button { display: flex; flex-direction: column; gap: 5px; width: 100%; padding: 10px; border: 1px solid transparent; border-left: 3px solid transparent; border-radius: 3px; background: transparent; color: inherit; text-align: left; cursor: pointer; }
.instance-logs__list button:hover:not(.selected) { background: color-mix(in srgb, var(--log-text) 5%, transparent); }
.instance-logs__list button.selected { border-left-color: var(--log-accent); background: color-mix(in srgb, var(--log-accent) 12%, transparent); color: var(--log-accent); }
.instance-logs__filename { width: 100%; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-size: 13px; }
small { color: var(--log-muted); font-size: 11px; }
.instance-logs__toolbar { flex-wrap: wrap; }
.instance-logs__toolbar > strong { min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-size: 14px; }
.instance-logs__actions { display: flex; align-items: center; flex-wrap: wrap; gap: 10px; }
.instance-logs__actions label { display: flex; align-items: center; gap: 5px; font-size: 12px; white-space: nowrap; }
.instance-logs__actions > button { border: 1px solid var(--log-border); border-radius: 4px; padding: 5px 9px; color: inherit; background: transparent; cursor: pointer; }
.instance-logs__actions > button:disabled { opacity: .5; cursor: default; }
.instance-logs__text { flex: 1; min-height: 0; min-width: 0; margin: 0; padding: 12px; overflow: auto; background: var(--log-bg); color: var(--log-text); font: 13px/1.55 Consolas, 'Cascadia Mono', 'Noto Sans Mono', monospace; font-variant-ligatures: none; text-align: left; white-space: pre; word-break: normal; overflow-wrap: normal; tab-size: 4; user-select: text; }
.instance-logs__text.wrap { white-space: pre-wrap; overflow-wrap: anywhere; }
.instance-logs__hint, .instance-logs__error { margin: 0; padding: 10px 12px; color: var(--log-muted); font-size: 12px; }
.instance-logs__error { color: #f08080; }
@container (max-width: 640px) { .instance-logs__grid { grid-template-columns: minmax(0, 1fr); height: auto; } .instance-logs__files { max-height: 190px; } .instance-logs__detail { height: 440px; } }
</style>
