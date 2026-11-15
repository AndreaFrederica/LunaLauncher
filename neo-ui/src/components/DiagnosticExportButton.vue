<template>
  <PclButton v-if="pcl" size="sm" icon="copy" variant="ghost" :disabled="!instance || disabled || exporting || copying" @click="copyDiagnostics">{{ copying ? t('diagnostics.preparing') : t('diagnostics.quickCopy') }}</PclButton>
  <q-btn v-else flat icon="content_copy" :label="t('diagnostics.quickCopy')" :loading="copying" :disable="!instance || disabled || exporting" @click="copyDiagnostics" />
  <PclButton v-if="pcl" size="sm" icon="download" variant="ghost" :disabled="!instance || disabled || exporting || copying" @click="exportBundle">{{ exporting ? t('diagnostics.exporting') : t('diagnostics.export') }}</PclButton>
  <q-btn v-else flat icon="download" :label="t('diagnostics.export')" :loading="exporting" :disable="!instance || disabled || copying" @click="exportBundle" />
  <q-dialog v-model="shown">
    <q-card class="diagnostic-export" :class="{ 'diagnostic-export--pcl': pcl }">
      <q-card-section>
        <h2>{{ t('diagnostics.title') }}</h2>
        <p v-if="error" class="diagnostic-export__error" role="alert">{{ error }}</p>
        <template v-if="result">
          <template v-if="result.path">
            <p>{{ t('diagnostics.saved') }}</p>
            <code>{{ result.path }}</code>
            <ul><li v-for="file in result.files" :key="file.file">{{ file.file }}</li><li>AI-PROMPT.md</li><li>manifest.json</li></ul>
          </template>
          <p v-if="result.missing.length">{{ t('diagnostics.missing', { items: result.missing.map(kind => t(`diagnostics.kind.${kind}`)).join('、') }) }}</p>
          <p>{{ t(result.path ? 'diagnostics.howToUse' : 'diagnostics.clipboardHelp') }}</p>
          <textarea readonly :value="result.prompt" :aria-label="t('diagnostics.prompt')" />
        </template>
      </q-card-section>
      <q-card-actions align="right">
        <q-btn v-if="result" flat :label="copied ? t('diagnostics.copied') : t('diagnostics.copy')" @click="copyPrompt" />
        <q-btn flat :label="t('diagnostics.close')" @click="shown = false" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { ref } from 'vue';
import { useI18n } from 'vue-i18n';
import { copyToClipboard, Notify } from 'quasar';
import PclButton from '../themes/pcl/components/PclButton.vue';
import { pickPclSavePath } from '../themes/pcl/composables/usePclFileDialog';

const props = defineProps<{
  instance: string; console?: string; pcl?: boolean; disabled?: boolean; since?: string; until?: string;
  execute: (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
}>();
type Result = { path?: string; prompt: string; files: { file: string }[]; missing: string[] };
const { t, locale } = useI18n();
const shown = ref(false), exporting = ref(false), copied = ref(false), error = ref('');
const copying = ref(false);
const result = ref<Result | null>(null);
async function copyDiagnostics() {
  if (!props.instance || props.disabled || exporting.value || copying.value) return;
  const instance = props.instance;
  const interval = { ...(props.since ? { since: props.since } : {}), ...(props.until ? { until: props.until } : {}) };
  const console = (props.console || '').slice(-4 * 1024 * 1024);
  copying.value = true; error.value = ''; result.value = null; copied.value = false;
  try {
    result.value = await props.execute('instance.diagnostics.prepare', { instance, console, ...interval, language: locale.value });
    await copyToClipboard(result.value!.prompt);
    copied.value = true;
    Notify.create({ type: 'positive', message: t('diagnostics.clipboardSuccess'), timeout: 2500 });
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
    shown.value = true;
  } finally { copying.value = false; }
}
async function exportBundle() {
  if (!props.instance || props.disabled || exporting.value || copying.value) return;
  // Capture the instance and its console together before a file picker can
  // yield to a selection change or another running instance's console.
  const instance = props.instance;
  const interval = { ...(props.since ? { since: props.since } : {}), ...(props.until ? { until: props.until } : {}) };
  const console = (props.console || '').slice(-4 * 1024 * 1024);
  exporting.value = true; error.value = ''; result.value = null; copied.value = false;
  try {
    const output = await pickPclSavePath({ title: t('diagnostics.export'),
      defaultPath: `luna-diagnostics-${new Date().toISOString().replace(/[:.]/g, '-')}.zip`,
      filters: [{ name: 'ZIP', extensions: ['zip'] }] });
    if (!output) return;
    result.value = await props.execute('instance.diagnostics.export', { instance, output, console, ...interval, language: locale.value });
    shown.value = true;
  } catch (exception) {
    error.value = exception instanceof Error ? exception.message : String(exception);
    shown.value = true;
  } finally { exporting.value = false; }
}
async function copyPrompt() {
  try { await copyToClipboard(result.value?.prompt || ''); copied.value = true; error.value = ''; }
  catch (exception) { error.value = exception instanceof Error ? exception.message : String(exception); }
}
</script>

<style scoped>
.diagnostic-export { width: 720px; max-width: 94vw; background: var(--neo-panel, #202733); color: var(--neo-text, #eef4ff); overflow-wrap: anywhere; }
.diagnostic-export--pcl { background: var(--pcl-white); color: var(--pcl-text); border-radius: var(--pcl-radius); }
h2 { margin: 0 0 12px; font-size: 19px; }
p { line-height: 1.6; }
textarea { width: 100%; height: 240px; resize: vertical; padding: 10px; border: 1px solid var(--pcl-border, #64748b); background: transparent; color: inherit; font: 12px/1.5 Consolas, monospace; }
.diagnostic-export__error { color: var(--q-negative, #f08080); }
</style>
