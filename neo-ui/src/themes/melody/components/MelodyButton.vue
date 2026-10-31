<template>
  <button
    class="melody-button"
    :class="[`melody-button--${variant}`, { 'is-large': large }]"
    :type="type"
    :disabled="disabled"
    @click="$emit('click', $event)"
  >
    <slot />
  </button>
</template>

<script setup lang="ts">
withDefaults(defineProps<{
  variant?: 'default' | 'primary' | 'ghost' | 'link' | 'dialog';
  large?: boolean;
  disabled?: boolean;
  type?: 'button' | 'submit' | 'reset';
}>(), { variant: 'default', type: 'button' });

defineEmits<{ click: [event: MouseEvent] }>();
</script>
