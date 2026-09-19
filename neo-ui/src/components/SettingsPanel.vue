<template>
 <div class="settings-panel">
  <q-input dark outlined v-model="search" label="搜索设置名称或键" clearable class="q-mb-md"/>
  <q-tabs v-model="group" align="left" outside-arrows mobile-arrows><q-tab v-for="g in groups" :key="g" :name="g" :label="g"/></q-tabs>
  <p class="text-grey-5">实例设置中的“覆盖全局设置”开关决定该组设置是否生效。每项单独保存，恢复默认会交给后端处理。</p>
  <q-card v-for="s in visible" :key="s.key" class="setting-row q-mb-sm">
   <q-card-section><div class="row items-center justify-between"><div><strong>{{ metadata(s).label }}</strong><div class="text-caption text-grey-5">{{ s.key }} · {{ s.isDefault ? '默认值' : '已修改' }}</div></div><q-btn flat label="恢复默认" :disable="busy" @click="$emit('reset',s.key)"/></div>
    <q-toggle v-if="typeof s.value==='boolean'" dark v-model="draft[s.key]" :disable="busy || s.redacted" :label="draft[s.key]?'启用':'关闭'"/>
    <q-input v-else-if="typeof s.value==='number'" dark outlined type="number" v-model.number="draft[s.key]" :min="metadata(s).minimum" :max="metadata(s).maximum" :disable="busy || s.redacted" class="q-mt-sm"/>
    <q-input v-else dark outlined v-model="draft[s.key]" :type="s.redacted?'password':'textarea'" autogrow :disable="busy || s.redacted" class="q-mt-sm"/>
    <div class="row justify-end q-mt-sm"><q-btn unelevated color="primary" label="保存" :disable="busy || s.redacted" @click="save(s)"/></div>
   </q-card-section>
  </q-card>
  <p v-if="!visible.length" class="text-grey-5">没有符合条件的设置。</p>
  <q-banner v-if="validation" class="bg-red-9">{{ validation }}</q-banner>
 </div>
</template>
<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { settingMetadata } from '../settings-schema';
const props=defineProps<{settings:any[];busy:boolean}>();
const emit=defineEmits<{save:[key:string,value:unknown];reset:[key:string]}>();
const search=ref(''),group=ref('全部'),validation=ref(''),draft=ref<Record<string,any>>({});
function metadata(s:any):{label:string;group:string;minimum?:number;maximum?:number}{return settingMetadata[s.key] || {label:s.key.startsWith('Override')?'覆盖全局设置 · '+s.key.substring(8):s.key,group:s.key.startsWith('Override')?'覆盖全局设置':'其他设置'};}
const groups=computed(()=>['全部',...new Set(props.settings.map(s=>metadata(s).group))]);
const visible=computed(()=>props.settings.filter(s=>(group.value==='全部'||metadata(s).group===group.value)&&`${s.key} ${metadata(s).label}`.toLowerCase().includes((search.value||'').toLowerCase())));
watch(()=>props.settings,items=>{draft.value=Object.fromEntries(items.map(s=>[s.key,typeof s.value==='object'?JSON.stringify(s.value,null,2):s.value]));},{immediate:true});
function save(s:any){validation.value='';let value=draft.value[s.key];try{if(typeof s.value==='object')value=JSON.parse(value);if(typeof s.value==='number'){const m=metadata(s);if(!Number.isFinite(value)||(m.minimum!==undefined&&value<m.minimum)||(m.maximum!==undefined&&value>m.maximum))throw new Error('数值超出允许范围');}emit('save',s.key,value);}catch(e){validation.value=e instanceof Error?e.message:String(e);}}
</script>
<style scoped>.setting-row{background:var(--neo-panel);border:1px solid var(--neo-border)}.settings-panel{max-width:960px}</style>
