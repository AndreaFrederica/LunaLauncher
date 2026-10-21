<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="neo-about-panel">
    <div class="about-hero"><div class="about-logo">L</div><div><div class="eyebrow">LUNA LAUNCHER</div><h4>Neo UI</h4><p>基于 Launcher API 的现代 Minecraft 启动器界面。</p></div></div>
    <div class="about-grid"><div class="about-card"><q-icon name="info"/><span>Neo UI 构建</span><strong>{{ buildLabel }}</strong></div><div class="about-card"><q-icon name="api"/><span>Launcher API</span><strong>{{ apiVersion }}</strong></div><div class="about-card"><q-icon name="devices"/><span>运行平台</span><strong>{{ platform }}</strong></div><div class="about-card"><q-icon name="code"/><span>前端</span><strong>Vue 3 · Quasar · Tauri</strong></div></div>
    <q-card flat class="runtime-card"><q-card-section><div class="text-subtitle1">运行时信息</div><div v-if="loading" class="text-grey-5 q-mt-sm">正在读取运行时信息…</div><dl v-else><div v-for="item in runtimeRows" :key="item.label"><dt>{{ item.label }}</dt><dd>{{ item.value }}</dd></div></dl></q-card-section></q-card>
    <q-card flat class="license-card"><q-card-section><div class="text-subtitle1">许可证与兼容性</div><p>Neo UI 自有代码采用 MPL-2.0。XMCL 移植部分保留其 MIT 许可；启动器其余部分仍按项目 GPL-3.0 许可。</p><q-btn flat icon="open_in_new" label="查看 MPL-2.0" @click="openLicense"/></q-card-section></q-card>
  </div>
</template>
<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
const props = defineProps<{ execute:(op:string,p?:Record<string,unknown>)=>Promise<any> }>();
const buildLabel = '2026-09-20.1';
const loading = ref(true); const runtime = ref<any>({});
const platform = computed(() => runtime.value.platform || runtime.value.os || 'Tauri');
const apiVersion = computed(() => runtime.value.apiVersion || runtime.value.api || 'Launcher API');
const runtimeRows = computed(() => Object.entries(runtime.value || {}).filter(([key]) => !['apiVersion','api','platform','os'].includes(key)).slice(0, 8).map(([label,value]) => ({ label, value: typeof value === 'object' ? JSON.stringify(value) : String(value) })));
async function load(){ try { runtime.value = await props.execute('runtime.info'); } catch { runtime.value = {}; } finally { loading.value = false; } }
function openLicense(){ void props.execute('desktop.open-url',{url:'https://mozilla.org/MPL/2.0/'}); }
onMounted(load);
</script>
<style scoped>
.neo-about-panel{max-width:920px;display:grid;gap:16px}.about-hero{display:flex;align-items:center;gap:16px;padding:24px;border:1px solid var(--neo-border);border-radius:18px;background:linear-gradient(130deg,rgba(99,102,241,.2),var(--neo-panel))}.about-logo{display:grid;place-items:center;width:72px;height:72px;border-radius:22px;background:linear-gradient(135deg,var(--neo-primary),#4f46e5);font-size:38px;font-weight:800;color:white}.about-hero h4{margin:2px 0 4px;font-size:30px}.about-hero p{margin:0;color:var(--neo-muted)}.eyebrow{font-size:11px;letter-spacing:.16em;color:var(--neo-primary);font-weight:700}.about-grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:12px}.about-card{display:flex;flex-direction:column;gap:7px;padding:16px;border:1px solid var(--neo-border);border-radius:14px;background:var(--neo-panel)}.about-card .q-icon{color:var(--neo-primary)}.about-card span{font-size:12px;color:var(--neo-muted)}.about-card strong{font-size:14px;overflow-wrap:anywhere}.runtime-card,.license-card{background:var(--neo-panel);border:1px solid var(--neo-border);border-radius:14px}.runtime-card dl{margin:12px 0 0}.runtime-card dl div{display:flex;gap:18px;padding:7px 0;border-bottom:1px solid var(--neo-border)}.runtime-card dt{width:180px;color:var(--neo-muted)}.runtime-card dd{margin:0;overflow-wrap:anywhere}.license-card p{color:var(--neo-muted);line-height:1.6}@media(max-width:760px){.about-grid{grid-template-columns:repeat(2,minmax(0,1fr))}}@media(max-width:450px){.about-grid{grid-template-columns:1fr}}
</style>
