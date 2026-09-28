<template>
  <div class="system-bar" role="toolbar" aria-label="XMCL system bar">
    <button class="system-bar-back" aria-label="刷新实例库" @click="emit('refresh')"><span class="material-icons">arrow_back</span></button>
    <div class="system-bar-drag" data-tauri-drag-region @mousedown="startDragging"><strong>Luna Launcher</strong><small>NEO UI</small></div>
    <div class="system-bar-spacer" data-tauri-drag-region @mousedown="startDragging" />
    <button class="system-bar-badge" aria-label="Web UI 管理" title="Web UI 管理 (Ctrl+Shift+U)" @click="openWebUiManager"><span class="material-icons">web</span></button>
    <button class="system-bar-badge" aria-label="搜索" @click="emit('search')"><span class="material-icons">search</span></button>
    <button class="system-bar-badge" aria-label="任务" @click="emit('tasks')"><span class="material-icons">assignment</span><i v-if="busy" /></button>
    <span class="system-bar-status"><span class="status-dot" :class="{ connected }" />{{ connected ? '已连接' : '未连接' }}</span>
    <button class="system-bar-badge" :disabled="busy" aria-label="刷新" @click="emit('refresh')"><span class="material-icons">refresh</span></button>
    <button class="system-bar-window" aria-label="最小化" @click="minimize"><span class="material-icons">remove</span></button>
    <button class="system-bar-window" aria-label="最大化" @click="toggleMaximize"><span class="material-icons">crop_square</span></button>
    <button class="system-bar-window close" aria-label="关闭" @click="closeWindow"><span class="material-icons">close</span></button>
  </div>
</template>
<script setup lang="ts">
import { getCurrentWindow } from '@tauri-apps/api/window';
import { launcher } from '../boot/launcher-api';
import { useQuasar } from 'quasar';
const quasar = useQuasar();
async function openWebUiManager() {
  try { await launcher.openWebUiManager(); }
  catch (e: any) { quasar.notify({ type: 'negative', message: e.message || String(e), timeout: 4000 }); }
}
defineProps<{ connected: boolean; busy: boolean }>();
const emit = defineEmits<{ refresh: []; tasks: []; search: [] }>();
async function minimize() { try { await getCurrentWindow().minimize(); } catch { /* Web preview has no native window. */ } }
async function toggleMaximize() { try { const window = getCurrentWindow(); if (await window.isMaximized()) await window.unmaximize(); else await window.maximize(); } catch { /* Web preview has no native window. */ } }
async function closeWindow() { try { await getCurrentWindow().close(); } catch { /* Web preview has no native window. */ } }
async function startDragging(event: MouseEvent) {
  if (event.button !== 0) return;
  try { await getCurrentWindow().startDragging(); } catch { /* Browser preview has no native window. */ }
}
</script>
<style scoped>
.system-bar{position:fixed;top:0;left:0;right:0;height:38px;z-index:100;display:flex;align-items:center;gap:3px;padding:0 8px;background:rgba(17,17,22,.86);backdrop-filter:blur(18px);color:rgba(255,255,255,.7);font-size:12px;box-shadow:0 1px 0 rgba(255,255,255,.06)}.system-bar-drag{height:38px;display:flex;align-items:center;gap:8px;padding:0 12px;flex:0 0 auto;-webkit-app-region:drag;user-select:none}.system-bar-drag strong{color:#fff;font-size:12px;letter-spacing:.02em}.system-bar-drag small{color:#818cf8;font-size:9px;letter-spacing:.14em;font-weight:700}.system-bar-spacer{height:38px;flex:1;-webkit-app-region:drag}.system-bar-back,.system-bar-badge,.system-bar-window{height:38px;min-width:32px;padding:0 8px;display:inline-flex;align-items:center;justify-content:center;border:0;background:transparent;color:inherit;cursor:pointer;-webkit-app-region:no-drag}.system-bar-back:hover,.system-bar-badge:hover,.system-bar-window:hover{background:rgba(255,255,255,.12);color:#fff}.system-bar-window.close:hover{background:#c42b3c;color:#fff}.system-bar-badge{position:relative}.system-bar-badge i{position:absolute;right:4px;top:7px;width:5px;height:5px;border-radius:50%;background:#fbc02d}.system-bar-status{display:flex;align-items:center;gap:5px;margin:0 4px;color:rgba(255,255,255,.55);-webkit-app-region:no-drag}.status-dot{width:7px;height:7px;border-radius:50%;background:#777}.status-dot.connected{background:#4caf50}.material-icons{font-size:18px;line-height:1}
</style>
