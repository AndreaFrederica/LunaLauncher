<template>
  <div v-show="active" class="pcl-settings-section">
    <PclCard :title="t('pcl.auto2.k7d6742783a')">
      <PclHint v-if="maxMem < 2048" theme="yellow">{{ t('pcl.auto2.kb9999f751b') }}</PclHint>
      <div class="pcl-choice-row">
        <PclRadio :model-value="memoryMode" value="auto" :label="t('pcl.auto2.k716e01ff80')" @update:model-value="onMemoryMode" />
        <PclRadio :model-value="memoryMode" value="custom" :label="t('pcl.auto2.kc493338e8c')" @update:model-value="onMemoryMode" />
        <span class="pcl-form-row__hint">{{ t('pcl.auto2.k5a65c23c82') }}</span>
      </div>
      <div class="pcl-memory-live-head"><span>{{ t('pcl.auto2.ke2dadf543a') }}</span><span class="pcl-memory-live-right">{{ t('pcl.auto2.kec461a8f29') }}<span class="pcl-memory-live-dot" />{{ t('pcl.auto2.k06df5e4583') }}</span></div>
      <div class="pcl-memory-bar"><span class="pcl-memory-bar__used" :style="{ width: `${memoryUsedPercent}%` }" /><span class="pcl-memory-bar__allocated" :style="{ left: `${memoryUsedPercent}%`, width: `${memoryAllocAfterPercent}%` }" /><span class="pcl-memory-bar__divider" :style="{ left: `${memoryUsedPercent}%` }" /></div>
      <div class="pcl-memory-values"><strong>{{ memoryUsedGib }} GiB <em>/</em> {{ memoryTotalGib }} GiB</strong><strong>{{ formatMib(maxMem) }}</strong></div>
      <div class="pcl-memory-legend"><span><b>{{ t('pcl.auto2.k97ebe06e74') }}</b>{{ memoryUsedPercent.toFixed(1) }}%</span><span><b>{{ t('pcl.auto2.k31793b6729') }}</b>{{ formatMib(draftNumber('MinMemAlloc', 512)) }}</span><span><b>{{ t('pcl.auto2.ked690ebb15') }}</b>{{ formatMib(draftNumber('MaxMemAlloc', 4096)) }}</span></div>
      <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.kccb8f3efaa') }}</span><div class="pcl-form-row__control"><PclSlider :model-value="draftNumber('MinMemAlloc', 512)" :min="256" :max="16384" :step="256" :disabled="memoryMode === 'auto'" :format="formatMib" @update:model-value="drafts.MinMemAlloc = $event" @change="save('MinMemAlloc', $event)" /></div><span class="pcl-form-row__hint">{{ t('ui.memoryUnit') }}</span></div>
      <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k1f4bebe9b9') }}</span><div class="pcl-form-row__control"><PclSlider :model-value="draftNumber('MaxMemAlloc', 4096)" :min="512" :max="32768" :step="512" :disabled="memoryMode === 'auto'" :format="formatMib" @update:model-value="drafts.MaxMemAlloc = $event" @change="save('MaxMemAlloc', $event)" /></div><span class="pcl-form-row__hint">{{ t('ui.memoryUnit') }}</span></div>
    </PclCard>
  </div>
</template>

<script setup lang="ts">
import PclCard from '../../components/PclCard.vue';
import PclHint from '../../components/PclHint.vue';
import PclRadio from '../../components/PclRadio.vue';
import PclSlider from '../../components/PclSlider.vue';
defineProps<{ active: boolean; t: (key: string) => string; maxMem: number; memoryMode: string; memoryUsedPercent: number; memoryAllocAfterPercent: number; memoryUsedGib: string; memoryTotalGib: string; drafts: Record<string, any>; draftNumber: (key: string, fallback: number) => number; formatMib: (value: number) => string; onMemoryMode: (value: string | number | boolean) => void; save: (key: string, value: any) => void }>();
</script>
