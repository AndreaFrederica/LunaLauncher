<template>
  <q-dialog :model-value="modelValue" persistent @update:model-value="emit('update:modelValue', $event)">
    <q-card class="ef-dialog">
      <q-card-section v-if="title || $slots.title">
        <h3 class="ef-dialog__title">{{ title }}<slot name="title" /></h3>
      </q-card-section>
      <q-card-section>
        <slot />
        <EfProgress v-if="progress !== null" :value="progress" :show-value="true" />
        <p v-if="stage" class="ef-dialog__stage">{{ stage }}</p>
      </q-card-section>
      <q-card-actions align="right">
        <slot name="actions" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import EfProgress from './EfProgress.vue';
const props = withDefaults(defineProps<{ modelValue?: boolean; title?: string; progress?: number | null; stage?: string }>(), { modelValue: false, title: '', progress: null, stage: '' });
const emit = defineEmits<{ 'update:modelValue': [value: boolean] }>();
</script>

<style scoped>
.ef-dialog { border: 1px solid var(--ef-ink); border-radius: 0; background: var(--ef-panel); color: var(--ef-ink); box-shadow: none; min-width: 320px; }
.ef-dialog__title { margin: 0; font-family: var(--ef-font-ui); font-size: 12px; font-stretch: 125%; font-weight: 700; letter-spacing: .14em; text-transform: uppercase; }
.ef-dialog__title::before { content: '// '; color: var(--ef-ink-2); }
.ef-dialog__stage { margin-top: 8px; color: var(--ef-ink-2); font-size: 12px; }
</style>
