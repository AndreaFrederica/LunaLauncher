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
        <div v-if="children.length" class="pcl-install-progress__children">
          <div class="pcl-install-progress__children-head">
            <span>文件进度</span>
            <small>{{ childDone }} / {{ children.length }}</small>
          </div>
          <div v-for="child in children" :key="child.id || child.label" class="pcl-install-progress__child">
            <div class="pcl-install-progress__child-head">
              <span>{{ child.label || child.status || '下载文件' }}</span>
              <small v-if="child.transferRate">{{ formatRate(child.transferRate) }}</small>
            </div>
            <div class="pcl-progress-track pcl-install-progress__child-track" :class="{ 'is-indeterminate': childPercent(child) === null }">
              <span :style="childPercent(child) === null ? undefined : { width: `${childPercent(child)}%` }" />
            </div>
            <div class="pcl-install-progress__child-meta">
              <span>{{ childProgressText(child) }}</span>
              <span v-if="child.status || child.details">{{ child.status || child.details }}</span>
            </div>
          </div>
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
import { computed } from 'vue';
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
const percent = computed<number | null>(() => {
  const total = Number(props.task?.totalProgress || props.task?.total || 0);
  const progress = Number(props.task?.progress || 0);
  if (total > 0) return Math.max(0, Math.min(100, Math.round(progress / total * 100)));
  if (props.task?.finished) return 100;
  return null;
});
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
  font-size: 16px;
  font-weight: 500;
}

.pcl-install-progress__head p {
  margin: 0;
  overflow: hidden;
  color: var(--pcl-muted);
  font-size: 10px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-install-progress__percent {
  color: var(--pcl-c2);
  font-size: 20px;
  font-weight: 600;
}

.pcl-install-progress__body {
  padding-top: 0;
}

.pcl-install-progress__track {
  margin: 4px 0 10px;
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
  font-size: 10px;
  font-weight: 600;
}

.pcl-install-progress__children-head small {
  color: var(--pcl-c5);
  font-size: 9px;
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
  font-size: 10px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-install-progress__child-head small,
.pcl-install-progress__child-meta {
  color: var(--pcl-c5);
  font-size: 9px;
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
  font-size: 11px;
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
  font-size: 10px;
}

.pcl-install-progress__step > span:nth-child(2) {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-install-progress__step small {
  margin-left: auto;
  color: var(--pcl-c5);
  font-size: 9px;
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
  font-size: 9px;
}

.pcl-install-progress__actions {
  padding-top: 0;
}
</style>
