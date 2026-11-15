<template>
  <q-dialog :model-value="true" @update:model-value="value => { if (!value) emit('close'); }">
    <q-card class="crash-diagnostic" :class="{ 'crash-diagnostic--pcl': pcl }">
      <q-card-section>
        <h2>{{ t(isGameCrash(failure) ? 'crashDiagnosis.crashed' : 'crashDiagnosis.failed') }}</h2>
        <p class="crash-diagnostic__instance">{{ failure.name }}</p>
        <p v-if="failure.exitCode !== undefined">{{ t('crashDiagnosis.exitCode', { code: failure.exitCode }) }}</p>
        <p v-if="failure.reason">{{ failure.reason }}</p>
      </q-card-section>
      <q-card-section class="crash-diagnostic__body" aria-live="polite">
        <p v-if="loading">{{ t('crashDiagnosis.analyzing') }}</p>
        <template v-else>
          <p v-if="error" role="alert">{{ t('crashDiagnosis.analysisError') }} {{ error }}</p>
          <p v-else-if="!analysis?.findings.length">{{ t('crashDiagnosis.unknown') }}</p>
          <article v-for="finding in analysis?.findings || []" :key="finding.id">
            <h3>{{ finding.title }} <small>{{ t(finding.confidence === 'high' ? 'crashDiagnosis.matched' : 'crashDiagnosis.possible') }}</small></h3>
            <p>{{ finding.advice }}</p>
            <details><summary>{{ t('crashDiagnosis.evidence') }}</summary>
              <div v-for="(evidence, index) in finding.evidence" :key="index"><strong>{{ evidence.source }}</strong><pre>{{ evidence.text }}</pre></div>
            </details>
          </article>
          <p v-if="analysis?.ignoredFiles.length" class="crash-diagnostic__note">{{ t('crashDiagnosis.stale', { count: analysis.ignoredFiles.length }) }}</p>
          <p v-if="analysis?.limitations.length || failure.consoleTruncated" class="crash-diagnostic__note">{{ t('crashDiagnosis.limited') }}</p>
          <p class="crash-diagnostic__note">{{ t('crashDiagnosis.local') }}</p>
        </template>
      </q-card-section>
      <q-card-actions align="right" class="crash-diagnostic__actions">
        <DiagnosticExportButton :instance="failure.instance" :console="failure.console" :since="failure.startedAt" :until="failure.finishedAt" :execute="execute" :pcl="pcl" />
        <q-btn flat :label="t('diagnostics.close')" @click="emit('close')" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>
<script setup lang="ts">
import { onMounted, ref } from 'vue';
import { useI18n } from 'vue-i18n';
import DiagnosticExportButton from './DiagnosticExportButton.vue';
import { isGameCrash, type LaunchCompletion } from '../common/launch-diagnostics';
const props = defineProps<{ failure: LaunchCompletion; pcl?: boolean; execute: (operation: string, parameters?: Record<string, unknown>) => Promise<any> }>();
const emit = defineEmits<{ close: [] }>();
const { t, locale } = useI18n();
type Analysis = { findings: { id: string; title: string; confidence: string; advice: string; evidence: {source: string; text: string}[] }[]; ignoredFiles: unknown[]; limitations: string[] };
const analysis = ref<Analysis | null>(null), loading = ref(true), error = ref('');
onMounted(async () => {
  try {
    analysis.value = await props.execute('instance.diagnostics.analyze', { instance: props.failure.instance,
      console: props.failure.console, reason: props.failure.reason, since: props.failure.startedAt,
      ...(props.failure.finishedAt ? { until: props.failure.finishedAt } : {}), language: locale.value });
  } catch (exception) { error.value = exception instanceof Error ? exception.message : String(exception); }
  finally { loading.value = false; }
});
</script>
<style scoped>
.crash-diagnostic { width: 780px; max-width: 94vw; background: var(--neo-panel, #202733); color: var(--neo-text, #eef4ff); overflow-wrap: anywhere; }
.crash-diagnostic--pcl { background: var(--pcl-white); color: var(--pcl-text); border-radius: var(--pcl-radius); }
h2 { font-size: 21px; line-height: 1.4; margin: 0 0 8px; }
h3 { font-size: 16px; line-height: 1.6; margin: 0; }
p { margin: 6px 0; line-height: 1.65; }
.crash-diagnostic__instance { font-weight: 600; }
.crash-diagnostic__body { max-height: 55vh; overflow: auto; padding-top: 0; }
article { padding: 14px 0; border-top: 1px solid var(--pcl-border, #64748b); }
small { font-size: 11px; font-weight: 400; opacity: .75; }
summary { cursor: pointer; margin: 8px 0; }
pre { white-space: pre-wrap; word-break: break-word; font-size: 12px; margin: 6px 0 12px; }
.crash-diagnostic__note { font-size: 12px; opacity: .8; }
.crash-diagnostic__actions { gap: 6px; flex-wrap: wrap; }
</style>
