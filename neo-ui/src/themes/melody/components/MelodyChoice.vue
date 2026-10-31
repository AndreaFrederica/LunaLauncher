<template>
  <label class="melody-choice" :class="{ 'is-disabled': disabled }">
    <input :type="type" :name="name" :value="value" :checked="checked" :disabled="disabled" @change="$emit('update:checked', ($event.target as HTMLInputElement).checked)" />
    <span><slot /></span>
  </label>
</template>
<script setup lang="ts">
withDefaults(defineProps<{ type?: 'radio' | 'checkbox'; name?: string; value?: string; checked?: boolean; disabled?: boolean }>(), { type: 'checkbox', checked: false });
defineEmits<{ 'update:checked': [value: boolean] }>();
</script>
