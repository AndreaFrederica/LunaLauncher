<template>
  <div class="instance-settings">
    <q-tabs v-model="tab" align="left" outside-arrows mobile-arrows>
      <q-tab name="components" label="组件" icon="layers" />
      <q-tab name="runtime" label="运行参数" icon="tune" />
      <q-tab name="tools" label="工具" icon="construction" />
    </q-tabs>
    <div v-if="tab==='components'" class="q-mt-md">
      <q-banner v-if="error" class="bg-red-9 q-mb-md">{{ error }}</q-banner>
      <q-list bordered separator><q-item v-for="(component,index) in components" :key="component.id"><q-item-section avatar><q-icon :name="component.enabled===false?'visibility_off':'layers'" /></q-item-section><q-item-section><q-item-label>{{ component.name || component.id }} · {{ component.version || '未安装' }}</q-item-label><q-item-label caption>{{ component.id }} · {{ component.custom?'自定义':'标准组件' }}</q-item-label><q-item-label v-for="problem in component.problems||[]" :key="problem.description" class="text-orange">{{ problem.description }}</q-item-label></q-item-section><q-item-section side><div class="row q-gutter-xs"><q-btn flat label="上移" :disable="busy || index===0" @click="move(component,-1)"/><q-btn flat label="下移" :disable="busy || index===components.length-1" @click="move(component,1)"/><q-btn flat :label="component.enabled===false?'启用':'禁用'" :disable="busy || !component.canDisable" @click="toggle(component)"/><q-btn flat label="自定义" :disable="busy || !component.canCustomize" @click="customize(component)"/><q-btn flat color="negative" label="删除" :disable="busy || !component.canRemove" @click="remove(component)"/></div></q-item-section></q-item></q-list>
      <div class="row q-gutter-sm q-mt-md"><q-input class="col" dark outlined v-model="customPath" label="自定义组件 / Jar / Java agent 路径"/><q-btn color="primary" label="安装自定义组件" :disable="busy || !customPath" @click="installCustom" /></div>
    </div>
    <div v-else-if="tab==='runtime'" class="q-mt-md"><p class="text-grey-5">完整的实例覆盖项在下方设置编辑器中按类型保存；Java 选择和诊断也位于同一设置页面。</p><q-btn color="primary" label="校验并修复文件" :disable="busy" @click="execute('instance.verify',{instance})" /><q-btn flat label="创建快捷方式" :disable="busy" class="q-ml-sm" @click="execute('instance.create-shortcut',{instance})" /></div>
    <div v-else class="q-mt-md"><div class="row q-gutter-sm"><q-input class="col" dark outlined v-model="exportPath" label="导出文件路径"/><q-select dark outlined v-model="exportFormat" :options="['launcher','modrinth','curseforge']" label="格式"/><q-btn color="primary" label="导出实例" :disable="busy || !exportPath" @click="exportInstance"/></div><q-btn flat label="打开实例目录" :disable="busy" class="q-mt-md" @click="execute('instance.open-folder',{instance})" /></div>
  </div>
</template>
<script setup lang="ts">
import { onMounted, ref, watch } from 'vue';
const props=defineProps<{instance:string;busy:boolean;execute:(op:string,p?:Record<string,unknown>)=>Promise<any>}>();
const emit=defineEmits<{changed:[]}>();
const tab=ref('components'),components=ref<any[]>([]),error=ref(''),customPath=ref(''),exportPath=ref(''),exportFormat=ref('launcher');
async function load(){error.value='';try{const r=await props.execute('instance.components.list',{instance:props.instance});components.value=r.components||r||[];}catch(e){error.value=String(e);}}
async function mutate(op:string,p:Record<string,unknown>){error.value='';try{await props.execute(op,{instance:props.instance,...p});emit('changed');await load();}catch(e){error.value=String(e);}}
function move(c:any,d:number){return mutate('instance.component.move',{component:c.id,direction:d>0?'down':'up'});}
function toggle(c:any){return mutate('instance.component.set-enabled',{component:c.id,enabled:c.enabled===false});}
async function customize(c:any){if(window.confirm('为该组件创建本地自定义补丁？'))await mutate('instance.component.customize',{component:c.id});}
function remove(c:any){return mutate('instance.component.remove',{component:c.id,confirm:true});}
function installCustom(){return mutate('instance.component.install-custom',{source:customPath.value});}
async function exportInstance(){error.value='';try{await props.execute('instance.export',{instance:props.instance,format:exportFormat.value,output:exportPath.value});}catch(e){error.value=String(e);}}
watch(()=>props.instance,load);onMounted(load);
</script>
<style scoped>.instance-settings{max-width:1100px}.instance-settings :deep(.q-item){min-height:76px}</style>
