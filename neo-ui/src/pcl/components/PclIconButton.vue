<template>
  <button
    type="button"
    class="pcl-icon-btn"
    :class="[`pcl-icon-btn--${theme}`, { 'pcl-icon-btn--danger': danger }]"
    :style="{ width: `${size}px`, height: `${size}px` }"
    :title="label || undefined"
    :aria-label="label || undefined"
    :disabled="disabled"
    @click="emit('click', $event)"
  >
    <PclIcon :name="icon" :size="Math.round(size * 0.62)" />
  </button>
</template>

<script setup lang="ts">
import PclIcon from './PclIcon.vue';

withDefaults(defineProps<{
  icon: string;
  label?: string;
  size?: number;
  theme?: 'color' | 'white';
  danger?: boolean;
  disabled?: boolean;
}>(), {
  label: '',
  size: 30,
  theme: 'color',
  danger: false,
  disabled: false,
});

const emit = defineEmits<{ click: [event: MouseEvent] }>();
</script>
