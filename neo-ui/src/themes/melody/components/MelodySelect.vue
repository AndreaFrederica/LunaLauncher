<template>
  <label class="melody-select" :class="[$attrs.class, { 'is-disabled': disabled }]">
    <span v-if="label">{{ label }}</span>
    <select v-bind="controlAttrs" :value="modelValue" :disabled="disabled" @change="$emit('update:modelValue', ($event.target as HTMLSelectElement).value)">
      <option v-if="!modelValue || placeholder" value="">{{ placeholder || '' }}</option>
      <option v-for="option in options" :key="option.value" :value="option.value" :disabled="option.disabled">{{ option.label }}</option>
    </select>
  </label>
</template>
<script setup lang="ts">
import { computed, useAttrs } from 'vue';
import type { MelodyOption } from '../types';
defineOptions({ inheritAttrs: false });
const attrs = useAttrs();
const controlAttrs = computed(() => { const { class: _class, ...rest } = attrs; return rest; });
withDefaults(defineProps<{ modelValue?: string; options: MelodyOption[]; label?: string; placeholder?: string; disabled?: boolean }>(), { modelValue: '' });
defineEmits<{ 'update:modelValue': [value: string] }>();
</script>
