<template>
  <textarea
    v-if="multiline"
    class="pcl-input"
    :value="String(modelValue ?? '')"
    :placeholder="placeholder"
    :disabled="disabled"
    :rows="rows"
    :spellcheck="false"
    @input="emit('update:modelValue', ($event.target as HTMLTextAreaElement).value)"
    @blur="emit('blur')"
  />
  <input
    v-else
    class="pcl-input"
    :type="type"
    :value="modelValue"
    :placeholder="placeholder"
    :disabled="disabled"
    :min="min"
    :max="max"
    :step="step"
    :spellcheck="false"
    @input="onInput"
    @blur="emit('blur')"
    @keydown.enter="emit('enter')"
  />
</template>

<script setup lang="ts">
const props = withDefaults(defineProps<{
  modelValue?: string | number;
  type?: string;
  placeholder?: string;
  disabled?: boolean;
  multiline?: boolean;
  rows?: number;
  min?: string | number;
  max?: string | number;
  step?: string | number;
}>(), {
  modelValue: '',
  type: 'text',
  placeholder: '',
  disabled: false,
  multiline: false,
  rows: 4,
  min: undefined,
  max: undefined,
  step: undefined,
});

const emit = defineEmits<{
  'update:modelValue': [value: string | number];
  enter: [];
  blur: [];
}>();

function onInput(event: Event) {
  const input = event.target as HTMLInputElement;
  emit('update:modelValue', props.type === 'number' ? (input.value === '' ? '' : Number(input.value)) : input.value);
}
</script>
