<template>
  <label class="pcl-check" :class="{ disabled }">
    <input type="checkbox" :checked="Boolean(modelValue)" :disabled="disabled" @change="emit('update:modelValue', ($event.target as HTMLInputElement).checked)" />
    <span class="pcl-check__box" aria-hidden="true" />
    <span v-if="label || $slots.default" class="pcl-check__label"><slot>{{ label }}</slot></span>
  </label>
</template>

<script setup lang="ts">
withDefaults(defineProps<{
  modelValue?: boolean;
  label?: string;
  disabled?: boolean;
}>(), {
  modelValue: false,
  label: '',
  disabled: false,
});

const emit = defineEmits<{ 'update:modelValue': [value: boolean] }>();
</script>
