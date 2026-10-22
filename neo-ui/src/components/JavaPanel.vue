<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
 <q-expansion-item label="Java 检测、诊断与下载" icon="code" class="q-mb-lg">
 <div class="java-panel-body">
   <div class="java-actions"><q-btn color="primary" label="扫描本机 Java" icon="search" :disable="busy || localBusy" @click="detect"/><q-btn outline label="测试当前设置" icon="fact_check" :disable="busy || localBusy" @click="diagnose"/><q-btn outline label="自动修复" icon="build" :disable="busy || localBusy" @click="repair"/><q-toggle v-model="automatic" label="自动选择兼容运行时" :disable="busy || localBusy" @update:model-value="toggleAutomatic"/></div>
   <q-banner v-if="diagnostic" class="java-diagnostic" :class="{ok:diagnostic.usable && diagnostic.compatible}"><strong>{{ diagnostic.usable?'Java 可以运行':'Java 不可用' }}</strong><span> · {{ diagnostic.compatible?'与实例兼容':'请检查兼容性' }}</span><pre>{{ diagnostic.probeOutput }}{{ diagnostic.probeError }}</pre><div v-for="r in diagnostic.recommendations" :key="r">{{ r }}</div></q-banner>
   <q-list bordered separator class="java-runtime-list"><q-item v-for="j in installed" :key="j.path"><q-item-section avatar><q-icon name="coffee" color="primary"/><q-badge v-if="j.is64Bit" color="positive">64 位</q-badge></q-item-section><q-item-section><q-item-label>{{ j.version || j.id || '未知版本' }} · {{ j.architecture || j.arch || '未知架构' }}</q-item-label><q-item-label caption>{{ j.path }}</q-item-label></q-item-section><q-item-section side><div class="runtime-actions"><q-btn flat color="primary" label="使用" :disable="busy || localBusy" @click="select(j.path)"/><q-btn flat color="negative" icon="delete_outline" :disable="busy || localBusy || !j.managed" @click="remove(j)"/></div></q-item-section></q-item><q-item v-if="!installed.length"><q-item-section class="text-grey-6">尚未扫描 Java。点击“扫描本机 Java”读取运行时列表。</q-item-section></q-item></q-list>
   <h6>下载 Java</h6>
   <q-select dark outlined v-model="provider" :options="providers" emit-value map-options label="发行商" :disable="busy" @update:model-value="versions=[];packages=[];version=''"/>
   <div class="row q-gutter-sm q-my-md"><q-select class="col" dark outlined v-model="version" :options="versions" label="Java 版本" @update:model-value="packages=[]"/><q-btn label="加载版本" :disable="busy" @click="loadVersions"/><q-btn label="查找安装包" :disable="busy || !version" @click="loadPackages"/></div>
   <q-list><q-item v-for="p in packages" :key="p.url"><q-item-section>{{ p.name }} · {{ p.version }}<q-item-label caption class="text-grey-5">{{ p.vendor }} · {{ p.runtimeOS }}</q-item-label></q-item-section><q-item-section side><q-btn label="安装" :disable="busy" @click="install(p)"/></q-item-section></q-item></q-list>
   <q-banner v-if="error" class="java-error">{{ error }}</q-banner>
  </div>
 </q-expansion-item>
</template>
<script setup lang="ts">
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
function remove(runtime:any){return perform(async()=>{if(!window.confirm(`确定删除托管 Java ${runtime.version || runtime.path} 吗？`))return;await props.execute('java.remove',{name:runtime.name,path:runtime.path,confirm:true});installed.value=await props.execute('java.refresh',{});});}
</script>
<style scoped>
.java-panel-body{padding:18px;color:#4b6578}.java-actions{display:flex;align-items:center;gap:9px;flex-wrap:wrap;padding-bottom:14px;border-bottom:1px solid #dce6ec}.java-runtime-list{margin-top:16px;border-color:#d7e2e9}.runtime-actions{display:flex;align-items:center;gap:3px}.java-diagnostic{margin:14px 0;border:1px solid #e0c995;background:#fff9e8!important;color:#785e27}.java-diagnostic.ok{border-color:#b9dcc5;background:#effaf2!important;color:#3f7950}.java-diagnostic pre{max-height:140px;margin:8px 0;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;font:11px/1.5 Consolas,monospace}.java-error{margin-top:14px;border:1px solid #efc5c5;background:#fff1f1!important;color:#aa5b5b}.java-panel-body :deep(.q-item__label--caption){color:#899aa6}.java-panel-body :deep(.q-field--dark .q-field__control){background:#fff}.java-panel-body :deep(.q-field--dark .q-field__native),.java-panel-body :deep(.q-field--dark .q-field__label){color:#526a7b}
</style>
