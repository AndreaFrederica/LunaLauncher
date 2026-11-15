<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef-slider">
    <input
      :value="modelValue"
      type="range"
      :min="min"
      :max="max"
      :step="step"
      :disabled="disabled"
      :aria-label="label"
      @input="emit('update:modelValue', Number(($event.target as HTMLInputElement).value))"
    />
    <span class="ef-slider__value">{{ display }}</span>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
const props = withDefaults(defineProps<{ modelValue: number; min?: number; max?: number; step?: number; label?: string; disabled?: boolean; suffix?: string }>(), { min: 0, max: 100, step: 1, label: '', disabled: false, suffix: '' });
const emit = defineEmits<{ 'update:modelValue': [value: number] }>();
const display = computed(() => `${props.modelValue}${props.suffix}`);
</script>

<style scoped>
.ef-slider {
  display: flex;
  align-items: center;
  gap: 10px;
  width: 100%;
}
.ef-slider input[type='range'] {
  flex: 1;
  height: 18px;
  margin: 0;
  accent-color: var(--ef-signal);
  background: transparent;
  cursor: pointer;
}
.ef-slider input[type='range']::-webkit-slider-runnable-track {
  height: 2px;
  background: var(--ef-rule);
}
.ef-slider input[type='range']::-webkit-slider-thumb {
  width: 12px;
  height: 12px;
  margin-top: -5px;
  border: 1px solid var(--ef-ink);
  border-radius: 0;
  background: var(--ef-signal);
  appearance: none;
}
.ef-slider__value {
  min-width: 48px;
  color: var(--ef-ink);
  font-family: var(--ef-font-ui);
  font-size: 11px;
  font-variant-numeric: tabular-nums;
  text-align: right;
}
</style>
