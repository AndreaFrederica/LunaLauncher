<template>
  <q-expansion-item label="检查资源更新" icon="update" class="q-my-md">
    <div class="q-pa-md"><div class="row q-gutter-sm"><q-btn color="primary" label="检查更新" :disable="busy" @click="check"/><q-btn flat label="放弃计划" :disable="busy || !planId" @click="discard"/></div><q-banner v-if="message" class="bg-grey-9 q-my-md">{{ message }}</q-banner><q-list bordered separator v-if="items.length"><q-item v-for="item in items" :key="item.id||item.fileName"><q-item-section><q-checkbox dark v-model="selected" :val="item.id||item.fileName"/><q-item-label>{{ item.name || item.fileName || item.id }}</q-item-label><q-item-label caption>{{ item.currentVersion || item.version || '' }} → {{ item.latestVersion || item.newVersion || '' }}</q-item-label></q-item-section></q-item></q-list><q-btn v-if="items.length" color="primary" label="应用选中更新" :disable="busy || !selected.length" class="q-mt-md" @click="apply"/><q-banner v-if="error" class="bg-red-9 q-mt-md">{{ error }}</q-banner></div>
  </q-expansion-item>
</template>
<script setup lang="ts">
import { ref } from 'vue';
const props=defineProps<{instance:string;kind:string;busy:boolean;execute:(op:string,p?:Record<string,unknown>)=>Promise<any>}>();
const items=ref<any[]>([]),selected=ref<string[]>([]),planId=ref(''),message=ref(''),error=ref('');
async function run(action:()=>Promise<any>){error.value='';try{return await action();}catch(e){error.value=String(e);}}
async function check(){await run(async()=>{const r=await props.execute('resource.updates.check',{instance:props.instance,kind:props.kind});planId.value=r.planId||r.id||'';items.value=r.items||r.updates||[];selected.value=items.value.map(x=>x.id||x.fileName);message.value=items.value.length?`发现 ${items.value.length} 项更新`:'没有可用更新';});}
async function apply(){await run(async()=>{const r=await props.execute('resource.updates.apply',{planId:planId.value,items:selected.value});message.value=r.message||'更新已应用';items.value=[];selected.value=[];planId.value='';});}
async function discard(){await run(async()=>{if(planId.value)await props.execute('resource.updates.discard',{planId:planId.value});items.value=[];selected.value=[];planId.value='';});}
</script>
