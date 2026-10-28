<template>
  <button
    :type="type"
    class="pcl-btn"
    :class="[`pcl-btn--${variant}`, `pcl-btn--${size}`, { 'pcl-btn--block': block }]"
    :disabled="disabled"
    @click="emit('click', $event)"
  >
    <PclIcon v-if="icon" :name="icon" :size="iconSize" />
    <slot />
  </button>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import PclIcon from './PclIcon.vue';

const props = withDefaults(defineProps<{
  variant?: 'normal' | 'highlight' | 'accent' | 'red' | 'ghost';
  size?: 'sm' | 'md' | 'lg' | 'xl';
  icon?: string;
  iconSize?: number;
  disabled?: boolean;
  block?: boolean;
  type?: 'button' | 'submit' | 'reset';
}>(), {
  variant: 'normal',
  size: 'md',
  icon: '',
  iconSize: 16,
  disabled: false,
  block: false,
  type: 'button',
});

const emit = defineEmits<{ click: [event: MouseEvent] }>();
const iconSize = computed(() => props.iconSize);
</script>
