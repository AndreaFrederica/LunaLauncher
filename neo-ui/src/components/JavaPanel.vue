<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
 <q-expansion-item :label="t('neo.auto2.kce6ed757ab')" icon="code" class="q-mb-lg">
 <div class="java-panel-body">
   <div class="java-actions"><q-btn color="primary" :label="t('neo.auto2.k2872e1bacb')" icon="search" :disable="busy || localBusy" @click="detect"/><q-btn outline :label="t('neo.auto2.k2d9d820da3')" icon="fact_check" :disable="busy || localBusy" @click="diagnose"/><q-btn outline :label="t('neo.auto2.kd3dcefea0d')" icon="build" :disable="busy || localBusy" @click="repair"/><q-toggle v-model="automatic" :label="t('neo.auto2.kff437da97e')" :disable="busy || localBusy" @update:model-value="toggleAutomatic"/></div>
   <q-banner v-if="diagnostic" class="java-diagnostic" :class="{ok:diagnostic.usable && diagnostic.compatible}"><strong>{{ diagnostic.usable?t('neo.auto2.k7cf0257485'):t('neo.auto2.k35ab8a7621') }}</strong><span> · {{ diagnostic.compatible?t('neo.auto2.k141e91aa5c'):t('neo.auto2.k9167236231') }}</span><pre>{{ diagnostic.probeOutput }}{{ diagnostic.probeError }}</pre><div v-for="r in diagnostic.recommendations" :key="r">{{ r }}</div></q-banner>
   <q-list bordered separator class="java-runtime-list"><q-item v-for="j in installed" :key="j.path"><q-item-section avatar><q-icon name="coffee" color="primary"/><q-badge v-if="j.is64Bit" color="positive">{{ t('neo.auto2.k83b7248e7c') }}</q-badge></q-item-section><q-item-section><q-item-label>{{ j.version || j.id || t('neo.auto2.k59b35caf64') }} · {{ j.architecture || j.arch || t('neo.auto2.kb130e8c87d') }}</q-item-label><q-item-label caption>{{ j.path }}</q-item-label></q-item-section><q-item-section side><div class="runtime-actions"><q-btn flat color="primary" :label="t('neo.auto2.k0e2d3a3c09')" :disable="busy || localBusy" @click="select(j.path)"/><q-btn flat color="negative" icon="delete_outline" :disable="busy || localBusy || !j.managed" @click="remove(j)"/></div></q-item-section></q-item><q-item v-if="!installed.length"><q-item-section class="text-grey-6">{{ t('neo.auto2.kbccc7d3acb') }}</q-item-section></q-item></q-list>
   <h6>{{ t('neo.auto2.k2522623fd9') }}</h6>
   <q-select dark outlined v-model="provider" :options="providers" emit-value map-options :label="t('neo.auto2.kcec1ec54d9')" :disable="busy" @update:model-value="versions=[];packages=[];version=''"/>
   <div class="row q-gutter-sm q-my-md"><q-select class="col" dark outlined v-model="version" :options="versions" :label="t('neo.auto2.k93d5afd8ac')" @update:model-value="packages=[]"/><q-btn :label="t('neo.auto2.k68f7347528')" :disable="busy" @click="loadVersions"/><q-btn :label="t('neo.auto2.k2da10d49dc')" :disable="busy || !version" @click="loadPackages"/></div>
   <q-list><q-item v-for="p in packages" :key="p.url"><q-item-section>{{ p.name }} · {{ p.version }}<q-item-label caption class="text-grey-5">{{ p.vendor }} · {{ p.runtimeOS }}</q-item-label></q-item-section><q-item-section side><q-btn :label="t('neo.auto2.k087db63ab1')" :disable="busy" @click="install(p)"/></q-item-section></q-item></q-list>
   <q-banner v-if="error" class="java-error">{{ error }}</q-banner>
  </div>
 </q-expansion-item>
</template>
<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { ref, watch } from 'vue';
const props=defineProps<{scope:Record<string,unknown>;settings:any[];busy:boolean;execute:(op:string,p:Record<string,unknown>)=>Promise<any>}>();
const emit=defineEmits<{changed:[]}>();
const installed=ref<any[]>([]),diagnostic=ref<any>(null),error=ref(''),provider=ref('net.minecraft.java'),version=ref(''),versions=ref<string[]>([]),packages=ref<any[]>([]),localBusy=ref(false),automatic=ref(Boolean(props.settings.find((s:any)=>s.key==='AutomaticJava')?.value));
const providers=[{label:'Mojang',value:'net.minecraft.java'},{label:'Adoptium',value:'net.adoptium.java'},{label:'Azul',value:'com.azul.java'},{label:'IBM',value:'com.ibm.java'}];
watch(()=>props.scope.instance,()=>{diagnostic.value=null;error.value='';});
async function perform(action:()=>Promise<void>){if(props.busy||localBusy.value)return;error.value='';localBusy.value=true;try{await action();}catch(e){error.value=e instanceof Error?e.message:String(e);}finally{localBusy.value=false;}}
function detect(){return perform(async()=>{installed.value=await props.execute('java.refresh',{});});}
function diagnose(){return perform(async()=>{diagnostic.value=await props.execute('java.diagnose',{...props.scope,arguments:props.settings.find(s=>s.key==='JvmArgs')?.value||''});});}
function repair(){return perform(async()=>{diagnostic.value=await props.execute('java.repair',props.scope);installed.value=await props.execute('java.refresh',{});});}
function toggleAutomatic(value:boolean){return perform(async()=>{await props.execute('settings.set',{...props.scope,key:'AutomaticJava',value});emit('changed');});}
function select(path:string){return perform(async()=>{await props.execute('java.select',{...props.scope,path});emit('changed');});}
function loadVersions(){return perform(async()=>{const r=await props.execute('component.versions',{uid:provider.value});versions.value=r.versions.map((v:any)=>v.version);});}
function loadPackages(){return perform(async()=>{packages.value=await props.execute('java.versions',{uid:provider.value,version:version.value});});}
function install(metadata:any){return perform(async()=>{await props.execute('java.install',{metadata});installed.value=await props.execute('java.refresh',{});});}
function remove(runtime:any){return perform(async()=>{if(!window.confirm(t('neo.extra.deleteJava',{name:runtime.version || runtime.path})))return;await props.execute('java.remove',{name:runtime.name,path:runtime.path,confirm:true});installed.value=await props.execute('java.refresh',{});});}
</script>
<style scoped>
.java-panel-body{padding:18px;color:#4b6578}.java-actions{display:flex;align-items:center;gap:9px;flex-wrap:wrap;padding-bottom:14px;border-bottom:1px solid #dce6ec}.java-runtime-list{margin-top:16px;border-color:#d7e2e9}.runtime-actions{display:flex;align-items:center;gap:3px}.java-diagnostic{margin:14px 0;border:1px solid #e0c995;background:#fff9e8!important;color:#785e27}.java-diagnostic.ok{border-color:#b9dcc5;background:#effaf2!important;color:#3f7950}.java-diagnostic pre{max-height:140px;margin:8px 0;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;font:11px/1.5 Consolas,monospace}.java-error{margin-top:14px;border:1px solid #efc5c5;background:#fff1f1!important;color:#aa5b5b}.java-panel-body :deep(.q-item__label--caption){color:#899aa6}.java-panel-body :deep(.q-field--dark .q-field__control){background:#fff}.java-panel-body :deep(.q-field--dark .q-field__native),.java-panel-body :deep(.q-field--dark .q-field__label){color:#526a7b}
</style>
