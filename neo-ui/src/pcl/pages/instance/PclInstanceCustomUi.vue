<template>
  <div class="pcl-custom-ui">
    <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

    <PclCard :title="t('pcl.customUi.title')">
      <template #actions>
        <PclButton size="sm" icon="refresh-cw" :disabled="busy || loading" @click="reload">{{ t('common.refresh') }}</PclButton>
        <PclButton size="sm" variant="ghost" icon="save" :disabled="busy || loading" @click="save">{{ t('pcl.customUi.saveState') }}</PclButton>
      </template>

      <div v-if="tabs.length" class="pcl-custom-tabs">
        <button v-for="(tab, index) in tabs" :key="tab.source || index" type="button" :class="{ active: activeTab === index }" @click="activeTab = index">
          {{ tab.resolvedTitle || tab.title || tab.name || t('pcl.customUi.panel', { index: index + 1 }) }}
        </button>
      </div>

      <div v-if="currentTab" class="pcl-custom-controls">
        <div v-for="control in currentTab.controls || []" :key="control.id || control.label" class="pcl-custom-control">
          <div class="pcl-custom-control__copy">
            <strong>{{ control.resolvedLabel || control.label || control.text || control.id }}</strong>
            <span v-if="control.resolvedDescription || control.description">{{ control.resolvedDescription || control.description }}</span>
          </div>
          <div class="pcl-custom-control__value">
            <PclCheckbox
              v-if="isToggle(control)"
              :model-value="Boolean(control.value)"
              :label="control.resolvedText || control.text || ''"
              :disabled="busy || loading"
              @update:model-value="value => trigger(control, Boolean(value))"
            />
            <PclSelect
              v-else-if="isSelect(control)"
              :model-value="control.value"
              :options="optionsOf(control)"
              :disabled="busy || loading"
              @update:model-value="value => trigger(control, value)"
            />
            <PclInput
              v-else-if="isText(control)"
              v-model="drafts[control.id]"
              :multiline="isMultiline(control)"
              :rows="3"
              :disabled="busy || loading"
              @blur="commitText(control)"
              @enter="commitText(control)"
            />
            <PclButton
              v-else-if="isButton(control)"
              size="sm"
              :disabled="busy || loading"
              @click="trigger(control, true)"
            >{{ control.resolvedText || control.text || control.label || t('pcl.customUi.execute') }}</PclButton>
            <span v-else class="pcl-custom-control__plain">{{ control.value ?? control.resolvedText ?? '' }}</span>
          </div>
        </div>
      </div>

      <div v-else class="pcl-empty">
        <PclIcon name="layout-list" :size="28" />
        <strong>{{ t('pcl.customUi.empty') }}</strong>
        <span>{{ t('pcl.customUi.emptyHint') }}</span>
      </div>

      <PclHint v-if="errors.length" theme="yellow">
        <span v-for="item in errors" :key="item">{{ item }}</span>
      </PclHint>
      <PclHint theme="blue">{{ t('pcl.customUi.hint') }}</PclHint>
    </PclCard>
  </div>
</template>

<script setup lang="ts">
import { useI18n } from 'vue-i18n';
const { t } = useI18n();
import { computed, onMounted, reactive, ref } from 'vue';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclInput from '../../components/PclInput.vue';
import PclSelect from '../../components/PclSelect.vue';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ instance: any; busy: boolean; execute: Execute }>();

const tabs = ref<any[]>([]);
const activeTab = ref(0);
const errors = ref<string[]>([]);
const loading = ref(false);
const error = ref('');
const drafts = reactive<Record<string, string>>({});
const currentTab = computed(() => tabs.value[activeTab.value] || null);

function message(value: unknown) {
  return value instanceof Error ? value.message : String(value);
}

function isToggle(control: any) {
  return ['toggle', 'checkbox'].includes(String(control.type || '').toLowerCase());
}
function isSelect(control: any) {
  return ['select', 'combo'].includes(String(control.type || '').toLowerCase());
}
function isMultiline(control: any) {
  return ['textarea', 'multiline'].includes(String(control.type || '').toLowerCase());
}
function isText(control: any) {
  return ['input', 'lineedit', 'textarea', 'multiline', 'number', 'spinbox', 'accountselect', 'account-select'].includes(String(control.type || '').toLowerCase());
}
function isButton(control: any) {
  return ['button', 'action'].includes(String(control.type || '').toLowerCase());
}
function optionsOf(control: any) {
  const options = Array.isArray(control.options) ? control.options : [];
  return options.map((option: any) => {
    if (typeof option === 'string' || typeof option === 'number') return { label: String(option), value: option };
    return { label: String(option.label || option.text || option.value || ''), value: option.value ?? option.label };
  });
}

function applySnapshot(snapshot: any) {
  tabs.value = Array.isArray(snapshot?.tabs) ? snapshot.tabs : [];
  errors.value = Array.isArray(snapshot?.errors) ? snapshot.errors.map(String) : [];
  if (activeTab.value >= tabs.value.length) activeTab.value = Math.max(0, tabs.value.length - 1);
  for (const tab of tabs.value) {
    for (const control of tab.controls || []) {
      if (control.id) drafts[control.id] = String(control.value ?? '');
    }
  }
}

async function load() {
  loading.value = true;
  error.value = '';
  try {
    applySnapshot(await props.execute('instance.custom-ui.snapshot', { instance: props.instance.id }));
  } catch (exception) {
    error.value = message(exception);
  } finally {
    loading.value = false;
  }
}

async function trigger(control: any, value: any) {
  loading.value = true;
  error.value = '';
  try {
    applySnapshot(await props.execute('instance.custom-ui.trigger', { instance: props.instance.id, control, value }));
  } catch (exception) {
    error.value = message(exception);
  } finally {
    loading.value = false;
  }
}

function commitText(control: any) {
  const value = drafts[control.id] ?? '';
  if (String(control.value ?? '') !== value) void trigger(control, value);
}

async function save() {
  loading.value = true;
  try { applySnapshot(await props.execute('instance.custom-ui.save', { instance: props.instance.id })); }
  catch (exception) { error.value = message(exception); }
  finally { loading.value = false; }
}

async function reload() {
  loading.value = true;
  try { applySnapshot(await props.execute('instance.custom-ui.reload', { instance: props.instance.id })); }
  catch (exception) { error.value = message(exception); }
  finally { loading.value = false; }
}

onMounted(() => { void load(); });
</script>

<style scoped>
.pcl-custom-tabs {
  display: flex;
  flex-wrap: wrap;
  gap: 2px;
  margin-bottom: 12px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-custom-tabs button {
  height: 30px;
  padding: 0 12px;
  border: 0;
  border-bottom: 2px solid transparent;
  background: transparent;
  color: var(--pcl-muted);
  font: 400 calc(12px * var(--pcl-font-scale, 1))/1 var(--pcl-font);
  cursor: pointer;
}

.pcl-custom-tabs button.active {
  border-bottom-color: var(--pcl-c3);
  color: var(--pcl-c2);
}

.pcl-custom-controls {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.pcl-custom-control {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  min-height: 42px;
  padding: 7px 0;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-custom-control__copy {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
}

.pcl-custom-control__copy strong {
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-custom-control__copy span {
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-custom-control__value {
  flex: 0 0 45%;
  min-width: 160px;
}

.pcl-custom-control__plain {
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 6px;
  padding: 36px 16px;
  border: 1px dashed var(--pcl-card-border);
  border-radius: 4px;
  color: var(--pcl-muted);
  text-align: center;
}

.pcl-empty strong {
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
}

.pcl-empty span {
  font-size: calc(11px * var(--pcl-font-scale, 1));
  line-height: 1.7;
}
</style>
