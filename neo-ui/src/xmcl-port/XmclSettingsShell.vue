<template>
  <!-- Port of XMCL Setting.vue's navigation-sidebar/content split. -->
  <div class="xmcl-setting-page">
    <nav class="xmcl-setting-sidebar" aria-label="设置导航"><div class="xmcl-setting-card"><div class="xmcl-setting-label">设置</div><button v-for="item in sections" :key="item.id" :class="{active:section===item.id}" @click="section=item.id"><q-icon :name="item.icon" size="18px"/><span>{{ item.label }}</span></button></div></nav>
    <main class="xmcl-setting-content">
      <section v-if="section==='general'" class="xmcl-setting-section"><header><q-icon name="tune"/><div><h5>通用</h5><p>启动器行为、默认目录和实例管理。</p></div></header><SettingsPanel :settings="settings" :busy="busy" @save="(key,value)=>execute('settings.set',{...scope,key,value})" @reset="key=>execute('settings.reset',{...scope,key})"/></section>
      <section v-else-if="section==='appearance'" class="xmcl-setting-section"><header><q-icon name="palette"/><div><h5>外观</h5><p>主题、图标、背景和语言。</p></div></header><LauncherControlPanel :busy="busy" :execute="execute"/></section>
      <section v-else-if="section==='runtime'" class="xmcl-setting-section"><header><q-icon name="code"/><div><h5>Java 与运行环境</h5><p>检测 Java、诊断兼容性并选择运行时。</p></div></header><JavaPanel :scope="scope" :settings="settings" :busy="busy" :execute="execute" @changed="emit('changed')"/></section>
      <section v-else-if="section==='network'" class="xmcl-setting-section"><header><q-icon name="public"/><div><h5>网络</h5><p>代理和网络连接设置。</p></div></header><LauncherControlPanel :busy="busy" :execute="execute"/></section>
      <section v-else class="xmcl-setting-section"><header><q-icon name="info"/><div><h5>关于和更新</h5><p>检查更新、清理缓存和导入设置。</p></div></header><LauncherControlPanel :busy="busy" :execute="execute"/></section>
    </main>
  </div>
</template>
<script setup lang="ts">
import { ref } from 'vue';
import JavaPanel from '../components/JavaPanel.vue';
import SettingsPanel from '../components/SettingsPanel.vue';
import LauncherControlPanel from '../components/LauncherControlPanel.vue';
defineProps<{settings:any[];busy:boolean;scope:Record<string,unknown>;execute:(op:string,p?:Record<string,unknown>)=>Promise<any>}>();
const emit=defineEmits<{changed:[]}>();
const section=ref('general');
const sections=[{id:'general',label:'通用',icon:'tune'},{id:'appearance',label:'外观',icon:'palette'},{id:'runtime',label:'Java',icon:'code'},{id:'network',label:'网络',icon:'wifi'},{id:'about',label:'关于与更新',icon:'info'}];
</script>
<style scoped>
.xmcl-setting-page{display:flex;min-height:680px;max-width:1250px;margin:0 auto}.xmcl-setting-sidebar{width:250px;min-width:250px;padding:24px 12px 24px 20px}.xmcl-setting-card{position:sticky;top:24px;padding:12px;border-radius:16px;background:rgba(35,35,42,.76);border:1px solid rgba(255,255,255,.1);backdrop-filter:blur(20px)}.xmcl-setting-label{padding:8px 12px 12px;color:rgba(255,255,255,.55);font-size:12px;text-transform:uppercase;font-weight:700;letter-spacing:.08em}.xmcl-setting-card button{width:100%;display:flex;align-items:center;gap:10px;border:0;background:transparent;color:rgba(255,255,255,.72);padding:11px 12px;border-radius:10px;text-align:left;cursor:pointer;margin:2px 0}.xmcl-setting-card button:hover{background:rgba(255,255,255,.08);color:#fff}.xmcl-setting-card button.active{background:rgba(129,140,248,.22);color:#a5b4fc}.xmcl-setting-content{flex:1;min-width:0;padding:24px 28px;overflow-y:auto}.xmcl-setting-section>header{display:flex;align-items:flex-start;gap:12px;margin:0 0 22px}.xmcl-setting-section>header>.q-icon{margin-top:4px;color:#a5b4fc}.xmcl-setting-section h5{margin:0 0 4px;font-size:24px}.xmcl-setting-section p{margin:0;color:rgba(255,255,255,.55)}@media(max-width:800px){.xmcl-setting-page{display:block}.xmcl-setting-sidebar{width:auto;min-width:0;padding:8px 12px}.xmcl-setting-card{position:static;display:flex;overflow:auto;gap:4px}.xmcl-setting-label{display:none}.xmcl-setting-card button{width:auto;white-space:nowrap}.xmcl-setting-content{padding:18px 12px}}
</style>
