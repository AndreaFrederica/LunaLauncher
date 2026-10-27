<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="neo-about-panel">
    <div class="about-hero"><div class="about-logo"><img :src="lunaLogo" alt="Luna Launcher" /></div><div><div class="eyebrow">{{ t('brand.name').toUpperCase() }}</div><h4>{{ t('brand.neoUi') }}</h4><p>{{ t('brand.tagline') }}</p></div></div>
    <div class="about-grid"><div class="about-card"><q-icon name="info"/><span>{{ t('about.build') }}</span><strong>{{ buildLabel }}</strong></div><div class="about-card"><q-icon name="api"/><span>{{ t('about.api') }}</span><strong>{{ apiVersion }}</strong></div><div class="about-card"><q-icon name="devices"/><span>{{ t('about.platform') }}</span><strong>{{ platform }}</strong></div><div class="about-card"><q-icon name="code"/><span>{{ t('about.frontend') }}</span><strong>{{ t('ui.frontendStack') }}</strong></div></div>
    <q-card flat class="runtime-card"><q-card-section><div class="text-subtitle1">{{ t('about.runtime') }}</div><div v-if="loading" class="text-grey-5 q-mt-sm">{{ t('about.loadingRuntime') }}</div><dl v-else><div v-for="item in runtimeRows" :key="item.label"><dt>{{ item.label }}</dt><dd>{{ item.value }}</dd></div></dl></q-card-section></q-card>
    <q-card flat class="services-card"><q-card-section><div class="text-subtitle1">{{ t('about.services') }}</div><p>{{ t('about.servicesDescription') }}</p><div class="service-links"><q-btn flat icon="open_in_new" :label="t('about.bmclapi')" @click="openUrl(bmclApiUrl)"/><q-btn flat icon="open_in_new" :label="t('about.mcim')" @click="openUrl(mcimUrl)"/></div></q-card-section></q-card>
    <q-card flat class="license-card"><q-card-section><div class="text-subtitle1">{{ t('about.license') }}</div><p>{{ t('about.licenseText') }}</p><div class="service-links"><q-btn flat icon="open_in_new" :label="t('about.projectHome')" @click="openUrl(projectUrl)"/><q-btn flat icon="open_in_new" :label="t('ui.licenseLabel')" @click="openUrl(licenseUrl)"/><q-btn flat icon="open_in_new" :label="t('about.docs')" @click="openUrl(docsUrl)"/><q-btn flat icon="open_in_new" :label="t('about.lucideLicense')" @click="openLicense"/></div></q-card-section></q-card>
  </div>
</template>
<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import { useI18n } from 'vue-i18n';
import lunaLogo from '../assets/luna-launcher.png';
const props = defineProps<{ execute:(op:string,p?:Record<string,unknown>)=>Promise<any> }>();
const { t } = useI18n();
const projectUrl = 'https://github.com/AndreaFrederica/LunaLauncher';
const licenseUrl = `${projectUrl}/blob/main/COPYING.md`;
const docsUrl = 'https://lunalauncher.sirrus.cc';
const bmclApiUrl = 'https://bmclapidoc.bangbang93.com/';
const mcimUrl = 'https://mod.mcimirror.top/docs';
const loading = ref(true); const runtime = ref<any>({});
const buildLabel = computed(() => runtime.value.applicationVersion || runtime.value.buildVersion || runtime.value.version || t('about.developmentBuild'));
const platform = computed(() => runtime.value.platform || runtime.value.os || 'Tauri');
const apiVersion = computed(() => runtime.value.apiVersion || runtime.value.api || 'Launcher API');
const runtimeRows = computed(() => Object.entries(runtime.value || {}).filter(([key]) => !['apiVersion','api','platform','os'].includes(key)).slice(0, 8).map(([label,value]) => ({ label, value: typeof value === 'object' ? JSON.stringify(value) : String(value) })));
async function load(){ try { runtime.value = await props.execute('runtime.info'); } catch { runtime.value = {}; } finally { loading.value = false; } }
function openLicense(){ void props.execute('desktop.open-url',{url:'https://mozilla.org/MPL/2.0/'}); }
function openUrl(url: string){ void props.execute('desktop.open-url',{url}); }
onMounted(load);
</script>
<style scoped>
.neo-about-panel{max-width:920px;display:grid;gap:16px}.about-hero{display:flex;align-items:center;gap:16px;padding:24px;border:1px solid var(--neo-border);border-radius:18px;background:linear-gradient(130deg,rgba(99,102,241,.2),var(--neo-panel))}.about-logo{display:grid;place-items:center;width:72px;height:72px;border-radius:22px;background:var(--neo-panel);overflow:hidden}.about-logo img{width:100%;height:100%;object-fit:contain}.about-hero h4{margin:2px 0 4px;font-size:30px}.about-hero p{margin:0;color:var(--neo-muted)}.eyebrow{font-size:11px;letter-spacing:.16em;color:var(--neo-primary);font-weight:700}.about-grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:12px}.about-card{display:flex;flex-direction:column;gap:7px;padding:16px;border:1px solid var(--neo-border);border-radius:14px;background:var(--neo-panel)}.about-card .q-icon{color:var(--neo-primary)}.about-card span{font-size:12px;color:var(--neo-muted)}.about-card strong{font-size:14px;overflow-wrap:anywhere}.runtime-card,.services-card,.license-card{background:var(--neo-panel);border:1px solid var(--neo-border);border-radius:14px}.runtime-card dl{margin:12px 0 0}.runtime-card dl div{display:flex;gap:18px;padding:7px 0;border-bottom:1px solid var(--neo-border)}.runtime-card dt{width:180px;color:var(--neo-muted)}.runtime-card dd{margin:0;overflow-wrap:anywhere}.services-card p,.license-card p{color:var(--neo-muted);line-height:1.6}.service-links{display:flex;flex-wrap:wrap;gap:8px}.service-links :deep(.q-btn){text-transform:none}@media(max-width:760px){.about-grid{grid-template-columns:repeat(2,minmax(0,1fr))}}@media(max-width:450px){.about-grid{grid-template-columns:1fr}}
</style>
