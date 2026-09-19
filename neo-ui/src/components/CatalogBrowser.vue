<template>
 <section class="catalog">
  <div class="row q-col-gutter-sm q-my-sm"><q-select class="col-12 col-sm-4" dark outlined v-model="provider" :options="providers" label="下载来源" :disable="busy" @update:model-value="reset"/><q-input class="col" dark outlined v-model="query" label="搜索名称" @keyup.enter="search(0)"/><q-btn class="q-ml-sm" color="primary" label="搜索" :disable="busy" @click="search(0)"/></div>
  <div v-if="!packs" class="row q-col-gutter-sm q-mb-md"><q-input class="col" dark outlined v-model="gameVersion" label="Minecraft 版本筛选"/><q-select class="col" dark outlined v-model="loader" :options="['','forge','fabric','neoforge','quilt','liteloader']" label="加载器筛选"/></div>
  <q-linear-progress v-if="busy" indeterminate/>
  <q-banner v-if="error" class="bg-red-9 q-my-sm">{{ error }}</q-banner>
  <template v-if="project">
   <q-btn flat icon="arrow_back" label="返回搜索结果" :disable="busy" @click="project=null"/><h6>{{ project.name }}</h6><p>{{ project.description }}</p>
   <q-input v-if="packs" dark outlined v-model="instanceName" label="新实例名称" class="q-mb-md"/>
   <q-select dark outlined v-model="version" :options="versions.map(v=>({label:v.name||v.version||v.versionId,value:v.versionId}))" emit-value map-options label="选择版本"/>
   <q-checkbox v-if="!packs" dark v-model="optional" label="同时安装可选依赖"/>
   <q-btn class="q-mt-md" color="primary" label="安装所选版本" :disable="busy || !version || (packs && !instanceName.trim())" @click="install"/>
  </template>
  <template v-else><q-list separator><q-item v-for="item in results" :key="item.projectId" clickable :disable="busy" @click="select(item)"><q-item-section avatar><q-icon name="extension" size="28px"/></q-item-section><q-item-section><q-item-label>{{ item.name }}</q-item-label><q-item-label caption class="text-grey-5">{{ item.description }}</q-item-label></q-item-section><q-item-section side><q-icon name="chevron_right"/></q-item-section></q-item></q-list>
   <p v-if="searched && !results.length && !busy">没有找到符合条件的项目。</p>
   <div v-if="searched && ['modrinth','curseforge'].includes(provider)" class="row justify-between q-mt-md"><q-btn flat label="上一页" :disable="busy || offset===0" @click="search(Math.max(0,offset-25))"/><q-btn flat label="下一页" :disable="busy || !results.length" @click="search(offset+25)"/></div>
  </template>
 </section>
</template>
<script setup lang="ts">
import { ref, watch, computed } from 'vue';
const props=defineProps<{packs?:boolean;instance?:string;kind?:string;minecraftVersion?:string;busy:boolean;execute:(op:string,p:Record<string,unknown>)=>Promise<any>}>();
const emit=defineEmits<{installed:[]}>();
const providers=computed(()=>props.packs?['modrinth','curseforge','atlauncher','ftb','technic','legacy-ftb']:['modrinth','curseforge']);
const provider=ref('modrinth'),query=ref(''),results=ref<any[]>([]),project=ref<any>(null),versions=ref<any[]>([]),version=ref(''),error=ref(''),gameVersion=ref(props.minecraftVersion||''),loader=ref(''),instanceName=ref(''),optional=ref(false),offset=ref(0),searched=ref(false);
watch(()=>[props.instance,props.kind,props.packs],reset);
function reset(){results.value=[];project.value=null;versions.value=[];version.value='';searched.value=false;error.value='';gameVersion.value=props.minecraftVersion||'';}
function params(){const p:Record<string,unknown>={provider:provider.value};if(!props.packs){p.kind=props.kind||'mods';if(gameVersion.value)p.minecraftVersion=gameVersion.value;if(loader.value)p.loaders=[loader.value];}return p;}
async function search(start:number){if(props.busy)return;error.value='';project.value=null;try{const r=await props.execute(props.packs?'modpack.search':'resource.search',{...params(),query:query.value,offset:start});const list=Array.isArray(r)?r:(r.results||r.projects||r.modpacks||[]);results.value=list.map((v:any)=>({...v,projectId:String(v.projectId||v.slug||v.id||''),name:v.displayName||v.name,description:v.description||v.synopsis||''}));offset.value=start;searched.value=true;}catch(e){error.value=String(e);}}
async function select(item:any){if(props.busy)return;error.value='';project.value=item;versions.value=[];version.value='';instanceName.value=item.name;try{const r=await props.execute(props.packs?'modpack.versions':'resource.versions',{...params(),projectId:item.projectId});versions.value=Array.isArray(r)?r:(r.versions||[]);}catch(e){error.value=String(e);}}
async function install(){if(props.busy)return;error.value='';try{await props.execute(props.packs?'modpack.install':'resource.install-with-dependencies',{...params(),projectId:project.value.projectId,versionId:version.value,...(props.packs?{name:instanceName.value}:{instance:props.instance,includeOptional:optional.value})});emit('installed');}catch(e){error.value=String(e);}}
</script>
