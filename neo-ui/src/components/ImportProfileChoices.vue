<template>
  <fieldset class="import-profiles">
    <legend>{{ t(ready ? 'importProfiles.choose' : 'importProfiles.noneReady') }}</legend>
    <label v-for="(choice, index) in choices" :key="choice.id" class="import-profiles__row"
      :class="{ 'is-selected': modelValue === index, 'is-invalid': choice.disabled }">
      <input type="radio" :name="group" :value="index" :checked="modelValue === index"
        :disabled="disabled || choice.disabled" :aria-describedby="group + '-' + index"
        @change="emit('update:modelValue', index)" />
      <span class="import-profiles__copy">
        <span class="import-profiles__name">{{ choice.name }}</span>
        <small v-if="choice.details">{{ choice.details }}</small>
        <small class="import-profiles__path">{{ choice.path }}</small>
        <span :id="group + '-' + index" class="import-profiles__error" v-if="choice.disabled">{{ choice.error }}</span>
        <span :id="group + '-' + index" class="import-profiles__ready" v-else>{{ t('importProfiles.ready') }}</span>
      </span>
    </label>
  </fieldset>
</template>

<script setup lang="ts">
import { computed, useId } from 'vue';
import { useI18n } from 'vue-i18n';

type ImportProfile = {
  id: string;
  name: string;
  path: string;
  details?: string;
  error?: string;
  disabled: boolean;
};
const props = defineProps<{ choices: ImportProfile[]; modelValue: string | number; disabled?: boolean }>();
const emit = defineEmits<{ 'update:modelValue': [value: number] }>();
const { t } = useI18n();
const group = useId();
const ready = computed(() => props.choices.some(choice => !choice.disabled));
</script>

<style scoped>
.import-profiles {
  display: grid;
  gap: 6px;
  min-inline-size: 0;
  margin: 0;
  padding: 0;
  border: 0;
}
.import-profiles legend { margin-bottom: 10px; font-size: .9em; }
.import-profiles__row {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr);
  gap: 10px;
  align-items: start;
  padding: 10px 12px;
  border: 1px solid var(--ef-rule, var(--pcl-border, var(--neo-border, #526075)));
  cursor: pointer;
}
.import-profiles__row:has(:focus-visible) { outline: 2px solid currentColor; outline-offset: 2px; }
.import-profiles__row.is-selected {
  border-color: var(--ef-signal, var(--pcl-c3, var(--q-primary)));
  background: color-mix(in srgb, var(--ef-signal, var(--pcl-c3, var(--q-primary))) 12%, transparent);
}
.import-profiles__row.is-invalid { cursor: default; }
.import-profiles__row input { margin: .3em 0 0; accent-color: var(--ef-signal, var(--pcl-c3, var(--q-primary))); }
.import-profiles__copy { display: grid; gap: 2px; min-width: 0; overflow-wrap: anywhere; }
.import-profiles__name { font-weight: 650; }
.import-profiles__path { font-family: ui-monospace, monospace; opacity: .8; }
.import-profiles__error, .import-profiles__ready { font-size: .86em; }
.import-profiles__error { color: var(--q-negative); }
</style>
