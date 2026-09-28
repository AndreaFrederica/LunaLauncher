<template>
  <div class="neo-appearance-panel">
    <div class="preference-card">
      <div class="preference-heading"><q-icon name="web"/><div><strong>Web UI</strong><p>从启动器目录或数据目录的 webui 文件夹加载独立界面。</p></div></div>
      <q-btn class="q-mt-md" color="primary" icon="web" label="管理与切换 Web UI" @click="openWebUiManager" />
    </div>
    <div class="preference-card">
      <div class="preference-heading"><q-icon name="palette"/><div><strong>Neo UI 外观</strong><p>这些选项只影响当前 Web/Tauri 界面，不会修改 Qt 旧界面的主题。</p></div></div>
      <div class="preference-grid">
        <q-select v-model="preferences.theme" dark outlined label="颜色模式" :options="themeOptions" emit-value map-options />
        <q-select v-model="preferences.density" dark outlined label="界面密度" :options="densityOptions" emit-value map-options />
        <q-select v-model="preferences.background" dark outlined label="背景样式" :options="backgroundOptions" emit-value map-options />
        <q-toggle v-model="preferences.animations" dark label="启用界面动效" />
      </div>
      <div class="accent-row"><span>强调色</span><q-btn v-for="color in accents" :key="color" round size="sm" :style="{ background: color }" :class="{ selected: preferences.accent === color }" @click="preferences.accent = color" /></div>
      <div class="preview" :style="{ '--preview-accent': preferences.accent }"><div class="preview-dot"/><div><strong>Neo UI 预览</strong><small>{{ themeLabel }} · {{ preferences.density === 'compact' ? '紧凑' : '舒适' }}</small></div><q-icon name="check_circle" color="primary"/></div>
      <q-btn flat label="恢复默认外观" icon="restart_alt" class="q-mt-md" @click="resetNeoUiPreferences" />
    </div>
    <div class="preference-card"><div class="preference-heading"><q-icon name="language"/><div><strong>语言</strong><p>语言和 Qt 资源包仍由兼容设置控制。</p></div></div><q-banner dense class="bg-grey-9">Neo UI 会沿用启动器当前语言；Qt 主题、图标和背景请在“Qt 兼容界面”中管理。</q-banner></div>
  </div>
</template>
<script setup lang="ts">
import { computed } from 'vue';
import { neoUiPreferences, resetNeoUiPreferences } from '../neo-ui-preferences';
import { launcher } from '../boot/launcher-api';
import { useQuasar } from 'quasar';
const quasar = useQuasar();
async function openWebUiManager() {
  try { await launcher.openWebUiManager(); }
  catch (e: any) { quasar.notify({ type: 'negative', message: e.message || String(e), timeout: 4000 }); }
}
const preferences = neoUiPreferences;
const accents = ['#818cf8', '#38bdf8', '#34d399', '#f59e0b', '#f472b6', '#fb7185'];
const themeOptions = [{ label: '深色', value: 'dark' }, { label: '浅色', value: 'light' }, { label: '跟随系统', value: 'system' }];
const densityOptions = [{ label: '舒适', value: 'comfortable' }, { label: '紧凑', value: 'compact' }];
const backgroundOptions = [{ label: '渐变背景', value: 'gradient' }, { label: '纯色背景', value: 'solid' }];
const themeLabel = computed(() => themeOptions.find(x => x.value === preferences.theme)?.label || '深色');
</script>
<style scoped>
.neo-appearance-panel{max-width:920px;display:grid;gap:16px}.preference-card{padding:20px;border:1px solid var(--neo-border);border-radius:16px;background:var(--neo-panel)}.preference-heading{display:flex;gap:12px;align-items:flex-start}.preference-heading>.q-icon{color:var(--neo-primary);font-size:24px}.preference-heading strong{font-size:17px}.preference-heading p{margin:4px 0 0;color:var(--neo-muted);font-size:13px}.preference-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px;margin-top:20px}.accent-row{display:flex;align-items:center;gap:10px;margin-top:18px;color:var(--neo-muted)}.accent-row .q-btn{border:2px solid transparent}.accent-row .q-btn.selected{border-color:#fff;box-shadow:0 0 0 2px var(--preview-accent)}.preview{display:flex;align-items:center;gap:12px;margin-top:20px;padding:14px;border-radius:12px;background:linear-gradient(110deg,color-mix(in srgb,var(--preview-accent) 28%,transparent),rgba(255,255,255,.04));border:1px solid color-mix(in srgb,var(--preview-accent) 45%,transparent)}.preview-dot{width:12px;height:12px;border-radius:50%;background:var(--preview-accent);box-shadow:0 0 14px var(--preview-accent)}.preview div:nth-child(2){flex:1}.preview small{display:block;color:var(--neo-muted);margin-top:3px}@media(max-width:650px){.preference-grid{grid-template-columns:1fr}}
</style>
