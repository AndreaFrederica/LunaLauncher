<template>
  <label class="pcl-radio" :class="{ disabled }">
    <input
      type="radio"
      :name="name"
      :value="value"
      :checked="modelValue === value"
      :disabled="disabled"
      @change="emit('update:modelValue', value)"
    />
    <span class="pcl-radio__box" aria-hidden="true" />
    <span v-if="label || $slots.default" class="pcl-radio__label"><slot>{{ label }}</slot></span>
  </label>
</template>

<script setup lang="ts">
withDefaults(defineProps<{
  modelValue?: string | number | boolean;
  value: string | number | boolean;
  name?: string;
  label?: string;
  disabled?: boolean;
}>(), {
  modelValue: '',
  name: '',
  label: '',
  disabled: false,
});

const emit = defineEmits<{ 'update:modelValue': [value: string | number | boolean] }>();
</script>
