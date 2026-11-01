<template>
  <div class="ef-record" :class="{ selected }" role="button" tabindex="0" @click="onClick">
    <slot name="tag">
      <span v-if="tag" class="ef-tag" :class="{ 'is-current': selected }">{{ tag }}</span>
    </slot>
    <span v-if="date" class="ef-field">{{ date }}</span>
    <span class="name"><slot name="title">{{ title }}</slot><br v-if="info" /><small v-if="info" class="ef-field">{{ info }}</small></span>
    <span v-if="count !== undefined" class="ef-field">{{ count }}</span>
    <span v-if="$slots.actions" class="ef-actions"><slot name="actions" /></span>
  </div>
</template>

<script setup lang="ts">
const props = withDefaults(defineProps<{ title?: string; info?: string; date?: string; count?: number | string; tag?: string; selected?: boolean; disabled?: boolean }>(), { title: '', info: '', tag: '', selected: false, disabled: false });
const emit = defineEmits<{ click: [event: MouseEvent | KeyboardEvent] }>();
function onClick(e: MouseEvent | KeyboardEvent) { if (props.disabled) return; emit('click', e); }
</script>

<style scoped>
.ef-record.selected { background: var(--ef-chip); }
.ef-record.selected::before { transform: none; }
</style>
