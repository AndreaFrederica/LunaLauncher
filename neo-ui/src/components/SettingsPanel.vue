<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="settings-panel">
    <div class="settings-toolbar">
      <q-input v-model="search" dark outlined dense clearable class="settings-search" :label="t('settings.search')" />
      <q-select :model-value="activeCategory" dark outlined dense emit-value map-options :options="categoryOptions" :label="t('settings.category')" class="settings-category" @update:model-value="selectCategory" />
    </div>
    <q-banner v-if="activeCategory === 'qt-ui'" class="bg-blue-grey-9 q-mb-md" rounded>{{ t('settings.qtHint') }}</q-banner>
    <div class="settings-summary text-caption text-grey-5 q-mb-md">{{ scope === 'instance' ? t('settings.instance') : t('settings.launcher') }} · {{ t('settings.apiCount', { count: visible.length }) }}</div>
    <q-card v-for="setting in visible" :key="setting.key" class="setting-row q-mb-sm">
      <q-card-section>
        <div class="setting-heading">
          <div class="min-width-0"><div class="setting-title">{{ metadata(setting).label }}</div><div class="setting-key">{{ setting.key }} · {{ typeLabel(setting) }} · {{ setting.isDefault ? t('settings.default') : t('settings.modified') }}</div></div>
          <q-btn flat dense icon="restart_alt" :label="t('settings.reset')" :disable="busy" @click="emit('reset', setting.key)" />
        </div>
        <q-toggle v-if="kind(setting) === 'boolean'" dark v-model="draft[setting.key]" :disable="busy" :label="draft[setting.key] ? t('settings.enabled') : t('settings.off')" />
        <q-input v-else-if="kind(setting) === 'number'" dark outlined dense type="number" v-model.number="draft[setting.key]" :disable="busy" class="q-mt-sm" />
        <q-input v-else dark outlined autogrow v-model="draft[setting.key]" :disable="busy" :type="setting.redacted ? 'password' : isStructured(setting) ? 'textarea' : 'text'" :placeholder="setting.redacted ? t('settings.redactedHint') : undefined" class="q-mt-sm" />
        <div class="row justify-end q-gutter-sm q-mt-sm"><q-chip v-if="setting.surface === 'qt'" dense color="blue-grey-8" text-color="grey-3" icon="desktop_windows">{{ t('settings.qtLegacy') }}</q-chip><q-btn unelevated color="primary" :label="t('common.save')" :disable="busy || (setting.redacted && !String(draft[setting.key] || '').trim())" @click="save(setting)" /></div>
      </q-card-section>
    </q-card>
    <p v-if="!visible.length" class="text-grey-5">{{ t('settings.noMatch') }}</p>
    <q-banner v-if="validation" class="bg-red-9 q-mt-md" rounded>{{ validation }}</q-banner>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { settingMetadata } from '../settings-schema';
import { settingCategoryOrder } from '../settings-categories';

type ApiSetting = { key: string; type?: string; value: unknown; default?: unknown; isDefault?: boolean; redacted?: boolean; surface?: string; category?: string };
const props = defineProps<{ settings: ApiSetting[]; busy: boolean; scope?: string; category?: string }>();
const { t } = useI18n();
const emit = defineEmits<{ save: [key: string, value: unknown]; reset: [key: string]; 'update:category': [category: string] }>();
const search = ref(''), category = ref('all'), validation = ref('');
const draft = ref<Record<string, any>>({});
const legacyGroups: Record<string, string> = { Proxy: 'network', Launcher: 'qt-ui', ExternalTools: 'advanced', API: 'advanced' };
function metadata(setting: ApiSetting) { const fallback = settingMetadata[setting.key]; const group = setting.category || legacyGroups[fallback?.group || ''] || fallback?.group || 'advanced'; return { label: fallback?.label ? t(fallback.label) : setting.key, group }; }
function categoryOf(setting: ApiSetting) { return setting.surface === 'qt' ? 'qt-ui' : setting.category || metadata(setting).group; }
function kind(setting: ApiSetting): 'boolean' | 'number' | 'structured' | 'string' { if (typeof setting.value === 'boolean') return 'boolean'; if (typeof setting.value === 'number') return 'number'; if (Array.isArray(setting.value) || (setting.value !== null && typeof setting.value === 'object')) return 'structured'; if ((setting.type || '').includes('bool')) return 'boolean'; if ((setting.type || '').match(/int|double|float/)) return 'number'; return 'string'; }
function isStructured(setting: ApiSetting) { return kind(setting) === 'structured'; }
function typeLabel(setting: ApiSetting) { return ({ boolean: t('settings.types.boolean'), number: t('settings.types.number'), structured: 'JSON', string: t('settings.types.string') }[kind(setting)]); }
const activeCategory = computed(() => props.category ?? category.value);
function selectCategory(value: string) { if (props.category === undefined) category.value = value; emit('update:category', value); }
const categoryOptions = computed(() => { const present = new Set(props.settings.map(categoryOf)); return [{ label: t('settings.categories.all'), value: 'all' }, ...settingCategoryOrder.filter(id => present.has(id)).map(id => ({ label: t(`settings.categories.${id}`), value: id }))]; });
const visible = computed(() => { const query = search.value.trim().toLowerCase(); return props.settings.filter(setting => (activeCategory.value === 'all' || categoryOf(setting) === activeCategory.value) && (!query || `${setting.key} ${metadata(setting).label}`.toLowerCase().includes(query))); });
watch(() => props.settings, items => { draft.value = Object.fromEntries(items.map(setting => [setting.key, setting.redacted ? '' : isStructured(setting) ? JSON.stringify(setting.value, null, 2) : setting.value])); }, { immediate: true });
function save(setting: ApiSetting) { validation.value = ''; let value: unknown = draft.value[setting.key]; try { if (isStructured(setting)) value = JSON.parse(String(value || '')); if (kind(setting) === 'number' && !Number.isFinite(Number(value))) throw new Error(t('neo.auto2.k7db3bc0d41')); emit('save', setting.key, value); } catch (error) { validation.value = error instanceof Error ? error.message : String(error); } }
</script>

<style scoped>
.settings-panel { max-width: 1040px; }
.settings-toolbar { display: flex; gap: 12px; margin-bottom: 18px; }
.settings-search { flex: 1; }
.settings-category { width: 240px; }
.setting-row { background: var(--neo-panel); border: 1px solid var(--neo-border); }
.setting-heading { display: flex; align-items: center; justify-content: space-between; gap: 12px; }
.setting-title { color: #f8fafc; font-weight: 650; }
.setting-key { color: rgba(255,255,255,.48); font-size: 12px; margin-top: 4px; overflow-wrap: anywhere; }
.min-width-0 { min-width: 0; }
@media (max-width: 700px) { .settings-toolbar { display: block; } .settings-category { width: 100%; margin-top: 10px; } }
</style>
