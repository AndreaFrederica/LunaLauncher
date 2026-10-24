<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="launcher-settings-panel">
    <q-tabs v-model="tab" align="left" outside-arrows mobile-arrows>
      <q-tab name="general" label="常规与路径" icon="tune" />
      <q-tab name="java" label="Java 管理" icon="code" />
      <q-tab name="downloads" label="下载核心" icon="download" />
      <q-tab name="mirrors" label="镜像与资源" icon="public" />
      <q-tab name="integrations" label="认证与工具" icon="extension" />
      <q-tab name="network" label="网络与界面" icon="settings" />
      <q-tab name="qt" label="Qt 兼容设置" icon="desktop_windows" />
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
        <h6>下载核心</h6>
        <q-select dark outlined emit-value map-options :model-value="value('DownloadBackend')" :options="backends" label="下载实现" @update:model-value="saveKey('DownloadBackend',$event)" />
        <q-toggle dark :model-value="bool('PclDownloadFallbackToQt')" label="专用下载失败时回退 Qt" @update:model-value="saveKey('PclDownloadFallbackToQt',$event)" />
        <div class="row q-col-gutter-md"><q-input class="col" dark outlined type="number" :model-value="number('PclDownloadThreadLimit')" label="专用下载并发线程" @update:model-value="saveKey('PclDownloadThreadLimit',Number($event))" /><q-input class="col" dark outlined type="number" :model-value="number('PclDownloadSpeedLimitKBps')" label="专用下载限速（KB/s，0 为不限）" @update:model-value="saveKey('PclDownloadSpeedLimitKBps',Number($event))" /></div>
      </section>
      <section class="settings-card q-mt-md">
        <h6>Aria2</h6>
        <q-toggle dark :model-value="bool('Aria2Enabled')" label="启用 Aria2" @update:model-value="saveKey('Aria2Enabled',$event)" />
        <q-toggle dark :model-value="bool('Aria2AutoInstall')" label="自动安装 Aria2" @update:model-value="saveKey('Aria2AutoInstall',$event)" />
        <q-toggle dark :model-value="bool('Aria2FallbackToQt')" label="Aria2 失败时回退 Qt" @update:model-value="saveKey('Aria2FallbackToQt',$event)" />
        <div class="row q-col-gutter-md"><q-input class="col" dark outlined type="number" :model-value="number('Aria2MaxConcurrentDownloads')" label="最大并发下载" @update:model-value="saveKey('Aria2MaxConcurrentDownloads',Number($event))" /><q-input class="col" dark outlined type="number" :model-value="number('Aria2MaxConnectionPerServer')" label="每服务器连接数" @update:model-value="saveKey('Aria2MaxConnectionPerServer',Number($event))" /><q-input class="col" dark outlined type="number" :model-value="number('Aria2Split')" label="分段数" @update:model-value="saveKey('Aria2Split',Number($event))" /></div>
        <div class="row q-col-gutter-md"><q-input class="col" dark outlined :model-value="text('Aria2MinSplitSize')" label="最小分段大小" @update:model-value="saveKey('Aria2MinSplitSize',$event)" /><q-input class="col" dark outlined :model-value="text('Aria2FileAllocation')" label="文件分配方式" @update:model-value="saveKey('Aria2FileAllocation',$event)" /><q-input class="col" dark outlined type="number" :model-value="number('Aria2RpcPort')" label="RPC 端口（0 自动）" @update:model-value="saveKey('Aria2RpcPort',Number($event))" /></div>
        <q-toggle dark :model-value="bool('Aria2Continue')" label="断点续传" @update:model-value="saveKey('Aria2Continue',$event)" />
        <q-input dark outlined type="textarea" autogrow :model-value="text('Aria2ExtraArgs')" label="Aria2 额外参数" @update:model-value="saveKey('Aria2ExtraArgs',$event)" />
        <div class="row q-gutter-sm q-mt-md"><q-btn flat label="刷新 Aria2 队列" :disable="busy" @click="loadDownloads" /><q-btn flat label="清理已完成" :disable="busy" @click="clearDownloads" /></div>
        <q-list v-if="downloads.length" bordered separator class="q-mt-md"><q-item v-for="item in downloads" :key="item.gid"><q-item-section><q-item-label>{{ item.name || item.gid }}</q-item-label><q-item-label caption>{{ item.status || item.state }} · {{ item.completedLength || 0 }} / {{ item.totalLength || 0 }}</q-item-label></q-item-section><q-item-section side><q-btn flat color="negative" label="取消" :disable="busy" @click="cancelDownload(item)" /></q-item-section></q-item></q-list>
      </section>
      <SettingsPanel class="q-mt-lg" :settings="downloadSettings" category="all" :busy="busy" @save="save" @reset="reset" />
    </div>
    <div v-else-if="tab==='mirrors'" class="q-mt-md">
      <section class="settings-card">
        <h6>游戏下载源</h6>
        <q-select dark outlined emit-value map-options :model-value="number('DownloadMirrorType')" :options="mirrorTypes" label="Minecraft 资源与依赖" @update:model-value="saveKey('DownloadMirrorType',$event)" />
        <q-banner v-if="number('DownloadMirrorType') === 2" class="bg-blue-grey-9 q-mt-md">自定义镜像需要提供兼容 Mojang / Maven 路径的地址；留空的地址会按主地址的 BMCLAPI 目录结构补全。</q-banner>
        <template v-if="number('DownloadMirrorType') === 2">
          <q-input dark outlined v-model="mirrorDraft.MojangDownloadsMirrorURL" label="Mojang 下载主地址" hint="例如 https://mirror.example.com/" @blur="saveKey('MojangDownloadsMirrorURL',mirrorDraft.MojangDownloadsMirrorURL)" />
          <q-input dark outlined v-model="mirrorDraft.LibrariesURL" label="Libraries Maven 地址（可选）" @blur="saveKey('LibrariesURL',mirrorDraft.LibrariesURL)" />
          <q-input dark outlined v-model="mirrorDraft.ResourceURLOverride" label="资源对象地址（可选）" @blur="saveKey('ResourceURLOverride',mirrorDraft.ResourceURLOverride)" />
          <q-input dark outlined v-model="mirrorDraft.LegacyFMLLibsURLOverride" label="旧版 Forge Libraries 地址（可选）" @blur="saveKey('LegacyFMLLibsURLOverride',mirrorDraft.LegacyFMLLibsURLOverride)" />
        </template>
      </section>
      <SettingsPanel class="q-mt-lg" :settings="mirrorOtherSettings" category="all" :busy="busy" @save="save" @reset="reset" />
    </div>
    <div v-else-if="tab==='integrations'" class="q-mt-md"><SettingsPanel :settings="settings" category="advanced" :busy="busy" @save="save" @reset="reset" /><q-banner class="bg-blue-grey-9 q-mt-lg">Yggdrasil 账户、预设和 authlib-injector 在“账户管理”中维护；这里保留认证辅助工具和外部工具设置。</q-banner><div class="row q-col-gutter-md q-mt-md"><q-select class="col" dark outlined v-model="helper" :options="['authlib-injector','nide8auth']" label="认证辅助工具" /><q-btn class="q-mt-sm" flat label="查看状态" :disable="busy" @click="helperStatus" /></div><div class="row q-col-gutter-md q-mt-md"><q-select class="col" dark outlined v-model="externalTool" :options="['jprofiler','jvisualvm','mcedit','curseforge']" label="外部工具" /><q-input class="col" dark outlined v-model="externalPath" label="工具路径" /><q-btn class="q-mt-sm" flat label="检查路径" :disable="busy || !externalPath" @click="externalCheck" /></div><q-list v-if="integrationResult" bordered class="q-mt-md"><q-item><q-item-section><pre>{{ JSON.stringify(integrationResult,null,2) }}</pre></q-item-section></q-item></q-list></div>
    <div v-else-if="tab==='network'" class="q-mt-md"><LauncherControlPanel :busy="busy" :execute="execute" /><SettingsPanel class="q-mt-lg" :settings="settings" category="network" :busy="busy" @save="save" @reset="reset" /></div>
    <div v-else class="q-mt-md"><LauncherControlPanel :busy="busy" :execute="execute" /><SettingsPanel class="q-mt-lg" :settings="settings" category="qt-ui" :busy="busy" @save="save" @reset="reset" /></div>
  </div>
</template>

<script setup lang="ts">
import { computed, reactive, ref, watch } from 'vue';
import SettingsPanel from './SettingsPanel.vue';
import JavaPanel from './JavaPanel.vue';
import LauncherControlPanel from './LauncherControlPanel.vue';
const props=defineProps<{settings:any[];busy:boolean;scope:Record<string,unknown>;execute:(op:string,p?:Record<string,unknown>)=>Promise<any>}>();
const emit=defineEmits<{changed:[]}>();
const tab=ref('general'),downloads=ref<any[]>([]),integrationResult=ref<any>(null),helper=ref('authlib-injector'),externalTool=ref('jprofiler'),externalPath=ref('');
const backends=[{label:'Qt 原生下载',value:0},{label:'Aria2',value:1},{label:'专用多线程下载',value:2}];
const mirrorTypes=[{label:'官方源',value:0},{label:'BMCLAPI 镜像',value:1},{label:'自定义镜像',value:2}];
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
