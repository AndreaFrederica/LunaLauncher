<template>
  <main class="runtime-debugger">
    <header class="runtime-debugger__header">
      <div><strong>NeoUI Runtime Debugger</strong><small>事件、数据 patch 和组件生命周期</small></div>
      <div class="runtime-debugger__actions">
        <label><input v-model="store.paused" type="checkbox" /> 暂停记录</label>
        <select v-model="kind"><option value="">全部类型</option><option value="event">事件</option><option value="store">数据</option><option value="refresh">刷新</option><option value="lifecycle">生命周期</option><option value="timer">定时器</option><option value="error">错误</option></select>
        <button type="button" @click="clearRuntimeDebug">清空</button>
      </div>
    </header>
    <section class="runtime-debugger__summary">
      <span>记录 {{ filtered.length }} / {{ store.entries.length }}</span>
      <span>当前主题 {{ theme }}</span>
      <span>窗口 {{ windowLabel }}</span>
    </section>
    <div ref="list" class="runtime-debugger__list">
      <article v-for="entry in filtered" :key="entry.id" class="runtime-debugger__entry" :class="`is-${entry.kind}`">
        <time>{{ formatTime(entry.at) }}</time><b>{{ entry.kind }}</b><strong>{{ entry.source }}</strong><span>{{ entry.message }}</span>
        <pre v-if="entry.data !== undefined">{{ formatData(entry.data) }}</pre>
      </article>
      <p v-if="!filtered.length" class="runtime-debugger__empty">暂无记录。打开主界面并操作页面后，这里会显示真实运行时活动。</p>
    </div>
  </main>
</template>
<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import { neoUiPreferences } from '../../neo-ui-preferences';
import { clearRuntimeDebug, listenRuntimeDebug, useRuntimeDebugStore, type RuntimeDebugKind } from '../runtime/runtime-debug';
const store = useRuntimeDebugStore();
const kind = ref<RuntimeDebugKind | ''>('');
const list = ref<HTMLElement>();
const theme = computed(() => neoUiPreferences.shell);
const windowLabel = typeof window === 'undefined' ? 'main' : (window.location.search || 'main');
const filtered = computed(() => kind.value ? store.entries.filter(entry => entry.kind === kind.value) : store.entries);
function formatTime(value: number) { return new Date(value).toLocaleTimeString([], { hour12: false, fractionalSecondDigits: 3 }); }
function formatData(value: unknown) { try { return JSON.stringify(value, null, 2); } catch { return String(value); } }
let stop: (() => void) | undefined;
onMounted(async () => { stop = await listenRuntimeDebug(() => { if (!store.paused) requestAnimationFrame(() => { if (list.value) list.value.scrollTop = list.value.scrollHeight; }); }); });
onBeforeUnmount(() => stop?.());
</script>
<style scoped>
.runtime-debugger{height:100vh;display:flex;flex-direction:column;background:#11151d;color:#dbe5f1;font:13px/1.45 "Segoe UI","Microsoft YaHei",sans-serif}.runtime-debugger__header{display:flex;align-items:center;justify-content:space-between;gap:20px;padding:14px 18px;border-bottom:1px solid #2b3545;background:#1a2130}.runtime-debugger__header strong{display:block;font-size:16px}.runtime-debugger__header small{color:#94a3b8}.runtime-debugger__actions{display:flex;align-items:center;gap:10px}.runtime-debugger button,.runtime-debugger select{border:1px solid #40506a;border-radius:5px;background:#202b3d;color:#dbe5f1;padding:6px 9px}.runtime-debugger__summary{display:flex;gap:24px;padding:8px 18px;color:#94a3b8;border-bottom:1px solid #263143}.runtime-debugger__list{flex:1;overflow:auto;padding:8px 12px}.runtime-debugger__entry{display:grid;grid-template-columns:95px 70px 190px minmax(0,1fr);gap:8px;align-items:start;padding:8px 6px;border-bottom:1px solid #202a39}.runtime-debugger__entry time{color:#8492a6;font-family:Consolas,monospace}.runtime-debugger__entry b{text-transform:uppercase;font-size:10px;color:#8ab4f8}.runtime-debugger__entry strong{color:#f3c879;overflow:hidden;text-overflow:ellipsis}.runtime-debugger__entry span{overflow-wrap:anywhere}.runtime-debugger__entry pre{grid-column:4;margin:4px 0 0;max-height:160px;overflow:auto;color:#9fb3cb;background:#0b1018;padding:6px;border-radius:4px;font:11px/1.35 Consolas,monospace}.runtime-debugger__entry.is-error b{color:#ff8c8c}.runtime-debugger__entry.is-lifecycle b{color:#8de0b1}.runtime-debugger__empty{padding:32px;color:#8492a6;text-align:center}
</style>
