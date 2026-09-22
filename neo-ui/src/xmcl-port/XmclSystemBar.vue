<template>
  <div class="system-bar" role="toolbar" aria-label="XMCL system bar">
    <button class="system-bar-back" aria-label="刷新实例库" @click="emit('refresh')"><span class="material-icons">arrow_back</span></button>
    <div class="system-bar-spacer" />
    <button class="system-bar-badge" aria-label="搜索"><span class="material-icons">search</span></button>
    <button class="system-bar-badge" aria-label="任务" @click="emit('tasks')"><span class="material-icons">assignment</span><i v-if="busy" /></button>
    <span class="system-bar-status"><span class="status-dot" :class="{ connected }" />{{ connected ? '已连接' : '未连接' }}</span>
    <button class="system-bar-badge" :disabled="busy" aria-label="刷新" @click="emit('refresh')"><span class="material-icons">refresh</span></button>
  </div>
</template>
<script setup lang="ts">
defineProps<{ connected: boolean; busy: boolean }>();
const emit = defineEmits<{ refresh: []; tasks: [] }>();
</script>
<style scoped>
.system-bar{position:fixed;top:0;left:0;right:0;height:30px;z-index:100;display:flex;align-items:center;gap:3px;padding:0 8px;background:rgba(17,17,22,.55);backdrop-filter:blur(18px);color:rgba(255,255,255,.7);font-size:12px}.system-bar-spacer{flex:1}.system-bar-back,.system-bar-badge{height:30px;min-width:30px;padding:0 7px;display:inline-flex;align-items:center;justify-content:center;border:0;background:transparent;color:inherit;cursor:pointer}.system-bar-back:hover,.system-bar-badge:hover{background:rgba(255,255,255,.12);color:#fff}.system-bar-badge{position:relative}.system-bar-badge i{position:absolute;right:4px;top:5px;width:5px;height:5px;border-radius:50%;background:#fbc02d}.system-bar-status{display:flex;align-items:center;gap:5px;margin:0 4px;color:rgba(255,255,255,.55)}.status-dot{width:7px;height:7px;border-radius:50%;background:#777}.status-dot.connected{background:#4caf50}.material-icons{font-size:18px;line-height:1}
</style>
