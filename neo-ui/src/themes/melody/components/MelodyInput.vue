<template>
  <input
    class="melody-input"
    :class="{ 'is-wide': wide }"
    :type="type"
    :value="modelValue"
    :placeholder="placeholder"
    :disabled="disabled"
    @input="$emit('update:modelValue', ($event.target as HTMLInputElement).value)"
  />
</template>

<script setup lang="ts">
withDefaults(defineProps<{
  modelValue?: string | number;
  type?: string;
  placeholder?: string;
  disabled?: boolean;
  wide?: boolean;
}>(), { type: 'text' });

defineEmits<{ 'update:modelValue': [value: string] }>();
</script>
