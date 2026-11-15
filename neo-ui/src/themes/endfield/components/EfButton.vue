<template>
  <button
    :type="type"
    class="ef-btn"
    :class="[variantClass, sizeClass, { 'is-busy': busy }]"
    :disabled="disabled || busy"
    :aria-busy="busy ? 'true' : undefined"
    @click="emit('click', $event)"
  >
    <span v-if="icon" class="material-icons">{{ icon }}</span>
    <slot />
  </button>
</template>

<script setup lang="ts">
import { computed } from 'vue';
const props = withDefaults(defineProps<{
  variant?: 'normal' | 'highlight' | 'accent' | 'red' | 'ghost';
  size?: 'sm' | 'md' | 'lg';
  icon?: string;
  disabled?: boolean;
  busy?: boolean;
  type?: 'button' | 'submit' | 'reset';
}>(), { variant: 'normal', size: 'md', disabled: false, busy: false, type: 'button' });
const emit = defineEmits<{ click: [event: MouseEvent] }>();

const variantClass = computed(() => {
  if (props.variant === 'accent' || props.variant === 'highlight') return 'go';
  if (props.variant === 'red') return 'danger';
  return '';
});
const sizeClass = computed(() => (props.size === 'sm' ? 'small' : ''));
</script>

<style scoped>
.ef-btn .material-icons { font-size: 1.1em; }
</style>
