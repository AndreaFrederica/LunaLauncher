<template>
  <div class="pcl-slider">
    <input
      type="range"
      :value="modelValue"
      :min="min"
      :max="max"
      :step="step"
      :disabled="disabled"
      :style="{ '--pcl-slider-progress': `${progress}%` }"
      @input="emit('update:modelValue', Number(($event.target as HTMLInputElement).value))"
      @change="emit('change', Number(($event.target as HTMLInputElement).value))"
    />
    <span v-if="showValue" class="pcl-slider__value">{{ displayValue }}</span>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';

const props = withDefaults(defineProps<{
  modelValue: number;
  min?: number;
  max?: number;
  step?: number;
  disabled?: boolean;
  showValue?: boolean;
  format?: (value: number) => string;
}>(), {
  min: 0,
  max: 100,
  step: 1,
  disabled: false,
  showValue: true,
  format: undefined,
});

const emit = defineEmits<{
  'update:modelValue': [value: number];
  change: [value: number];
}>();
const progress = computed(() => {
  const span = props.max - props.min;
  if (span <= 0) return 0;
  return Math.max(0, Math.min(100, ((props.modelValue - props.min) / span) * 100));
});
const displayValue = computed(() => props.format ? props.format(props.modelValue) : String(props.modelValue));
</script>
