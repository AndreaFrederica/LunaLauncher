<template>
  <q-dialog :model-value="modelValue" persistent @update:model-value="emit('update:modelValue', $event)">
    <q-card class="pcl-dialog pcl-launch-dialog" >
      <q-card-section class="pcl-launch-dialog__head">
        <span class="pcl-launch-dialog__badge"><PclIcon name="play" :size="20" /></span>
        <div class="pcl-launch-dialog__head-copy">
          <span class="pcl-kicker">{{ t('pcl.launchDialog.kicker') }}</span>
          <h3>{{ state === 'launched' ? t('pcl.launchDialog.success') : state === 'failed' ? t('launch.failed') : state === 'cancelled' ? t('launch.cancelled') : t('pcl.launchDialog.launching') }}</h3>
          <p>{{ instanceName || t('pcl.launchDialog.instance') }}</p>
        </div>
        <span class="pcl-launch-dialog__percent">{{ percentText }}</span>
      </q-card-section>

      <q-card-section class="pcl-launch-dialog__body">
        <div class="pcl-progress-track pcl-launch-dialog__track" :class="{ 'is-indeterminate': percent === null }">
          <span :style="percent === null ? undefined : { width: `${percent}%` }" />
        </div>

        <div class="pcl-launch-dialog__stage">
          <PclIcon v-if="state === 'launched'" name="circle-check" :size="15" />
          <PclIcon v-else name="info" :size="15" />
          <span>{{ stage || (state === 'launching' ? t('pcl.launchDialog.preparing') : state === 'launched' ? t('pcl.launchDialog.processStarted') : t('pcl.launchDialog.ended')) }}</span>
        </div>
        <p v-if="details" class="pcl-launch-dialog__details">{{ details }}</p>
        <div v-if="failedRows.length" class="pcl-launch-dialog__downloads pcl-launch-dialog__failures">
          <div class="pcl-launch-dialog__downloads-head">{{ t('pcl.launchDialog.failedItems') }}</div>
          <div v-for="row in failedRows" :key="row.id" class="pcl-launch-dialog__download">
            <span class="pcl-launch-dialog__download-name">{{ taskItemLabel(row) }}</span>
            <span class="pcl-launch-dialog__download-meta">{{ row.details || row.status || t('pcl.launchDialog.verifyFailed') }}</span>
          </div>
        </div>
        <div v-if="stepRows.length || state === 'launching'" class="pcl-launch-dialog__downloads">
          <div class="pcl-launch-dialog__downloads-head">{{ t('pcl.launchDialog.steps') }}</div>
          <div v-if="!stepRows.length" class="pcl-launch-dialog__step-empty">{{ t('pcl.launchDialog.waitingSteps') }}</div>
          <div v-for="row in stepRows" :key="row.id" class="pcl-launch-dialog__download pcl-launch-dialog__step" :class="`is-${row.state}`">
            <span class="pcl-launch-dialog__step-mark">{{ row.mark }}</span>
            <span class="pcl-launch-dialog__download-name">{{ row.label }}</span>
            <span class="pcl-launch-dialog__download-meta">{{ row.detail }}<template v-if="row.rate"> · {{ formatRate(row.rate) }}</template></span>
            <div v-if="row.state === 'running' && row.total > 0" class="pcl-progress-track pcl-launch-dialog__child-track"><span :style="{ width: `${row.percentValue}%` }" /></div>
          </div>
        </div>

        <div class="pcl-launch-dialog__info">
          <div><span>{{ t('launch.loginMethod') }}</span><strong>{{ accountName || t('nav.noAccount') }}</strong></div>
          <div><span>{{ t('pcl.launchDialog.loginMethod') }}</span><strong>{{ loginMethod || '—' }}</strong></div>
          <div><span>{{ t('launch.progress') }}</span><strong>{{ percentText }}</strong></div>
          <div v-if="speed > 0"><span>{{ t('launch.speed') }}</span><strong>{{ formatRate(speed) }}</strong></div>
        </div>

        <PclHint v-if="state === 'launched'" theme="blue">
          {{ hint || t('pcl.launchDialog.startedHint') }}
        </PclHint>
        <PclHint v-else-if="error" theme="red" :closable="false">{{ error }}</PclHint>
        <PclHint v-else theme="blue">{{ t('pcl.launchDialog.environment') }}</PclHint>
      </q-card-section>

      <q-card-actions align="right" class="pcl-launch-dialog__actions">
        <PclButton v-if="state === 'launching'" variant="ghost" icon="x" :disabled="cancelling" @click="emit('cancel')">
          {{ cancelling ? t('pcl.launchDialog.cancelling') : t('launch.cancel') }}
        </PclButton>
        <PclButton v-else-if="state === 'failed'" variant="highlight" icon="refresh-cw" @click="emit('retry')">{{ t('common.retry') }}</PclButton>
        <PclButton v-if="state !== 'launching'" variant="ghost" icon="x" @click="emit('update:modelValue', false)">{{ t('window.close') }}</PclButton>
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import { useI18n } from 'vue-i18n';
import PclButton from './PclButton.vue';
import PclHint from './PclHint.vue';
import PclIcon from './PclIcon.vue';
import {
  flattenTaskItems,
  formatTaskRate,
  taskActiveItems,
  taskItemProgressText,
  taskItemCurrent,
  taskItemLabel,
  taskItemTotal,
  taskState,
} from '../composables/useTaskProgress';

const props = withDefaults(defineProps<{
  modelValue: boolean;
  state?: string;
  progress?: number;
  stage?: string;
  speed?: number;
  error?: string;
  instanceName?: string;
  accountName?: string;
  loginMethod?: string;
  hint?: string;
  details?: string;
  children?: any[];
  cancelling?: boolean;
}>(), {
  state: 'launching',
  progress: 0,
  stage: '',
  speed: 0,
  error: '',
  instanceName: '',
  accountName: '',
  loginMethod: '',
  hint: '',
  details: '',
  children: () => [],
  cancelling: false,
});

const emit = defineEmits<{
  'update:modelValue': [value: boolean];
  cancel: [];
  retry: [];
}>();
const { t } = useI18n();

const percent = computed<number | null>(() => {
  const value = Number(props.progress);
  if (!Number.isFinite(value)) return null;
  return Math.max(0, Math.min(100, Math.round(value * 100)));
});
const percentText = computed(() => percent.value === null ? '—' : `${percent.value}%`);

const rows = computed(() => flattenTaskItems(props.children || []));
const failedRows = computed(() => rows.value
  .filter((row: any) => row && taskState(row) === 'failed')
  .slice(0, 8));
const stateMarks: Record<string, string> = { running: '●', succeeded: '✓', failed: '×', waiting: '○' };
// Current authoritative rows only: disappearing or truncated rows are not
// evidence of success. Prioritise active files instead of the last 16 entries.
const stepRows = computed(() => {
  const visible = taskActiveItems(rows.value, 16);
  if (!visible.length) visible.push(...rows.value.slice(0, 16));
  return visible.map(row => {
    const state = taskState(row);
    const total = taskItemTotal(row), current = taskItemCurrent(row);
    return { ...row, state, mark: stateMarks[state] || '○', label: taskItemLabel(row),
      detail: taskItemProgressText(row), rate: state === 'running' ? Number(row.transferRate || 0) : 0,
      percentValue: total > 0 ? Math.min(100, current / total * 100) : 0, total };
  });
});

function formatRate(value: unknown) {
  return formatTaskRate(value);
}
</script>

<style scoped>
.pcl-launch-dialog {
  width: 520px;
  max-width: 94vw;
}

.pcl-launch-dialog__head {
  display: flex;
  align-items: flex-start;
  gap: 10px;
  padding-bottom: 10px;
}

.pcl-launch-dialog__badge {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 34px;
  width: 34px;
  height: 34px;
  border-radius: 5px;
  background: var(--pcl-c6);
  color: var(--pcl-c2);
}

.pcl-launch-dialog__head-copy { min-width: 0; flex: 1; }
.pcl-launch-dialog__head h3 { margin: 3px 0; color: var(--pcl-text); font-size: calc(16px * var(--pcl-font-scale, 1)); font-weight: 500; }
.pcl-launch-dialog__head p { margin: 0; overflow: hidden; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); text-overflow: ellipsis; white-space: nowrap; }
.pcl-launch-dialog__percent { color: var(--pcl-c2); font-size: calc(20px * var(--pcl-font-scale, 1)); font-weight: 600; }
.pcl-launch-dialog__body { padding-top: 0; }
.pcl-launch-dialog__track { margin: 4px 0 10px; }

.pcl-launch-dialog__stage {
  display: flex;
  align-items: center;
  gap: 7px;
  min-height: 20px;
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-launch-dialog__info {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 5px 18px;
  margin: 12px 0;
  padding: 9px 10px;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c8);
}

.pcl-launch-dialog__info div { display: flex; justify-content: space-between; gap: 8px; min-width: 0; }
.pcl-launch-dialog__info span { color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-launch-dialog__info strong { overflow: hidden; color: var(--pcl-text); font-size: calc(9px * var(--pcl-font-scale, 1)); font-weight: 500; text-overflow: ellipsis; white-space: nowrap; }
.pcl-launch-dialog__actions { padding-top: 2px; }

.pcl-launch-dialog__details {
  margin: 6px 0 0;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  overflow-wrap: anywhere;
}
.pcl-launch-dialog__downloads {
  margin: 8px 0 0;
  padding: 7px 8px;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c8);
}
.pcl-launch-dialog__downloads-head {
  margin-bottom: 4px;
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
  letter-spacing: .04em;
}
.pcl-launch-dialog__download {
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto;
  gap: 2px 8px;
  padding: 4px 0;
  border-top: 1px solid var(--pcl-border);
}
.pcl-launch-dialog__download:first-of-type { border-top: 0; }
.pcl-launch-dialog__download-name {
  overflow: hidden;
  color: var(--pcl-text);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}
.pcl-launch-dialog__download-meta {
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
  white-space: nowrap;
}
.pcl-launch-dialog__child-track { grid-column: 1 / -1; margin-top: 2px; }

.pcl-launch-dialog__failures { border-color: color-mix(in srgb, #fb7185 55%, var(--pcl-border)); }
.pcl-launch-dialog__failures .pcl-launch-dialog__downloads-head { color: #fb7185; }
.pcl-launch-dialog__failures .pcl-launch-dialog__download-meta { color: #fecdd3; white-space: normal; }

.pcl-launch-dialog__step { grid-template-columns: 14px minmax(0, 1fr) auto; align-items: center; }
.pcl-launch-dialog__step-mark { color: var(--pcl-muted); font-size: 10px; line-height: 1; }
.pcl-launch-dialog__step.is-running .pcl-launch-dialog__step-mark { color: var(--pcl-c3); }
.pcl-launch-dialog__step.is-succeeded .pcl-launch-dialog__step-mark { color: #34d399; }
.pcl-launch-dialog__step.is-failed .pcl-launch-dialog__step-mark { color: #fb7185; }
.pcl-launch-dialog__step.is-waiting { opacity: .62; }
.pcl-launch-dialog__step .pcl-launch-dialog__download-name { grid-column: 2; }
.pcl-launch-dialog__step .pcl-launch-dialog__download-meta { grid-column: 3; }
.pcl-launch-dialog__step .pcl-launch-dialog__child-track { grid-column: 2 / -1; }

.pcl-launch-dialog__downloads { height: 152px; max-height: 152px; overflow-y: auto; }
.pcl-launch-dialog__step-empty { padding: 8px 2px; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); }
</style>
