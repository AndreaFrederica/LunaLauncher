<template>
  <section class="cache-browser">
    <label class="cache-browser__policy"><input type="checkbox" :checked="artifactsEnabled" :disabled="policyBusy" @change="setPolicy(($event.target as HTMLInputElement).checked)" />保留下载文件副本（模组 JAR / 整合包，默认关闭）</label>
    <p class="cache-browser__help">关闭时使用临时下载文件，退出时清理；游戏所需的依赖、实例内模组和已安装 Java 正常保留。已有下载副本需手动清理。</p>
    <p>已有下载副本：{{ artifactCount ?? '—' }} 条 / {{ formatCacheSize(artifactBytes) }}</p>
    <p v-if="policyError" role="alert">{{ policyError }}</p>
    <button type="button" class="cache-browser__toggle" :aria-expanded="opened" @click="open">{{ opened ? '收起缓存明细' : '查看缓存了什么 / 删除缓存' }}</button>
    <div v-if="opened">
      <div class="cache-browser__actions">
        <button v-for="tab in tabs" :key="tab.id" type="button" :aria-pressed="kind === tab.id" :disabled="busy" @click="select(tab.id)">{{ tab.label }}</button>
        <button type="button" :disabled="busy" @click="load(false)">刷新明细</button>
        <button type="button" :disabled="busy" @click="pending = '*'">清空当前类型</button>
      </div>
      <p class="cache-browser__help">元数据和图片不包含 JAR；下载文件副本单独管理。游戏依赖不属于可清理副本。旧资源条目未记录来源时显示路由和标识。</p>
      <label class="cache-browser__search">筛选已加载的条目<input v-model="search" type="search" placeholder="输入文件名、来源或分类" /></label>
      <p v-if="error" role="alert">{{ error }}</p>
      <p v-if="notice" role="status">{{ notice }}</p>
      <p v-if="busy" role="status">正在处理缓存…</p>
      <div v-if="pending === '*'" class="cache-browser__confirm">
        清空{{ tabs.find(tab => tab.id === kind)?.label }}？
        <button type="button" :disabled="busy" @click="remove()">确认清空</button>
        <button type="button" :disabled="busy" @click="pending = ''">取消</button>
      </div>
      <div class="cache-browser__list" aria-label="缓存文件明细" :aria-busy="busy">
        <article v-for="entry in filtered" :key="entry.id" class="cache-browser__entry">
          <div class="cache-browser__info">
            <strong>{{ entry.name }}</strong>
            <span>{{ entry.category }} · {{ formatCacheSize(entry.bytes) }} · {{ entry.time || '时间未记录' }}</span>
            <code>{{ entry.location }}</code>
            <small v-if="entry.missing">文件已不存在</small>
          </div>
          <div class="cache-browser__row-actions">
            <template v-if="pending === entry.id">
              <span>删除此项？</span>
              <button type="button" :disabled="busy" @click="remove(entry)">确认删除</button>
              <button type="button" :disabled="busy" @click="pending = ''">取消</button>
            </template>
            <button v-else type="button" :disabled="busy" @click="pending = entry.id">删除</button>
          </div>
        </article>
        <p v-if="!busy && !filtered.length">{{ search ? '已加载的条目中没有匹配项。' : '当前没有缓存条目。' }}</p>
      </div>
      <p>已加载 {{ entries.length }} 条，当前显示 {{ filtered.length }} 条。</p>
      <button v-if="cursor" type="button" :disabled="busy" @click="load(true)">加载更多</button>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue';
import { formatCacheSize } from '../common/cacheStatus';
import { formatError } from '../common/formatError';
type Kind = 'http' | 'artifacts' | 'resources';
type Entry = { id: string; name: string; category: string; bytes: number; time: string; location: string; base?: string; path?: string; key?: string; missing?: boolean };
const props = defineProps<{ execute: (operation: string, parameters?: any) => Promise<any>; version?: string }>();
const emit = defineEmits<{ changed: [] }>();
const tabs: { id: Kind; label: string }[] = [{ id: 'http', label: '元数据和图片' }, { id: 'artifacts', label: '下载文件副本（JAR / 整合包）' }, { id: 'resources', label: '资源缓存（版本 / 纹理）' }];
const opened = ref(false), busy = ref(false), kind = ref<Kind>('http'), entries = ref<Entry[]>([]), cursor = ref('');
const search = ref(''), error = ref(''), notice = ref(''), pending = ref('');
const artifactsEnabled = ref(false), artifactCount = ref<number>(), artifactBytes = ref<number>(), policyBusy = ref(true), policyError = ref('');
async function loadPolicy() {
  policyBusy.value = true; policyError.value = '';
  try { const state = await props.execute('launcher.cache.status'); artifactsEnabled.value = state.artifactsEnabled === true; artifactCount.value = state.artifacts?.entries; artifactBytes.value = state.artifacts?.bytes; }
  catch (e) { policyError.value = formatError(e); }
  finally { policyBusy.value = false; }
}
async function setPolicy(enabled: boolean) {
  policyBusy.value = true; policyError.value = '';
  try { await props.execute('settings.set', { scope: 'launcher', key: 'CacheDownloadArtifacts', value: enabled }); artifactsEnabled.value = enabled; emit('changed'); }
  catch (e) { policyError.value = formatError(e); }
  finally { policyBusy.value = false; }
}
onMounted(loadPolicy);
watch(() => props.version, () => { void loadPolicy(); if (opened.value && !busy.value) void load(false); });

const filtered = computed(() => entries.value.filter(e => `${e.name} ${e.category} ${e.location}`.toLowerCase().includes(search.value.toLowerCase())));
function date(value: string | number) { if (!value) return ''; const d = new Date(typeof value === 'number' ? value * 1000 : value); return Number.isNaN(d.getTime()) ? '' : d.toLocaleString(); }
async function open() { opened.value = !opened.value; if (opened.value) await load(false); }
async function select(value: Kind) { kind.value = value; search.value = ''; notice.value = ''; pending.value = ''; await load(false); }
async function load(append: boolean) {
  busy.value = true; error.value = '';
  if (!append) { entries.value = []; cursor.value = ''; }
  try {
    if (kind.value !== 'resources') {
      const result = await props.execute('launcher.cache.entries', { kind: kind.value === 'http' ? 'metadata' : 'artifacts' });
      entries.value = result.map((e: any) => ({ id: JSON.stringify([e.base, e.path]), name: e.path, category: e.base, bytes: e.bytes, time: date(e.modified), location: e.filePath, base: e.base, path: e.path, missing: !e.exists }));
    } else {
      const result = await props.execute('yggdrasil.cache.entries', { cursor: append ? cursor.value : '' });
      const next = result.items.map((e: any) => ({ id: e.key, key: e.key, name: e.source || `旧缓存 · ${e.key}`, category: e.routeId, bytes: e.bytes, time: date(e.stored), location: e.source || `未记录来源 · ${e.key}` }));
      entries.value = append ? [...entries.value, ...next] : next;
      cursor.value = result.next_cursor || '';
    }
  } catch (e) { error.value = formatError(e); }
  finally { busy.value = false; }
}
async function remove(entry?: Entry) {
  busy.value = true; error.value = ''; notice.value = '';
  try {
    if (kind.value !== 'resources') await props.execute(entry ? 'launcher.cache.remove' : 'launcher.cache.clear', { confirm: true, ...(entry ? { base: entry.base, path: entry.path } : { kind: kind.value === 'http' ? 'metadata' : 'artifacts' }) });
    else await props.execute('yggdrasil.cache.purge', { confirm: true, ...(entry ? { key: entry.key } : {}) });
    pending.value = ''; notice.value = entry ? '已删除此缓存。' : '已清空当前类型的缓存。';
    emit('changed');
    await load(false);
    await loadPolicy();
  } catch (e) { error.value = formatError(e); }
  finally { busy.value = false; }
}
</script>

<style scoped>
.cache-browser { margin-top: 16px; min-width: 0; color: inherit; }
.cache-browser button { color: inherit; background: transparent; border: 1px solid currentColor; border-radius: 4px; padding: 6px 10px; cursor: pointer; }
.cache-browser button:disabled { opacity: .5; cursor: wait; }
.cache-browser button[aria-pressed="true"] { font-weight: 700; background: color-mix(in srgb, currentColor 12%, transparent); }
.cache-browser__toggle { font-weight: 700; }
.cache-browser__actions, .cache-browser__confirm, .cache-browser__row-actions { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; margin: 10px 0; }
.cache-browser__search { display: flex; flex-wrap: wrap; align-items: center; gap: 10px; margin: 12px 0; }
.cache-browser input { min-width: 0; width: min(320px, 100%); color: inherit; background: transparent; border: 1px solid currentColor; border-radius: 4px; padding: 7px; }
.cache-browser__policy { display: flex; align-items: center; gap: 8px; margin-block: 8px; }
.cache-browser__policy input { width: auto; }
.cache-browser__info { user-select: text; }
.cache-browser__help { font-size: 12px; opacity: .8; }
.cache-browser__list { max-height: 420px; overflow: auto; overscroll-behavior: contain; border-block: 1px solid color-mix(in srgb, currentColor 25%, transparent); }
.cache-browser__entry { display: flex; flex-wrap: wrap; align-items: center; gap: 12px; padding: 12px 4px; border-bottom: 1px solid color-mix(in srgb, currentColor 15%, transparent); }
.cache-browser__info { flex: 1 1 260px; min-width: 0; display: grid; gap: 5px; overflow-wrap: anywhere; }
.cache-browser__info span, .cache-browser__info code { font-size: 12px; opacity: .8; white-space: normal; }
</style>
