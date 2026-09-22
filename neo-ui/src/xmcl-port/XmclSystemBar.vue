<template>
  <!-- Port of XMCL AppSystemBar with browser-safe controls. -->
  <div class="xmcl-system-bar"><div class="xmcl-system-brand"><img :src="logo" alt="Luna"/><span>Luna</span><small>XMCL UI port</small></div><div class="xmcl-system-status"><q-icon name="circle" size="8px" :color="connected?'positive':'grey'"/> {{ connected?'已连接启动器':'未连接启动器' }}<q-btn flat dense round icon="refresh" :disable="busy" aria-label="刷新" @click="emit('refresh')"/></div></div>
</template>
<script setup lang="ts">
import logo from './assets/logo.webp';
defineProps<{connected:boolean;busy:boolean}>();const emit=defineEmits<{refresh:[]}>();
</script>
<style scoped>.xmcl-system-bar{position:fixed;top:0;left:0;right:0;height:34px;z-index:100;display:flex;align-items:center;justify-content:space-between;padding:0 12px 0 92px;background:rgba(17,17,22,.68);backdrop-filter:blur(18px);border-bottom:1px solid rgba(255,255,255,.08);user-select:none}.xmcl-system-brand{display:flex;align-items:center;gap:6px;font-size:12px;font-weight:700}.xmcl-system-brand img{width:18px;height:18px;border-radius:5px}.xmcl-system-brand small{color:rgba(255,255,255,.4);font-weight:400;margin-left:4px}.xmcl-system-status{display:flex;align-items:center;gap:7px;color:rgba(255,255,255,.6);font-size:11px}.xmcl-system-status .q-btn{margin-left:6px}@media(max-width:600px){.xmcl-system-bar{padding-left:86px}.xmcl-system-brand small{display:none}}
</style>
