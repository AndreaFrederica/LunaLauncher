<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="neo-appearance-panel">
    <div class="preference-card">
      <div class="preference-heading"><q-icon name="web"/><div><strong>{{ t('appearance.webTitle') }}</strong><p>{{ t('appearance.webDescription') }}</p></div></div>
      <q-btn class="q-mt-md" color="primary" icon="web" :label="t('appearance.webManage')" @click="openWebUiManager" />
    </div>
    <div class="preference-card">
      <div class="preference-heading"><q-icon name="palette"/><div><strong>{{ t('appearance.title') }}</strong><p>{{ t('appearance.description') }}</p></div></div>
      <div class="preference-grid">
        <q-select v-model="preferences.shell" dark outlined :label="t('appearance.style')" :options="shellOptions" emit-value map-options />
        <q-select v-model="preferences.theme" dark outlined :label="t('appearance.colorMode')" :options="themeOptions" emit-value map-options />
        <q-select v-model="preferences.density" dark outlined :label="t('appearance.density')" :options="densityOptions" emit-value map-options />
        <q-select v-model="preferences.background" dark outlined :label="t('appearance.background')" :options="backgroundOptions" emit-value map-options />
        <q-toggle v-model="preferences.animations" dark :label="t('appearance.animations')" />
      </div>
      <div class="accent-row"><span>{{ t('appearance.accent') }}</span><q-btn v-for="color in accents" :key="color" round size="sm" :style="{ background: color }" :class="{ selected: preferences.accent === color }" @click="preferences.accent = color" /></div>
      <div class="preview" :class="{ 'preview-pcl': preferences.shell === 'pcl' }" :style="{ '--preview-accent': preferences.accent }"><div class="preview-dot"/><div><strong>{{ preferences.shell === 'pcl' ? 'PCL Style' : t('appearance.nativePreview') }}</strong><small>{{ themeLabel }} · {{ densityLabel }}</small></div><q-icon name="check_circle" color="primary"/></div>
      <q-btn flat :label="t('appearance.reset')" icon="restart_alt" class="q-mt-md" @click="resetNeoUiPreferences" />
    </div>
    <div class="preference-card"><div class="preference-heading"><q-icon name="language"/><div><strong>{{ t('language.title') }}</strong><p>{{ t('language.nativeUi') }}</p></div></div><q-banner dense class="bg-grey-9">{{ t('language.introBody') }}</q-banner></div>
  </div>
</template>
<script setup lang="ts">
import { computed } from 'vue';
import { useI18n } from 'vue-i18n';
import { neoUiPreferences, resetNeoUiPreferences } from '../neo-ui-preferences';
import { launcher } from '../boot/launcher-api';
import { useQuasar } from 'quasar';
const quasar = useQuasar();
const { t } = useI18n();
async function openWebUiManager() {
  try { await launcher.openWebUiManager(); }
  catch (e: any) { quasar.notify({ type: 'negative', message: e.message || String(e), timeout: 4000 }); }
}
const preferences = neoUiPreferences;
const accents = ['#818cf8', '#38bdf8', '#34d399', '#f59e0b', '#f472b6', '#fb7185'];
const shellOptions = computed(() => [{ label: 'PCL Style', value: 'pcl' }, { label: t('appearance.native'), value: 'neo' }]);
const themeOptions = computed(() => [{ label: t('appearance.dark'), value: 'dark' }, { label: t('appearance.light'), value: 'light' }, { label: t('appearance.system'), value: 'system' }]);
const densityOptions = computed(() => [{ label: t('appearance.comfortable'), value: 'comfortable' }, { label: t('appearance.compact'), value: 'compact' }]);
const backgroundOptions = computed(() => [{ label: t('appearance.gradient'), value: 'gradient' }, { label: t('appearance.solid'), value: 'solid' }]);
const themeLabel = computed(() => themeOptions.value.find(x => x.value === preferences.theme)?.label || t('appearance.dark'));
const densityLabel = computed(() => densityOptions.value.find(x => x.value === preferences.density)?.label || t('appearance.comfortable'));
</script>
<style scoped>
.neo-appearance-panel{max-width:920px;display:grid;gap:16px}.preference-card{padding:20px;border:1px solid var(--neo-border);border-radius:16px;background:var(--neo-panel)}.preference-heading{display:flex;gap:12px;align-items:flex-start}.preference-heading>.q-icon{color:var(--neo-primary);font-size:24px}.preference-heading strong{font-size:17px}.preference-heading p{margin:4px 0 0;color:var(--neo-muted);font-size:13px}.preference-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px;margin-top:20px}.accent-row{display:flex;align-items:center;gap:10px;margin-top:18px;color:var(--neo-muted)}.accent-row .q-btn{border:2px solid transparent}.accent-row .q-btn.selected{border-color:#fff;box-shadow:0 0 0 2px var(--preview-accent)}.preview{display:flex;align-items:center;gap:12px;margin-top:20px;padding:14px;border-radius:12px;background:linear-gradient(110deg,color-mix(in srgb,var(--preview-accent) 28%,transparent),rgba(255,255,255,.04));border:1px solid color-mix(in srgb,var(--preview-accent) 45%,transparent)}.preview.preview-pcl{background:linear-gradient(110deg,rgba(65,139,201,.28),rgba(255,255,255,.04));border-color:rgba(65,139,201,.5)}.preview-dot{width:12px;height:12px;border-radius:50%;background:var(--preview-accent);box-shadow:0 0 14px var(--preview-accent)}.preview-pcl .preview-dot{background:#418bc9;box-shadow:0 0 14px rgba(65,139,201,.8)}.preview div:nth-child(2){flex:1}.preview small{display:block;color:var(--neo-muted);margin-top:3px}@media(max-width:650px){.preference-grid{grid-template-columns:1fr}}
</style>
