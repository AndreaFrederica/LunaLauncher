<template>
  <q-dialog :model-value="modelValue" persistent @update:model-value="emit('update:modelValue', $event)">
    <q-card class="pcl-dialog pcl-install-progress">
      <q-card-section class="pcl-install-progress__head">
        <span class="pcl-install-progress__icon"><PclIcon name="download" :size="22" /></span>
        <div>
          <div class="pcl-kicker">INSTALLATION</div>
          <h3>{{ title || '正在安装整合包' }}</h3>
          <p>{{ task?.name || task?.type || '准备安装任务' }}</p>
        </div>
        <span class="pcl-install-progress__percent">{{ percent === null ? '—' : `${percent}%` }}</span>
      </q-card-section>

      <q-card-section class="pcl-install-progress__body">
        <div class="pcl-progress-track pcl-install-progress__track" :class="{ 'is-indeterminate': percent === null }">
          <span :style="percent === null ? undefined : { width: `${percent}%` }" />
        </div>
        <div class="pcl-install-progress__status">
          <PclLoading v-if="!finished" inline />
          <span>{{ task?.details || task?.status || '正在准备安装…' }}</span>
        </div>
        <div v-if="activeFiles.length" class="pcl-install-progress__files">
          <div v-for="file in activeFiles" :key="file.id || file.label" class="pcl-install-progress__current">
            <div class="pcl-install-progress__current-head">
              <span>{{ file.label || file.status || '正在下载' }}</span>
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
          <div class="pcl-install-progress__current-head"><span>{{ task?.status || '正在准备下一步…' }}</span></div>
          <div class="pcl-progress-track pcl-install-progress__current-track is-indeterminate"><span /></div>
        </div>
        <div class="pcl-install-progress__stages">
          <div v-for="stage in stages" :key="stage.label" class="pcl-install-progress__stage" :class="stage.state">
            <span class="pcl-install-progress__stage-dot" />
            <span>{{ stage.label }}</span>
          </div>
        </div>
        <div class="pcl-install-progress__summary">
          <span>已完成 {{ childDone }} / {{ children.length }}</span>
          <span v-if="pendingCount"> · 等待 {{ pendingCount }}</span>
          <span v-if="activeCount > 1"> · {{ activeCount }} 个并行下载</span>
        </div>
        <div class="pcl-install-progress__meta">
          <span v-if="rateText(task)">{{ rateText(task) }}</span>
          <span v-if="task?.details">{{ task.details }}</span>
        </div>
      </q-card-section>

      <q-card-actions align="right" class="pcl-install-progress__actions">
        <PclButton variant="ghost" :disabled="finished || cancelling" @click="emit('cancel')">
          {{ cancelling ? '正在取消…' : '取消安装' }}
        </PclButton>
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { computed, onUnmounted, ref, watch } from 'vue';
import PclButton from '../../components/PclButton.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclLoading from '../../components/PclLoading.vue';

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

const finished = computed(() => Boolean(props.task?.finished) || ['succeeded', 'failed', 'cancelled', 'aborted', 'finished', 'completed'].includes(String(props.task?.state || '').toLowerCase()));
const targetPercent = computed<number | null>(() => {
  const total = Number(props.task?.totalProgress || props.task?.total || 0);
  const progress = Number(props.task?.progress || 0);
  if (total > 0) return Math.max(0, Math.min(100, Math.round(progress / total * 100)));
  if (props.task?.finished) return 100;
  return null;
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
const children = computed<any[]>(() => {
  const source = Array.isArray(props.task?.children) ? props.task.children : Array.isArray(props.task?.steps) ? props.task.steps : [];
  const seen = new Set<string>();
  return source.filter((child: any) => {
    const key = String(child.id || child.label || '');
    if (!key || seen.has(key)) return false;
    seen.add(key);
    return true;
  }).slice(0, 64);
});
const childDone = computed(() => children.value.filter(child => ['succeeded', 'failed', 'cancelled'].includes(String(child.state || '').toLowerCase())).length);
const activeFiles = computed(() => children.value.filter(child => String(child.state || '').toLowerCase() === 'running').slice(0, 8));
const activeCount = computed(() => children.value.filter(child => String(child.state || '').toLowerCase() === 'running').length);
const pendingCount = computed(() => children.value.filter(child => String(child.state || '').toLowerCase() === 'waiting').length);
const stages = computed(() => {
  const labels = props.task?.operation === 'modpack.install'
    ? ['准备', '下载整合包', '解压安装', '下载模组', '完成']
    : ['准备', '下载', '安装', '完成'];
  const status = String(props.task?.status || '').toLowerCase();
  const finishedTask = Boolean(props.task?.finished);
  let active = 0;
  if (finishedTask) active = labels.length - 1;
  else if (children.value.some(child => /\.(jar|zip|litemod)$/i.test(String(child.label || '')))) active = Math.min(labels.length - 2, 3);
  else if (status.includes('unpack') || status.includes('解压') || status.includes('install') || status.includes('安装')) active = Math.min(labels.length - 2, 2);
  else if (children.value.length || status.includes('下载') || status.includes('download')) active = 1;
  return labels.map((label, index) => ({ label, state: index < active ? 'done' : index === active ? 'active' : 'waiting' }));
});
function childPercent(child: any): number | null {
  const total = Number(child?.total || 0);
  const current = Number(child?.current || 0);
  if (total > 0) return Math.max(0, Math.min(100, Math.round(current / total * 100)));
  if (['succeeded', 'failed', 'cancelled'].includes(String(child?.state || '').toLowerCase())) return 100;
  return null;
}
function childProgressText(child: any): string {
  const total = Number(child?.total || 0);
  const current = Number(child?.current || 0);
  if (total > 0) return `${formatSize(current)} / ${formatSize(total)}`;
  return child?.state === 'succeeded' ? '已完成' : '等待中';
}

function rateText(value: any) {
  if (!value) return '';
  if (Number(value.transferRate) > 0) return formatRate(value.transferRate);
  const match = String(value.details || '').match(/([\d.]+\s*(?:B|KiB|MiB|GiB|KB|MB|GB)\s*\/s)/i);
  return match ? match[1] : '';
}

function formatRate(value: unknown) {
  const bytes = Number(value || 0);
  if (!bytes) return '';
  return `${formatSize(bytes)}/s`;
}

function formatSize(value: unknown) {
  const bytes = Number(value || 0);
  if (!bytes) return '0 B';
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)} MiB`;
  return `${(bytes / 1024 / 1024 / 1024).toFixed(2)} GiB`;
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
