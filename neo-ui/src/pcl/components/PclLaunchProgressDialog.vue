<template>
  <q-dialog :model-value="modelValue" persistent @update:model-value="emit('update:modelValue', $event)">
    <q-card class="pcl-dialog pcl-launch-dialog">
      <q-card-section class="pcl-launch-dialog__head">
        <span class="pcl-launch-dialog__badge"><PclIcon name="play" :size="20" /></span>
        <div class="pcl-launch-dialog__head-copy">
          <span class="pcl-kicker">MINECRAFT LAUNCH</span>
          <h3>{{ state === 'launched' ? '启动成功' : state === 'failed' ? '启动失败' : state === 'cancelled' ? '启动已取消' : '正在启动游戏' }}</h3>
          <p>{{ instanceName || 'Minecraft 实例' }}</p>
        </div>
        <span class="pcl-launch-dialog__percent">{{ percentText }}</span>
      </q-card-section>

      <q-card-section class="pcl-launch-dialog__body">
        <div class="pcl-progress-track pcl-launch-dialog__track" :class="{ 'is-indeterminate': percent === null }">
          <span :style="percent === null ? undefined : { width: `${percent}%` }" />
        </div>

        <div class="pcl-launch-dialog__stage">
          <PclLoading v-if="state === 'launching'" inline />
          <PclIcon v-else-if="state === 'launched'" name="circle-check" :size="15" />
          <PclIcon v-else name="info" :size="15" />
          <span>{{ stage || (state === 'launching' ? '正在准备启动…' : state === 'launched' ? '游戏进程已启动' : '启动流程已结束') }}</span>
        </div>
        <p v-if="details" class="pcl-launch-dialog__details">{{ details }}</p>
        <div v-if="failedRows.length" class="pcl-launch-dialog__downloads pcl-launch-dialog__failures">
          <div class="pcl-launch-dialog__downloads-head">失败的项目</div>
          <div v-for="row in failedRows" :key="row.id" class="pcl-launch-dialog__download">
            <span class="pcl-launch-dialog__download-name">{{ row.label || '下载任务' }}</span>
            <span class="pcl-launch-dialog__download-meta">{{ row.details || row.status || '校验失败' }}</span>
          </div>
        </div>
        <div v-if="activeRows.length" class="pcl-launch-dialog__downloads">
          <div class="pcl-launch-dialog__downloads-head">正在处理</div>
          <div v-for="row in activeRows" :key="row.id" class="pcl-launch-dialog__download">
            <span class="pcl-launch-dialog__download-name">{{ row.label || row.details || '子任务' }}</span>
            <span class="pcl-launch-dialog__download-meta">{{ childPercent(row) }}<template v-if="row.transferRate"> · {{ formatRate(row.transferRate) }}</template></span>
            <div class="pcl-progress-track pcl-launch-dialog__child-track"><span :style="{ width: `${childPercentValue(row)}%` }" /></div>
          </div>
        </div>

        <div class="pcl-launch-dialog__info">
          <div><span>启动账户</span><strong>{{ accountName || '未选择账户' }}</strong></div>
          <div><span>登录方式</span><strong>{{ loginMethod || '—' }}</strong></div>
          <div><span>启动进度</span><strong>{{ percentText }}</strong></div>
          <div v-if="speed > 0"><span>下载速度</span><strong>{{ formatRate(speed) }}</strong></div>
        </div>

        <PclHint v-if="state === 'launched'" theme="blue">
          {{ hint || '游戏已经启动，可以关闭此窗口。' }}
        </PclHint>
        <PclHint v-else-if="error" theme="red" :closable="false">{{ error }}</PclHint>
        <PclHint v-else theme="blue">正在准备游戏环境，请稍候。</PclHint>
      </q-card-section>

      <q-card-actions align="right" class="pcl-launch-dialog__actions">
        <PclButton v-if="state === 'launching'" variant="ghost" icon="x" :disabled="cancelling" @click="emit('cancel')">
          {{ cancelling ? '正在取消…' : '取消启动' }}
        </PclButton>
        <PclButton v-else-if="state === 'failed'" variant="highlight" icon="refresh-cw" @click="emit('retry')">重试</PclButton>
        <PclButton v-if="state !== 'launching'" variant="ghost" icon="x" @click="emit('update:modelValue', false)">关闭</PclButton>
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import PclButton from './PclButton.vue';
import PclHint from './PclHint.vue';
import PclIcon from './PclIcon.vue';
import PclLoading from './PclLoading.vue';

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

const percent = computed<number | null>(() => {
  const value = Number(props.progress);
  if (!Number.isFinite(value)) return null;
  return Math.max(0, Math.min(100, Math.round(value * 100)));
});
const percentText = computed(() => percent.value === null ? '—' : `${percent.value}%`);

const failedRows = computed(() => (props.children || [])
  .filter((row: any) => row && (row.state === 'failed' || /fail|失败/i.test(String(row.status || ''))))
  .slice(0, 8));
const activeRows = computed(() => (props.children || [])
  .filter((row: any) => row && row.state === 'running' && (Number(row.total) > 0 || row.details || row.label))
  .slice(0, 8));
function childPercentValue(row: any) {
  const total = Number(row?.total || 0), current = Number(row?.current || 0);
  return total > 0 ? Math.max(0, Math.min(100, Math.round(current / total * 100))) : 0;
}
function childPercent(row: any) {
  const total = Number(row?.total || 0), current = Number(row?.current || 0);
  return total > 0 ? `${childPercentValue(row)}%` : (row?.state || '进行中');
}

function formatRate(value: unknown) {
  const bytes = Number(value || 0);
  if (!bytes) return '';
  if (bytes < 1024) return `${bytes} B/s`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB/s`;
  return `${(bytes / 1024 / 1024).toFixed(1)} MiB/s`;
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
</style>
