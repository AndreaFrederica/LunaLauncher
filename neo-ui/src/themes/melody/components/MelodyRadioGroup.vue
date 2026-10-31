<template>
  <div class="melody-radio-group" :class="{ 'is-vertical': vertical }" role="radiogroup" :aria-label="label">
    <MelodyChoice v-for="option in options" :key="option.value" type="radio" :name="groupName" :value="option.value"
      :checked="modelValue === option.value" :disabled="disabled || option.disabled" @update:checked="checked => checked && $emit('update:modelValue', option.value)">{{ option.label }}</MelodyChoice>
  </div>
</template>
<script setup lang="ts">
import { useId } from 'vue';
import MelodyChoice from './MelodyChoice.vue';
import type { MelodyOption } from '../types';
defineProps<{ modelValue: string; options: MelodyOption[]; label: string; vertical?: boolean; disabled?: boolean }>();
defineEmits<{ 'update:modelValue': [value: string] }>();
const groupName = `melody-radio-${useId()}`;
</script>
