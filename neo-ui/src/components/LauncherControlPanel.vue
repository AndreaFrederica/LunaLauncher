<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="control-panel">
    <q-tabs v-model="tab" align="left" outside-arrows mobile-arrows>
      <q-tab name="appearance" :label="t('neo.auto2.k29b208b606')" icon="palette" />
      <q-tab name="network" :label="t('neo.auto2.kb046fe277d')" icon="cloud" />
      <q-tab name="updates" :label="t('neo.auto2.kd9db02d07a')" icon="system_update" />
      <q-tab name="maintenance" :label="t('neo.auto2.k72527e2f0e')" icon="build" />
    </q-tabs>

    <div v-if="tab==='appearance'" class="q-mt-md">
      <div class="row q-col-gutter-md">
        <div class="col-12 col-md-4"><q-select dark outlined v-model="appearance.theme" :options="themes" option-label="name" option-value="id" emit-value map-options :label="t('neo.auto2.ka68549797b')" /></div>
        <div class="col-12 col-md-4"><q-select dark outlined v-model="appearance.icons" :options="iconThemes" option-label="name" option-value="id" emit-value map-options :label="t('neo.auto2.k69fed286c9')" /></div>
        <div class="col-12 col-md-4"><q-select dark outlined v-model="appearance.background" :options="backgrounds" option-label="name" option-value="id" emit-value map-options :label="t('neo.auto2.kb13d33a528')" /></div>
      </div>
      <div class="row q-col-gutter-md q-mt-sm">
        <div class="col-12 col-md-6"><q-select dark outlined v-model="language" :options="languages" option-label="name" option-value="id" emit-value map-options :label="t('neo.auto2.kd5d8edd12c')" /></div>
        <div class="col-12 col-md-6 flex items-center"><q-toggle dark v-model="useSystemLocale" :label="t('neo.auto2.k726bce844f')" /></div>
      </div>
      <div class="row q-gutter-sm q-mt-md"><q-btn color="primary" :label="t('neo.auto2.k845ee19d2a')" :disable="busy" @click="saveAppearance" /><q-btn color="primary" :label="t('neo.auto2.k94171f0c57')" :disable="busy || !language" @click="saveLanguage" /><q-btn flat :label="t('neo.auto2.k5ce653f3f5')" :disable="busy" @click="loadAppearance" /><q-btn flat :label="t('neo.auto2.k6c58ac1d7a')" :disable="busy" @click="refreshLanguage" /></div>
      <q-list bordered separator class="q-mt-lg"><q-item v-for="pack in installedPacks" :key="pack.kind+':'+pack.id"><q-item-section><q-item-label>{{ pack.name || pack.id }}</q-item-label><q-item-label caption>{{ pack.kind }} · {{ pack.builtin ? t('neo.auto2.k09ceea7644') : t('neo.auto2.kc493338e8c') }}</q-item-label></q-item-section><q-item-section side><q-btn v-if="!pack.builtin" flat color="negative" :label="t('neo.auto2.k3755f56f2f')" :disable="busy" @click="removePack(pack)" /></q-item-section></q-item></q-list>
    </div>

    <div v-else-if="tab==='network'" class="q-mt-md">
      <div class="row q-col-gutter-md"><div class="col-12 col-md-4"><q-select dark outlined v-model="proxy.type" :options="['None','HTTP','SOCKS5','Default']" :label="t('neo.auto2.k6caded1529')" /></div><div class="col-12 col-md-5"><q-input dark outlined v-model="proxy.host" :label="t('neo.auto2.k2e8a0cf711')" /></div><div class="col-12 col-md-3"><q-input dark outlined type="number" v-model.number="proxy.port" :label="t('neo.auto2.k6cbb7335b5')" /></div></div>
      <div class="row q-col-gutter-md q-mt-sm"><div class="col-12 col-md-6"><q-input dark outlined v-model="proxy.username" :label="t('neo.auto2.k48517cc4c2')" /></div><div class="col-12 col-md-6"><q-input dark outlined v-model="proxy.password" type="password" :label="t('neo.auto2.k29e03e6c82')" /></div></div>
      <div class="row q-gutter-sm q-mt-md"><q-btn color="primary" :label="t('neo.auto2.k1c5b23a17a')" :disable="busy" @click="saveProxy" /><q-btn flat :label="t('neo.auto2.k10b7d8eccc')" :disable="busy" @click="testProxy" /></div><q-banner v-if="proxyResult" class="bg-grey-9 q-mt-md">{{ proxyResult }}</q-banner>
    </div>

    <div v-else-if="tab==='updates'" class="q-mt-md">
      <q-toggle dark v-model="update.automatic" :label="t('neo.auto2.kf54e16b861')" /><q-toggle dark v-model="update.beta" :label="t('neo.auto2.k414244df13')" class="q-ml-md" /><q-input dark outlined type="number" v-model.number="update.intervalSeconds" :label="t('neo.auto2.k1c03f013bd')" class="q-mt-md" />
      <div class="row q-gutter-sm q-mt-md"><q-btn color="primary" :label="t('neo.auto2.k6de0ba5064')" :disable="busy" @click="saveUpdate" /><q-btn flat :label="t('neo.auto2.ka6df38586d')" :disable="busy" @click="checkUpdates" /><q-btn flat :label="t('neo.auto2.ke1ef0250c0')" :disable="busy" @click="listReleases" /></div>
      <q-list v-if="releases.length" bordered separator class="q-mt-md"><q-item v-for="release in releases" :key="release.tag || release.versionTag"><q-item-section><q-item-label>{{ release.name || release.tag || release.versionTag }}</q-item-label><q-item-label caption>{{ release.publishedAt || release.date || '' }}</q-item-label></q-item-section><q-item-section side><q-btn flat :label="t('neo.auto2.k087db63ab1')" :disable="busy" @click="applyRelease(release)" /></q-item-section></q-item></q-list>
    </div>

    <div v-else class="q-mt-md">
      <q-btn color="primary" :label="t('neo.auto2.k2dcd7ce3b3')" :disable="busy" @click="importPrism" /><q-btn flat :label="t('neo.auto2.kacc58eb1fb')" :disable="busy" @click="clearCache" class="q-ml-sm" /><q-btn flat :label="t('neo.auto2.ka033084f9a')" :disable="busy" @click="loadNews" class="q-ml-sm" />
      <q-list v-if="news.length" bordered separator class="q-mt-md"><q-item v-for="entry in news" :key="entry.url || entry.title"><q-item-section><q-item-label>{{ entry.title }}</q-item-label><q-item-label caption>{{ entry.url }}</q-item-label></q-item-section><q-item-section side><q-btn flat :label="t('neo.auto2.k65fc81e161')" @click="execute('desktop.open-url',{url:entry.url})" /></q-item-section></q-item></q-list>
    </div>
    <q-banner v-if="error" class="bg-red-9 q-mt-md">{{ error }}</q-banner>
  </div>
</template>
<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { onMounted, ref } from 'vue';
const props=defineProps<{busy:boolean;execute:(op:string,p?:Record<string,unknown>)=>Promise<any>}>();
const tab=ref('appearance'),error=ref(''),themes=ref<any[]>([]),iconThemes=ref<any[]>([]),backgrounds=ref<any[]>([]),installedPacks=ref<any[]>([]),languages=ref<any[]>([]),language=ref(''),useSystemLocale=ref(false),appearance=ref<any>({theme:'',icons:'',background:''}),proxy=ref<any>({type:'None',host:'',port:0,username:'',password:''}),proxyResult=ref(''),update=ref<any>({automatic:true,beta:false,intervalSeconds:86400}),releases=ref<any[]>([]),news=ref<any[]>([]);
async function attempt(action:()=>Promise<any>){error.value='';try{return await action();}catch(e){error.value=String(e);}}
async function loadAppearance(){await attempt(async()=>{const r=await props.execute('appearance.catalog');themes.value=r.themes||r.applicationThemes||[];iconThemes.value=r.icons||r.iconThemes||[];backgrounds.value=r.backgrounds||[];installedPacks.value=[...themes.value.map((x:any)=>({...x,kind:'theme'})),...iconThemes.value.map((x:any)=>({...x,kind:'icons'})),...backgrounds.value.map((x:any)=>({...x,kind:'background'}))];const current=r.selected||r.current||{};appearance.value={...appearance.value,...current};});}
async function loadLanguage(){await attempt(async()=>{const r=await props.execute('language.list');languages.value=r.languages||r;language.value=r.current||r.language||language.value;useSystemLocale.value=Boolean(r.useSystemLocale);});}
async function saveAppearance(){await attempt(async()=>{await props.execute('appearance.select',appearance.value);await loadAppearance();});}
async function refreshLanguage(){await attempt(async()=>{await props.execute('language.refresh');await loadLanguage();});}
async function saveLanguage(){await attempt(async()=>{await props.execute('language.select',{language:language.value,useSystemLocale:useSystemLocale.value});await loadLanguage();});}
async function removePack(pack:any){await attempt(async()=>{await props.execute('appearance.remove',{kind:pack.kind,id:pack.id,confirm:true});await loadAppearance();});}
async function loadProxy(){await attempt(async()=>{proxy.value={...proxy.value,...await props.execute('network.proxy.get')};});}
async function saveProxy(){await attempt(async()=>{await props.execute('network.proxy.set',proxy.value);await loadProxy();});}
async function testProxy(){await attempt(async()=>{const r=await props.execute('network.proxy.test',{url:'https://launcher.mojang.com',timeoutMs:10000});proxyResult.value=r.message||t('neo.extra.connectionSucceeded',{status:r.status||'OK'});});}
async function loadUpdate(){await attempt(async()=>{update.value={...update.value,...await props.execute('launcher.update.status')};});}
async function saveUpdate(){await attempt(async()=>{await props.execute('launcher.update.configure',update.value);await loadUpdate();});}
async function checkUpdates(){await attempt(async()=>{const r=await props.execute('launcher.update.check');releases.value=r.releases||r;});}
async function listReleases(){await attempt(async()=>{const r=await props.execute('launcher.update.releases');releases.value=r.releases||r;});}
async function applyRelease(r:any){await attempt(()=>props.execute('launcher.update.apply',{versionTag:r.versionTag||r.tag,assetName:r.assetName}));}
async function importPrism(){await attempt(()=>props.execute('settings.import-prism',{confirm:true}));}
async function clearCache(){await attempt(()=>props.execute('launcher.cache.clear',{confirm:true}));}
async function loadNews(){await attempt(async()=>{const r=await props.execute('launcher.news');news.value=r||[];});}
onMounted(()=>{void Promise.all([loadAppearance(),loadLanguage(),loadProxy(),loadUpdate()]);});
</script>
<style scoped>.control-panel{max-width:1000px}.control-panel :deep(.q-card){background:var(--neo-panel);border:1px solid var(--neo-border)}</style>
