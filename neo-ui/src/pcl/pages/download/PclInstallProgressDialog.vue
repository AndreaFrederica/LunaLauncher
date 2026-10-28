<template>
  <q-dialog :model-value="modelValue" persistent @update:model-value="emit('update:modelValue', $event)">
    <q-card class="pcl-dialog pcl-install-progress">
      <q-card-section class="pcl-install-progress__head">
        <span class="pcl-install-progress__icon"><PclIcon name="download" :size="22" /></span>
        <div>
          <div class="pcl-kicker">{{ t('pcl.installDialog.kicker') }}</div>
          <h3>{{ title || t('pcl.installDialog.installingPack') }}</h3>
          <p>{{ task?.name || task?.type || t('pcl.installDialog.preparingTask') }}</p>
        </div>
        <span class="pcl-install-progress__percent">{{ percent === null ? '—' : `${percent}%` }}</span>
      </q-card-section>

      <q-card-section class="pcl-install-progress__body">
        <div class="pcl-progress-track pcl-install-progress__track" :class="{ 'is-indeterminate': percent === null }">
          <span :style="percent === null ? undefined : { width: `${percent}%` }" />
        </div>
        <div class="pcl-install-progress__status">
          <PclLoading v-if="!finished" inline />
          <span>{{ task?.details || task?.status || t('pcl.installDialog.preparing') }}</span>
        </div>
        <div v-if="activeFiles.length" class="pcl-install-progress__files">
          <div v-for="file in activeFiles" :key="file.id || file.label" class="pcl-install-progress__current">
            <div class="pcl-install-progress__current-head">
              <span>{{ taskItemLabel(file) }}</span>
              <small>{{ rateText(file) }}</small>
            </div>
            <div class="pcl-progress-track pcl-install-progress__current-track" :class="{ 'is-indeterminate': childPercent(file) === null }">
              <span :style="childPercent(file) === null ? undefined : { width: `${childPercent(file)}%` }" />
            </div>
            <div class="pcl-install-progress__current-meta">
              <span>{{ childProgressText(file) }}</span>
              <span v-if="file.status">{{ file.status }}</span>
            </div>
          </div>
        </div>
        <div v-else-if="!finished" class="pcl-install-progress__current">
          <div class="pcl-install-progress__current-head"><span>{{ task?.status || task?.details || t('pcl.installDialog.nextStep') }}</span></div>
          <div class="pcl-progress-track pcl-install-progress__current-track is-indeterminate"><span /></div>
        </div>
        <div class="pcl-install-progress__stages">
          <div v-for="stage in stages" :key="stage.label" class="pcl-install-progress__stage" :class="stage.state">
            <span class="pcl-install-progress__stage-dot" />
            <span>{{ stage.label }}</span>
          </div>
        </div>
        <div class="pcl-install-progress__summary">
          <span>{{ stats.summary }}</span>
          <span v-if="stats.byteSummary"> · {{ stats.byteSummary }}</span>
          <span v-if="pendingCount"> · {{ t('pcl.installDialog.waitingCount', { count: pendingCount }) }}</span>
          <span v-if="activeCount > 1"> · {{ t('pcl.installDialog.activeCount', { count: activeCount }) }}</span>
        </div>
        <div class="pcl-install-progress__meta">
          <span v-if="rateText(task)">{{ rateText(task) }}</span>
          <span v-if="task?.details">{{ task.details }}</span>
        </div>
      </q-card-section>

      <q-card-actions align="right" class="pcl-install-progress__actions">
        <PclButton variant="ghost" :disabled="finished || cancelling" @click="emit('cancel')">
          {{ cancelling ? t('pcl.installDialog.cancelling') : t('pcl.installDialog.cancel') }}
        </PclButton>
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { computed, onUnmounted, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import PclButton from '../../components/PclButton.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclLoading from '../../components/PclLoading.vue';
import {
  flattenTaskItems,
  isTaskItemFinished,
  formatTaskRate,
  taskItemProgressText,
  taskItemPercent,
  taskActiveItems,
  taskItemLabel,
  taskProgressStats,
} from '../../composables/useTaskProgress';

const props = withDefaults(defineProps<{
  modelValue: boolean;
  title?: string;
  task?: any;
  cancelling?: boolean;
}>(), {
  title: '',
  task: null,
  cancelling: false,
});

const emit = defineEmits<{
  'update:modelValue': [value: boolean];
  cancel: [];
}>();
const { t } = useI18n();

const finished = computed(() => isTaskItemFinished(props.task));
const children = computed<any[]>(() => flattenTaskItems(props.task));
const stats = computed(() => taskProgressStats(props.task, children.value));
const targetPercent = computed<number | null>(() => {
  return stats.value.percent === null ? null : Math.round(stats.value.percent);
});
const percent = ref<number | null>(null);
let percentFrame = 0;
watch(targetPercent, value => {
  cancelAnimationFrame(percentFrame);
  if (value === null) { percent.value = null; return; }
  const start = percent.value ?? 0;
  const started = performance.now();
  const duration = 240;
  const tick = (now: number) => {
    const ratio = Math.min(1, (now - started) / duration);
    const eased = 1 - Math.pow(1 - ratio, 3);
    percent.value = Math.round(start + (value - start) * eased);
    if (ratio < 1) percentFrame = requestAnimationFrame(tick);
  };
  percentFrame = requestAnimationFrame(tick);
}, { immediate: true });
onUnmounted(() => cancelAnimationFrame(percentFrame));
const activeFiles = computed(() => taskActiveItems(children.value, 8));
const activeCount = computed(() => stats.value.active);
const pendingCount = computed(() => stats.value.waiting);
const stages = computed(() => {
  const labels = props.task?.operation === 'modpack.install'
    ? [t('pcl.installDialog.prepare'), t('pcl.installDialog.downloadPack'), t('pcl.installDialog.unpack'), t('pcl.installDialog.downloadMods'), t('pcl.installDialog.done')]
    : [t('pcl.installDialog.prepare'), t('pcl.installDialog.download'), t('pcl.installDialog.install'), t('pcl.installDialog.done')];
  const status = String(props.task?.status || '').toLowerCase();
  const finishedTask = props.task?.state === 'succeeded';
  let active = 0;
  if (finishedTask) active = labels.length - 1;
  else if (children.value.some(child => /\.(jar|zip|litemod)$/i.test(String(child.label || '')))) active = Math.min(labels.length - 2, 3);
  else if (status.includes('unpack') || status.includes('install')) active = Math.min(labels.length - 2, 2);
  else if (children.value.length || status.includes('download')) active = 1;
  return labels.map((label, index) => ({ label, state: index < active ? 'done' : index === active ? 'active' : 'waiting' }));
});
const childPercent = taskItemPercent;
const childProgressText = taskItemProgressText;

function rateText(value: any) {
  if (!value) return '';
  if (Number(value.transferRate) > 0) return formatTaskRate(value.transferRate);
  const match = String(value.details || '').match(/([\d.]+\s*(?:B|KiB|MiB|GiB|KB|MB|GB)\s*\/s)/i);
  return match ? match[1] : '';
}

</script>

<style scoped>
.pcl-install-progress {
  width: 520px;
  max-width: 94vw;
}

.pcl-install-progress__head {
  display: flex;
  align-items: flex-start;
  gap: 10px;
  padding-bottom: 10px;
}

.pcl-install-progress__icon {
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

.pcl-install-progress__head > div {
  min-width: 0;
  flex: 1;
}

.pcl-install-progress__head h3 {
  margin: 3px 0;
  color: var(--pcl-text);
  font-size: calc(16px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-install-progress__head p {
  margin: 0;
  overflow: hidden;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-install-progress__percent {
  color: var(--pcl-c2);
  font-size: calc(20px * var(--pcl-font-scale, 1));
  font-weight: 600;
}

.pcl-install-progress__body {
  padding-top: 0;
}

.pcl-install-progress__track {
  margin: 4px 0 10px;
}

.pcl-install-progress__files {
  display: flex;
  flex-direction: column;
  gap: 7px;
  margin-top: 10px;
  max-height: 190px;
  overflow-y: auto;
}

.pcl-install-progress__current {
  display: flex;
  flex-direction: column;
  gap: 4px;
  padding: 8px 9px;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c8);
}

.pcl-install-progress__current-head,
.pcl-install-progress__current-meta {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  min-width: 0;
}

.pcl-install-progress__current-head span {
  overflow: hidden;
  color: var(--pcl-text);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-install-progress__current-head small {
  flex: 0 0 auto;
  color: var(--pcl-c3);
  font-size: calc(9px * var(--pcl-font-scale, 1));
}

.pcl-install-progress__current-track {
  height: 5px !important;
  margin: 0 !important;
}

.pcl-install-progress__current-meta {
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
}

.pcl-install-progress__current-meta span:last-child {
  overflow: hidden;
  margin-left: auto;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-install-progress__stages {
  display: flex;
  align-items: center;
  gap: 5px;
  margin-top: 12px;
  overflow: hidden;
}

.pcl-install-progress__stage {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  min-width: 0;
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
  white-space: nowrap;
}

.pcl-install-progress__stage:not(:last-child)::after {
  content: '';
  width: 10px;
  height: 1px;
  margin-left: 2px;
  background: var(--pcl-border);
}

.pcl-install-progress__stage-dot {
  width: 5px;
  height: 5px;
  border-radius: 50%;
  background: var(--pcl-c6);
}

.pcl-install-progress__stage.done { color: #34d399; }
.pcl-install-progress__stage.done .pcl-install-progress__stage-dot { background: #34d399; }
.pcl-install-progress__stage.active { color: var(--pcl-c3); }
.pcl-install-progress__stage.active .pcl-install-progress__stage-dot { background: var(--pcl-c3); box-shadow: 0 0 6px var(--pcl-c3); }

.pcl-install-progress__summary {
  display: flex;
  flex-wrap: wrap;
  gap: 5px;
  margin-top: 8px;
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
}

.pcl-progress-track.is-indeterminate > span {
  width: 40% !important;
  animation: pcl-progress-indeterminate 1.1s ease-in-out infinite;
}

@keyframes pcl-progress-indeterminate {
  0% { margin-left: -40%; }
  100% { margin-left: 100%; }
}

.pcl-install-progress__children {
  display: flex;
  flex-direction: column;
  gap: 8px;
  max-height: 220px;
  margin-top: 10px;
  overflow-y: auto;
  padding: 8px;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c8);
}

.pcl-install-progress__children-head,
.pcl-install-progress__child-head,
.pcl-install-progress__child-meta {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  min-width: 0;
}

.pcl-install-progress__children-head {
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  font-weight: 600;
}

.pcl-install-progress__children-head small {
  color: var(--pcl-c5);
  font-size: calc(9px * var(--pcl-font-scale, 1));
}

.pcl-install-progress__child {
  display: flex;
  flex-direction: column;
  gap: 4px;
  min-width: 0;
}

.pcl-install-progress__child-head span {
  overflow: hidden;
  color: var(--pcl-text);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-install-progress__child-head small,
.pcl-install-progress__child-meta {
  color: var(--pcl-c5);
  font-size: calc(9px * var(--pcl-font-scale, 1));
}

.pcl-install-progress__child-meta span:last-child {
  overflow: hidden;
  margin-left: auto;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-install-progress__child-track {
  height: 4px !important;
  margin: 0 !important;
}

.pcl-install-progress__status {
  display: flex;
  align-items: center;
  gap: 7px;
  min-height: 24px;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-install-progress__steps {
  display: flex;
  flex-direction: column;
  gap: 4px;
  margin-top: 9px;
  padding: 8px;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c8);
}

.pcl-install-progress__step {
  display: flex;
  align-items: center;
  gap: 7px;
  min-width: 0;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-install-progress__step > span:nth-child(2) {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-install-progress__step small {
  margin-left: auto;
  color: var(--pcl-c5);
  font-size: calc(9px * var(--pcl-font-scale, 1));
}

.pcl-install-progress__dot {
  flex: 0 0 6px;
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: var(--pcl-c5);
}

.pcl-install-progress__dot.done {
  background: var(--pcl-c2);
}

.pcl-install-progress__dot.active {
  background: var(--pcl-c3);
  box-shadow: 0 0 0 3px color-mix(in srgb, var(--pcl-c3) 20%, transparent);
}

.pcl-install-progress__meta {
  display: flex;
  justify-content: space-between;
  gap: 10px;
  min-height: 16px;
  margin-top: 7px;
  color: var(--pcl-c5);
  font-size: calc(9px * var(--pcl-font-scale, 1));
}

.pcl-install-progress__actions {
  padding-top: 0;
}
</style>
