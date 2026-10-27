<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="launcher-settings-panel">
    <q-tabs v-model="tab" align="left" outside-arrows mobile-arrows>
      <q-tab name="general" :label="t('neo.auto2.k021a1cbe78')" icon="tune" />
      <q-tab name="java" :label="t('neo.auto2.kfc95eae98d')" icon="code" />
      <q-tab name="downloads" :label="t('neo.auto2.k2f85759474')" icon="download" />
      <q-tab name="mirrors" :label="t('neo.auto2.k39acdd11d4')" icon="public" />
      <q-tab name="integrations" :label="t('neo.auto2.ke12180a10c')" icon="extension" />
      <q-tab name="network" :label="t('neo.auto2.k9e5f712974')" icon="settings" />
      <q-tab name="qt" :label="t('neo.auto2.kbd52c2c514')" icon="desktop_windows" />
    </q-tabs>

    <div v-if="tab==='general'" class="q-mt-md">
      <SettingsPanel :settings="settings" category="paths" :busy="busy" @save="save" @reset="reset" />
      <SettingsPanel class="q-mt-lg" :settings="settings" category="console" :busy="busy" @save="save" @reset="reset" />
    </div>
    <div v-else-if="tab==='java'" class="q-mt-md">
      <JavaPanel :scope="scope" :settings="settings" :busy="busy" :execute="execute" @changed="emit('changed')" />
      <SettingsPanel class="q-mt-lg" :settings="settings" category="java" :busy="busy" @save="save" @reset="reset" />
    </div>
    <div v-else-if="tab==='downloads'" class="q-mt-md">
      <section class="settings-card">
        <h6>{{ t('neo.auto2.k2f85759474') }}</h6>
        <q-select dark outlined emit-value map-options :model-value="value('DownloadBackend')" :options="backends" :label="t('neo.auto2.kd21febb3dc')" @update:model-value="saveKey('DownloadBackend',$event)" />
        <q-toggle dark :model-value="bool('PclDownloadFallbackToQt')" :label="t('neo.auto2.k30ad19d3a5')" @update:model-value="saveKey('PclDownloadFallbackToQt',$event)" />
        <div class="row q-col-gutter-md"><q-input class="col" dark outlined type="number" :model-value="number('PclDownloadThreadLimit')" :label="t('neo.auto2.kb0279be358')" @update:model-value="saveKey('PclDownloadThreadLimit',Number($event))" /><q-input class="col" dark outlined type="number" :model-value="number('PclDownloadSpeedLimitKBps')" :label="t('neo.auto2.k25d6c7e35d')" @update:model-value="saveKey('PclDownloadSpeedLimitKBps',Number($event))" /></div>
      </section>
      <section class="settings-card q-mt-md">
        <h6>{{ t('ui.aria2') }}</h6>
        <q-toggle dark :model-value="bool('Aria2Enabled')" :label="t('neo.auto2.k10bfe990fe')" @update:model-value="saveKey('Aria2Enabled',$event)" />
        <q-toggle dark :model-value="bool('Aria2AutoInstall')" :label="t('neo.auto2.k75ff090a68')" @update:model-value="saveKey('Aria2AutoInstall',$event)" />
        <q-toggle dark :model-value="bool('Aria2FallbackToQt')" :label="t('neo.auto2.ka8ffe0fe9c')" @update:model-value="saveKey('Aria2FallbackToQt',$event)" />
        <div class="row q-col-gutter-md"><q-input class="col" dark outlined type="number" :model-value="number('Aria2MaxConcurrentDownloads')" :label="t('neo.auto2.kb8c414a7d8')" @update:model-value="saveKey('Aria2MaxConcurrentDownloads',Number($event))" /><q-input class="col" dark outlined type="number" :model-value="number('Aria2MaxConnectionPerServer')" :label="t('neo.auto2.kafb366a11a')" @update:model-value="saveKey('Aria2MaxConnectionPerServer',Number($event))" /><q-input class="col" dark outlined type="number" :model-value="number('Aria2Split')" :label="t('neo.auto2.k7c6534c94c')" @update:model-value="saveKey('Aria2Split',Number($event))" /></div>
        <div class="row q-col-gutter-md"><q-input class="col" dark outlined :model-value="text('Aria2MinSplitSize')" :label="t('neo.auto2.k956f301756')" @update:model-value="saveKey('Aria2MinSplitSize',$event)" /><q-input class="col" dark outlined :model-value="text('Aria2FileAllocation')" :label="t('neo.auto2.k5016a7bffb')" @update:model-value="saveKey('Aria2FileAllocation',$event)" /><q-input class="col" dark outlined type="number" :model-value="number('Aria2RpcPort')" :label="t('neo.auto2.k681dccf8d5')" @update:model-value="saveKey('Aria2RpcPort',Number($event))" /></div>
        <q-toggle dark :model-value="bool('Aria2Continue')" :label="t('neo.auto2.ka99f863cff')" @update:model-value="saveKey('Aria2Continue',$event)" />
        <q-input dark outlined type="textarea" autogrow :model-value="text('Aria2ExtraArgs')" :label="t('neo.auto2.ka63811eb1f')" @update:model-value="saveKey('Aria2ExtraArgs',$event)" />
        <div class="row q-gutter-sm q-mt-md"><q-btn flat :label="t('neo.auto2.k85b62254d1')" :disable="busy" @click="loadDownloads" /><q-btn flat :label="t('neo.auto2.k149b3c638f')" :disable="busy" @click="clearDownloads" /></div>
        <q-list v-if="downloads.length" bordered separator class="q-mt-md"><q-item v-for="item in downloads" :key="item.gid"><q-item-section><q-item-label>{{ item.name || item.gid }}</q-item-label><q-item-label caption>{{ item.status || item.state }} · {{ item.completedLength || 0 }} / {{ item.totalLength || 0 }}</q-item-label></q-item-section><q-item-section side><q-btn flat color="negative" :label="t('neo.auto2.k4d0b4688c7')" :disable="busy" @click="cancelDownload(item)" /></q-item-section></q-item></q-list>
      </section>
      <SettingsPanel class="q-mt-lg" :settings="downloadSettings" category="all" :busy="busy" @save="save" @reset="reset" />
    </div>
    <div v-else-if="tab==='mirrors'" class="q-mt-md">
      <section class="settings-card">
        <h6>{{ t('neo.auto2.kc82dda0236') }}</h6>
        <q-select dark outlined emit-value map-options :model-value="number('DownloadMirrorType')" :options="mirrorTypes" :label="t('neo.auto2.k01b7899496')" @update:model-value="saveKey('DownloadMirrorType',$event)" />
        <q-banner v-if="number('DownloadMirrorType') === 2" class="bg-blue-grey-9 q-mt-md">{{ t('neo.auto2.kccbd1ffb95') }}</q-banner>
        <template v-if="number('DownloadMirrorType') === 2">
          <q-input dark outlined v-model="mirrorDraft.MojangDownloadsMirrorURL" :label="t('neo.auto2.k2fa65ccaca')" :hint="t('neo.auto2.kd5d8edd12c')" @blur="saveKey('MojangDownloadsMirrorURL',mirrorDraft.MojangDownloadsMirrorURL)" />
          <q-input dark outlined v-model="mirrorDraft.LibrariesURL" :label="t('neo.auto2.k3346e12074')" @blur="saveKey('LibrariesURL',mirrorDraft.LibrariesURL)" />
          <q-input dark outlined v-model="mirrorDraft.ResourceURLOverride" :label="t('neo.auto2.ke322863a69')" @blur="saveKey('ResourceURLOverride',mirrorDraft.ResourceURLOverride)" />
          <q-input dark outlined v-model="mirrorDraft.LegacyFMLLibsURLOverride" :label="t('neo.auto2.kce3f74d643')" @blur="saveKey('LegacyFMLLibsURLOverride',mirrorDraft.LegacyFMLLibsURLOverride)" />
        </template>
      </section>
      <SettingsPanel class="q-mt-lg" :settings="mirrorOtherSettings" category="all" :busy="busy" @save="save" @reset="reset" />
    </div>
    <div v-else-if="tab==='integrations'" class="q-mt-md"><SettingsPanel :settings="settings" category="advanced" :busy="busy" @save="save" @reset="reset" /><q-banner class="bg-blue-grey-9 q-mt-lg">{{ t('neo.auto2.kbe26e1af41') }}</q-banner><div class="row q-col-gutter-md q-mt-md"><q-select class="col" dark outlined v-model="helper" :options="['authlib-injector','nide8auth']" :label="t('neo.auto2.k34bc6b32e7')" /><q-btn class="q-mt-sm" flat :label="t('neo.auto2.k4785148aa3')" :disable="busy" @click="helperStatus" /></div><div class="row q-col-gutter-md q-mt-md"><q-select class="col" dark outlined v-model="externalTool" :options="['jprofiler','jvisualvm','mcedit','curseforge']" :label="t('neo.auto2.k1a818fb254')" /><q-input class="col" dark outlined v-model="externalPath" :label="t('neo.auto2.kebade72c21')" /><q-btn class="q-mt-sm" flat :label="t('neo.auto2.k63abb84f10')" :disable="busy || !externalPath" @click="externalCheck" /></div><q-list v-if="integrationResult" bordered class="q-mt-md"><q-item><q-item-section><pre>{{ JSON.stringify(integrationResult,null,2) }}</pre></q-item-section></q-item></q-list></div>
    <div v-else-if="tab==='network'" class="q-mt-md"><LauncherControlPanel :busy="busy" :execute="execute" /><SettingsPanel class="q-mt-lg" :settings="settings" category="network" :busy="busy" @save="save" @reset="reset" /></div>
    <div v-else class="q-mt-md"><LauncherControlPanel :busy="busy" :execute="execute" /><SettingsPanel class="q-mt-lg" :settings="settings" category="qt-ui" :busy="busy" @save="save" @reset="reset" /></div>
  </div>
</template>

<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { computed, reactive, ref, watch } from 'vue';
import SettingsPanel from './SettingsPanel.vue';
import JavaPanel from './JavaPanel.vue';
import LauncherControlPanel from './LauncherControlPanel.vue';
const props=defineProps<{settings:any[];busy:boolean;scope:Record<string,unknown>;execute:(op:string,p?:Record<string,unknown>)=>Promise<any>}>();
const emit=defineEmits<{changed:[]}>();
const tab=ref('general'),downloads=ref<any[]>([]),integrationResult=ref<any>(null),helper=ref('authlib-injector'),externalTool=ref('jprofiler'),externalPath=ref('');
const backends=[{label:t('neo.auto2.ka626fe7700'),value:0},{label:'Aria2',value:1},{label:t('neo.auto2.k6a21c71e5f'),value:2}];
const mirrorTypes=[{label:t('neo.auto2.k212e81e6d0'),value:0},{label:t('neo.auto2.kf0aba8a42d'),value:1},{label:t('neo.auto2.kfe7276cd17'),value:2}];
const setting=(key:string)=>props.settings.find(x=>x.key===key); const value=(key:string)=>setting(key)?.value; const bool=(key:string)=>Boolean(value(key)); const text=(key:string)=>String(value(key)??''); const number=(key:string)=>Number(value(key)??0);
const mirrorKeys=new Set(['ModMetadataDisabled','ModDependenciesDisabled','SkipModpackUpdatePrompt','ModDownloadLoaders','OverrideModDownloadLoaders','CurseForgeDownloadBrowser','ShowModIncompat','FallbackMRBlockedMods','MetaURLOverride','ResourceURLOverride','DownloadMirrorType','ModrinthMirror','CurseForgeMirror','ModrinthMirrorURL','CurseForgeMirrorURL','LibrariesURL','LegacyFMLLibsURLOverride','MojangDownloadsMirrorURL','CFKeyOverride','FlameKeyOverride','ModrinthToken']);
const mirrorControlKeys=new Set(['DownloadMirrorType','MojangDownloadsMirrorURL','LibrariesURL','ResourceURLOverride','LegacyFMLLibsURLOverride']);
const downloadSettings=computed(()=>props.settings.filter(x=>x.category==='downloads'));
const mirrorSettings=computed(()=>downloadSettings.value.filter(x=>mirrorKeys.has(x.key)));
const mirrorOtherSettings=computed(()=>mirrorSettings.value.filter(x=>!mirrorControlKeys.has(x.key)));
const mirrorDraft=reactive({ MojangDownloadsMirrorURL:'', LibrariesURL:'', ResourceURLOverride:'', LegacyFMLLibsURLOverride:'' });
watch(()=>props.settings, items=>{ for(const key of Object.keys(mirrorDraft)) mirrorDraft[key as keyof typeof mirrorDraft]=text(key); }, { immediate:true });
async function save(key:string,value:any){await props.execute('settings.set',{...props.scope,key,value});emit('changed');}
function saveKey(key:string,value:any){void save(key,value);}
function reset(key:string){void props.execute('settings.reset',{...props.scope,key}).then(()=>emit('changed'));}
async function loadDownloads(){downloads.value=await props.execute('aria2.downloads');}
async function clearDownloads(){await props.execute('aria2.clear-finished');await loadDownloads();}
async function cancelDownload(item:any){await props.execute('aria2.cancel',{gid:item.gid});await loadDownloads();}
async function helperStatus(){integrationResult.value=await props.execute('authentication.helper.status',{helper:helper.value});}
async function externalCheck(){integrationResult.value=await props.execute('external-tool.check',{tool:externalTool.value,path:externalPath.value});}
</script>

<style scoped>
.launcher-settings-panel{max-width:1120px}.settings-card{padding:20px;border:1px solid var(--neo-border);border-radius:16px;background:var(--neo-panel)}.settings-card h6{margin:0 0 14px;font-size:18px}.settings-card .q-toggle{display:block;margin:7px 0}.settings-card .q-input,.settings-card .q-select{margin-top:10px}pre{white-space:pre-wrap;overflow-wrap:anywhere}
</style>
