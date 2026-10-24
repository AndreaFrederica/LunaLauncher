<template>
  <PclCard title="游戏内存">
    <template #actions>
      <span class="pcl-memory-live">
        <span class="pcl-memory-live__dot" />
        实时刷新
      </span>
    </template>

    <div class="pcl-memory-mode">
      <PclRadio v-model="mode" value="auto" label="自动配置" name="pcl-memory-mode" />
      <PclRadio v-model="mode" value="custom" label="自定义" name="pcl-memory-mode" />
      <span class="pcl-memory-mode__hint">{{ mode === 'auto' ? `自动配置会设为 ${autoLabel}` : '手动设置最小/最大内存' }}</span>
    </div>

    <div class="pcl-memory-usage">
      <div class="pcl-memory-usage__head">
        <span>已使用内存 / 已安装内存</span>
        <strong>{{ usedGiB }} GiB <em>/</em> {{ totalGiB }} GiB</strong>
      </div>
      <div class="pcl-memory-track">
        <span class="pcl-memory-fill" :style="{ width: `${usedPercent}%` }" />
        <span class="pcl-memory-allocated" :style="{ left: `${usedPercent}%`, width: `${allocationAfterPercent}%` }" />
        <span class="pcl-memory-divider" :style="{ left: `${usedPercent}%` }" />
      </div>
      <div class="pcl-memory-usage__foot">
        <span>系统占用 {{ usedPercent.toFixed(1) }}%</span>
        <span>游戏分配 <strong>{{ allocationGiB }} GiB</strong></span>
      </div>
    </div>

    <div class="pcl-memory-sliders">
      <label class="pcl-memory-field">
        <span>最小内存</span>
        <PclSlider
          :model-value="minMemory"
          :min="512"
          :max="8192"
          :step="256"
          :format="formatMiB"
          @update:model-value="minMemory = $event"
          @change="save('MinMemAlloc', $event)"
        />
      </label>
      <label class="pcl-memory-field">
        <span>最大内存</span>
        <PclSlider
          :model-value="maxMemory"
          :min="1024"
          :max="32768"
          :step="512"
          :format="formatMiB"
          @update:model-value="maxMemory = $event"
          @change="save('MaxMemAlloc', $event)"
        />
      </label>
    </div>

    <PclHint theme="blue">
      内存条每 2 秒读取一次系统物理内存；游戏分配位置显示当前实例的最大内存。自动配置会按系统内存推荐一个上限，仍可手动覆盖。
    </PclHint>
  </PclCard>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import PclCard from '../../components/PclCard.vue';
import PclHint from '../../components/PclHint.vue';
import PclRadio from '../../components/PclRadio.vue';
import PclSlider from '../../components/PclSlider.vue';
import { launcher } from '../../../boot/launcher-api';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ settings: any[]; busy: boolean; execute: Execute }>();
const emit = defineEmits<{ save: [payload: { key: string; value: any }] }>();

const mode = ref<'auto' | 'custom'>('auto');
const minMemory = ref(512);
const maxMemory = ref(4096);
const totalMiB = ref(0);
const usedMiB = ref(0);
const availableMiB = ref(0);
let memorySubscription = '';
let unlistenMemory: (() => void) | undefined;

const totalGiB = computed(() => formatGiB(totalMiB.value));
const usedGiB = computed(() => formatGiB(usedMiB.value));
const allocationGiB = computed(() => formatGiB(maxMemory.value));
const usedPercent = computed(() => totalMiB.value > 0 ? Math.max(0, Math.min(100, usedMiB.value * 100 / totalMiB.value)) : 0);
const allocationPercent = computed(() => totalMiB.value > 0 ? Math.max(0, Math.min(100, maxMemory.value * 100 / totalMiB.value)) : 0);
const allocationAfterPercent = computed(() => Math.max(0, Math.min(100 - usedPercent.value, allocationPercent.value)));
const autoLabel = computed(() => {
  const total = totalMiB.value || 8192;
  const recommended = Math.max(1024, Math.min(8192, Math.floor(total * 0.5 / 512) * 512));
  return formatMiB(recommended);
});

function val(key: string): unknown {
  return props.settings?.find(item => item.key === key)?.value;
}
function numberValue(key: string, fallback: number): number {
  const value = val(key);
  const number = typeof value === 'string' ? Number(value) : Number(value ?? fallback);
  return Number.isFinite(number) ? number : fallback;
}
function formatGiB(value: number): string {
  return (Number(value || 0) / 1024).toFixed(1);
}
function formatMiB(value: number): string {
  return `${Number(value || 0)} MiB`;
}
function syncSettings() {
  minMemory.value = numberValue('MinMemAlloc', 512);
  maxMemory.value = numberValue('MaxMemAlloc', 4096);
}
watch(() => props.settings, syncSettings, { immediate: true, deep: true });

async function refreshMemory() {
  try {
    const result = await props.execute('runtime.memory');
    const data = result?.data || result || {};
    const total = Number(data.totalMiB || 0), used = Number(data.usedMiB || 0), available = Number(data.availableMiB || 0);
    if (total > 0) {
      totalMiB.value = total;
      usedMiB.value = Math.max(0, Math.min(total, used));
      availableMiB.value = available;
    }
  } catch {
    // Older backends may not expose runtime.memory; keep the last sample.
  }
}

async function subscribeMemory() {
  try {
    const result: any = await props.execute('runtime.memory.subscribe');
    memorySubscription = String(result?.subscriptionId || '');
    if (!memorySubscription) return;
    unlistenMemory = await launcher.onStream((batch: any) => {
      if (!batch || batch.subscriptionId !== memorySubscription) return;
      for (const event of batch.events || []) {
        if (event.kind !== 'memory.sample') continue;
        const total = Number(event.totalMiB || 0);
        if (total > 0) {
          totalMiB.value = total;
          usedMiB.value = Math.max(0, Math.min(total, Number(event.usedMiB || 0)));
          availableMiB.value = Number(event.availableMiB || 0);
        }
      }
    });
  } catch {
    // Older backends may not expose the memory stream; the one-shot read remains.
  }
}

function save(key: string, value: number) {
  if (key === 'MinMemAlloc') minMemory.value = value;
  if (key === 'MaxMemAlloc') maxMemory.value = value;
  mode.value = 'custom';
  emit('save', { key, value });
}

watch(mode, value => {
  if (value !== 'auto' || !totalMiB.value) return;
  const recommended = Math.max(1024, Math.min(8192, Math.floor(totalMiB.value * 0.5 / 512) * 512));
  if (maxMemory.value !== recommended) {
    maxMemory.value = recommended;
    emit('save', { key: 'MaxMemAlloc', value: recommended });
  }
});

onMounted(() => {
  void refreshMemory();
  void subscribeMemory();
});
onBeforeUnmount(() => {
  unlistenMemory?.();
  if (memorySubscription) {
    const subscriptionId = memorySubscription;
    memorySubscription = '';
    void props.execute('event.unsubscribe', { subscriptionId }).catch(() => {});
  }
});
</script>

<style scoped>
.pcl-memory-live {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-memory-live__dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: #34d399;
  box-shadow: 0 0 0 0 rgba(52, 211, 153, .5);
  animation: pcl-memory-pulse 2s ease-out infinite;
}

@keyframes pcl-memory-pulse {
  0% { box-shadow: 0 0 0 0 rgba(52, 211, 153, .5); }
  70% { box-shadow: 0 0 0 6px rgba(52, 211, 153, 0); }
  100% { box-shadow: 0 0 0 0 rgba(52, 211, 153, 0); }
}

.pcl-memory-mode {
  display: flex;
  align-items: center;
  gap: 16px;
  margin-bottom: 12px;
}

.pcl-memory-mode__hint {
  margin-left: auto;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-memory-usage {
  padding: 10px 12px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c7);
}

.pcl-memory-usage__head,
.pcl-memory-usage__foot {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 12px;
}

.pcl-memory-usage__head {
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-memory-usage__head strong {
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  font-weight: 400;
}

.pcl-memory-usage__head em {
  margin: 0 3px;
  color: var(--pcl-muted);
  font-style: normal;
}

.pcl-memory-track {
  position: relative;
  height: 9px;
  margin: 8px 0 7px;
  overflow: hidden;
  border-radius: 3px;
  background: var(--pcl-c6);
}

.pcl-memory-allocated {
  position: absolute;
  top: 0;
  bottom: 0;
  border-radius: 0 3px 3px 0;
  background: #8fc4ff;
  transition: left .8s cubic-bezier(.22, 1, .36, 1), width .8s cubic-bezier(.22, 1, .36, 1);
}

.pcl-memory-fill {
  position: absolute;
  top: 0;
  bottom: 0;
  left: 0;
  border-radius: 3px 0 0 3px;
  background: var(--pcl-c3);
  transition: width .8s cubic-bezier(.22, 1, .36, 1);
}

.pcl-memory-divider {
  position: absolute;
  top: 0;
  bottom: 0;
  width: 2px;
  background: #fff;
  box-shadow: 0 0 0 1px rgba(42, 128, 227, .32);
  transition: left .8s cubic-bezier(.22, 1, .36, 1);
}

.pcl-memory-usage__foot {
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-memory-usage__foot strong {
  color: var(--pcl-c2);
  font-weight: 400;
}

.pcl-memory-sliders {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 16px;
  margin: 14px 0;
}

.pcl-memory-field {
  display: flex;
  flex-direction: column;
  gap: 5px;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

@media (max-width: 700px) {
  .pcl-memory-sliders {
    grid-template-columns: 1fr;
  }
}
</style>
