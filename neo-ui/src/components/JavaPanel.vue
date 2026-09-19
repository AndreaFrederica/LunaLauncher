<template>
 <q-expansion-item label="Java 检测、诊断与下载" icon="code" class="q-mb-lg">
  <div class="q-pa-md">
   <div class="row q-gutter-sm"><q-btn label="检测本机 Java" :disable="busy" @click="detect"/><q-btn label="测试当前设置" :disable="busy" @click="diagnose"/></div>
   <q-list><q-item v-for="j in installed" :key="j.path"><q-item-section>{{ j.version }} · {{ j.architecture }}<q-item-label caption class="text-grey-5">{{ j.path }}</q-item-label></q-item-section><q-item-section side><q-btn label="使用" :disable="busy" @click="select(j.path)"/></q-item-section></q-item></q-list>
   <q-banner v-if="diagnostic" class="bg-grey-9 q-my-md">{{ diagnostic.usable?'Java 可以运行':'Java 不可用' }} · {{ diagnostic.compatible?'与实例兼容':'请检查兼容性' }}<pre>{{ diagnostic.probeOutput }}{{ diagnostic.probeError }}</pre><div v-for="r in diagnostic.recommendations" :key="r">{{ r }}</div></q-banner>
   <h6>下载 Java</h6>
   <q-select dark outlined v-model="provider" :options="providers" emit-value map-options label="发行商" :disable="busy" @update:model-value="versions=[];packages=[];version=''"/>
   <div class="row q-gutter-sm q-my-md"><q-select class="col" dark outlined v-model="version" :options="versions" label="Java 版本" @update:model-value="packages=[]"/><q-btn label="加载版本" :disable="busy" @click="loadVersions"/><q-btn label="查找安装包" :disable="busy || !version" @click="loadPackages"/></div>
   <q-list><q-item v-for="p in packages" :key="p.url"><q-item-section>{{ p.name }} · {{ p.version }}<q-item-label caption class="text-grey-5">{{ p.vendor }} · {{ p.runtimeOS }}</q-item-label></q-item-section><q-item-section side><q-btn label="安装" :disable="busy" @click="install(p)"/></q-item-section></q-item></q-list>
   <q-banner v-if="error" class="bg-red-9">{{ error }}</q-banner>
  </div>
 </q-expansion-item>
</template>
<script setup lang="ts">
import { ref, watch } from 'vue';
const props=defineProps<{scope:Record<string,unknown>;settings:any[];busy:boolean;execute:(op:string,p:Record<string,unknown>)=>Promise<any>}>();
const emit=defineEmits<{changed:[]}>();
const installed=ref<any[]>([]),diagnostic=ref<any>(null),error=ref(''),provider=ref('net.minecraft.java'),version=ref(''),versions=ref<string[]>([]),packages=ref<any[]>([]);
const providers=[{label:'Mojang',value:'net.minecraft.java'},{label:'Adoptium',value:'net.adoptium.java'},{label:'Azul',value:'com.azul.java'},{label:'IBM',value:'com.ibm.java'}];
watch(()=>props.scope.instance,()=>{diagnostic.value=null;error.value='';});
async function perform(action:()=>Promise<void>){if(props.busy)return;error.value='';try{await action();}catch(e){error.value=String(e);}}
function detect(){return perform(async()=>{installed.value=await props.execute('java.refresh',{});});}
function diagnose(){return perform(async()=>{diagnostic.value=await props.execute('java.diagnose',{...props.scope,arguments:props.settings.find(s=>s.key==='JvmArgs')?.value||''});});}
function select(path:string){return perform(async()=>{await props.execute('java.select',{...props.scope,path});emit('changed');});}
function loadVersions(){return perform(async()=>{const r=await props.execute('component.versions',{uid:provider.value});versions.value=r.versions.map((v:any)=>v.version);});}
function loadPackages(){return perform(async()=>{packages.value=await props.execute('java.versions',{uid:provider.value,version:version.value});});}
function install(metadata:any){return perform(async()=>{await props.execute('java.install',{metadata});installed.value=await props.execute('java.refresh',{});});}
</script>
<style scoped>pre{white-space:pre-wrap;overflow-wrap:anywhere}</style>
