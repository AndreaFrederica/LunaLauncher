<template>
  <div class="settings-panel">
    <div class="settings-toolbar">
      <q-input v-model="search" dark outlined dense clearable class="settings-search" label="搜索设置" />
      <q-select v-model="category" dark outlined dense emit-value map-options :options="categoryOptions" label="设置分类" class="settings-category" />
    </div>
    <q-banner v-if="category === 'qt-ui'" class="bg-blue-grey-9 q-mb-md" rounded>这些选项只影响 Qt 旧界面的布局、工具栏或外部集成，对 Neo UI 本身不生效。</q-banner>
    <div class="settings-summary text-caption text-grey-5 q-mb-md">{{ scope === 'instance' ? '客户端设置' : '启动器设置' }} · {{ visible.length }} 项由 API 提供</div>
    <q-card v-for="setting in visible" :key="setting.key" class="setting-row q-mb-sm">
      <q-card-section>
        <div class="setting-heading">
          <div class="min-width-0"><div class="setting-title">{{ metadata(setting).label }}</div><div class="setting-key">{{ setting.key }} · {{ typeLabel(setting) }} · {{ setting.isDefault ? '默认值' : '已修改' }}</div></div>
          <q-btn flat dense icon="restart_alt" label="恢复默认" :disable="busy" @click="emit('reset', setting.key)" />
        </div>
        <q-toggle v-if="kind(setting) === 'boolean'" dark v-model="draft[setting.key]" :disable="busy" :label="draft[setting.key] ? '启用' : '关闭'" />
        <q-input v-else-if="kind(setting) === 'number'" dark outlined dense type="number" v-model.number="draft[setting.key]" :disable="busy" class="q-mt-sm" />
        <q-input v-else dark outlined autogrow v-model="draft[setting.key]" :disable="busy" :type="setting.redacted ? 'password' : isStructured(setting) ? 'textarea' : 'text'" :placeholder="setting.redacted ? '留空表示不修改现有值' : undefined" class="q-mt-sm" />
        <div class="row justify-end q-gutter-sm q-mt-sm"><q-chip v-if="setting.surface === 'qt'" dense color="blue-grey-8" text-color="grey-3" icon="desktop_windows">Qt 旧界面</q-chip><q-btn unelevated color="primary" label="保存" :disable="busy || (setting.redacted && !String(draft[setting.key] || '').trim())" @click="save(setting)" /></div>
      </q-card-section>
    </q-card>
    <p v-if="!visible.length" class="text-grey-5">没有符合条件的设置。</p>
    <q-banner v-if="validation" class="bg-red-9 q-mt-md" rounded>{{ validation }}</q-banner>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { settingMetadata } from '../settings-schema';

type ApiSetting = { key: string; type?: string; value: unknown; default?: unknown; isDefault?: boolean; redacted?: boolean; surface?: string; category?: string };
const props = defineProps<{ settings: ApiSetting[]; busy: boolean; scope?: string }>();
const emit = defineEmits<{ save: [key: string, value: unknown]; reset: [key: string] }>();
const search = ref(''), category = ref('all'), validation = ref('');
const draft = ref<Record<string, any>>({});
const categoryLabels: Record<string, string> = { all: '全部设置', launch: '启动与窗口', java: 'Java 与内存', paths: '目录与文件', network: '网络连接', downloads: '下载与资源', compatibility: '兼容性与性能', console: '控制台与日志', playtime: '游戏时间', instance: '客户端信息', 'instance-overrides': '客户端覆盖项', 'qt-ui': 'Qt 旧界面兼容', advanced: '高级设置' };
const categoryOrder = ['launch', 'java', 'paths', 'network', 'downloads', 'compatibility', 'console', 'playtime', 'instance', 'instance-overrides', 'qt-ui', 'advanced'];
const legacyGroups: Record<string, string> = { '窗口与启动': 'launch', '游戏与启动': 'launch', 'Java 与内存': 'java', '命令与环境': 'launch', '目录与文件': 'paths', '网络': 'network', 'Proxy': 'network', '下载与资源': 'downloads', '性能与兼容': 'compatibility', '控制台': 'console', '游戏时间': 'playtime', 'Launcher': 'qt-ui', 'ExternalTools': 'advanced', 'API': 'advanced' };
function metadata(setting: ApiSetting) { const fallback = settingMetadata[setting.key]; const group = setting.category || legacyGroups[fallback?.group || ''] || fallback?.group || 'advanced'; return { label: fallback?.label || setting.key, group }; }
function categoryOf(setting: ApiSetting) { return setting.surface === 'qt' ? 'qt-ui' : setting.category || metadata(setting).group; }
function kind(setting: ApiSetting): 'boolean' | 'number' | 'structured' | 'string' { if (typeof setting.value === 'boolean') return 'boolean'; if (typeof setting.value === 'number') return 'number'; if (Array.isArray(setting.value) || (setting.value !== null && typeof setting.value === 'object')) return 'structured'; if ((setting.type || '').includes('bool')) return 'boolean'; if ((setting.type || '').match(/int|double|float/)) return 'number'; return 'string'; }
function isStructured(setting: ApiSetting) { return kind(setting) === 'structured'; }
function typeLabel(setting: ApiSetting) { return ({ boolean: '开关', number: '数值', structured: 'JSON', string: '文本' }[kind(setting)]); }
const categoryOptions = computed(() => { const present = new Set(props.settings.map(categoryOf)); return [{ label: categoryLabels.all, value: 'all' }, ...categoryOrder.filter(id => present.has(id)).map(id => ({ label: categoryLabels[id], value: id }))]; });
const visible = computed(() => { const query = search.value.trim().toLowerCase(); return props.settings.filter(setting => (category.value === 'all' || categoryOf(setting) === category.value) && (!query || `${setting.key} ${metadata(setting).label}`.toLowerCase().includes(query))); });
watch(() => props.settings, items => { draft.value = Object.fromEntries(items.map(setting => [setting.key, setting.redacted ? '' : isStructured(setting) ? JSON.stringify(setting.value, null, 2) : setting.value])); }, { immediate: true });
function save(setting: ApiSetting) { validation.value = ''; let value: unknown = draft.value[setting.key]; try { if (isStructured(setting)) value = JSON.parse(String(value || '')); if (kind(setting) === 'number' && !Number.isFinite(Number(value))) throw new Error('请输入有效数字'); emit('save', setting.key, value); } catch (error) { validation.value = error instanceof Error ? error.message : String(error); } }
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
