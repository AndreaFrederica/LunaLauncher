<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <q-expansion-item :label="t('neo.auto2.k4f2b231d46')" icon="update" class="q-my-md">
    <div class="q-pa-md"><div class="row q-gutter-sm"><q-btn color="primary" :label="t('neo.auto2.ka6df38586d')" :disable="busy" @click="check"/><q-btn flat :label="t('neo.auto2.kc6a3e864d6')" :disable="busy || !planId" @click="discard"/></div><q-banner v-if="message" class="bg-grey-9 q-my-md">{{ message }}</q-banner><q-list bordered separator v-if="items.length"><q-item v-for="item in items" :key="item.id||item.fileName"><q-item-section><q-checkbox dark v-model="selected" :val="item.id||item.fileName"/><q-item-label>{{ item.name || item.fileName || item.id }}</q-item-label><q-item-label caption>{{ item.currentVersion || item.version || '' }} → {{ item.latestVersion || item.newVersion || '' }}</q-item-label></q-item-section></q-item></q-list><q-btn v-if="items.length" color="primary" :label="t('neo.auto2.k5e1d738895')" :disable="busy || !selected.length" class="q-mt-md" @click="apply"/><q-banner v-if="error" class="bg-red-9 q-mt-md">{{ error }}</q-banner></div>
  </q-expansion-item>
</template>
<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { ref } from 'vue';
const props=defineProps<{instance:string;kind:string;busy:boolean;execute:(op:string,p?:Record<string,unknown>)=>Promise<any>}>();
const items=ref<any[]>([]),selected=ref<string[]>([]),planId=ref(''),message=ref(''),error=ref('');
async function run(action:()=>Promise<any>){error.value='';try{return await action();}catch(e){error.value=String(e);}}
async function check(){await run(async()=>{const r=await props.execute('resource.updates.check',{instance:props.instance,kind:props.kind});planId.value=r.planId||r.id||'';items.value=r.items||r.updates||[];selected.value=items.value.map(x=>x.id||x.fileName);message.value=items.value.length?t('neo.extra.updatesFound',{count:items.value.length}):t('neo.auto2.k5f3e4d9f2f');});}
async function apply(){await run(async()=>{const r=await props.execute('resource.updates.apply',{planId:planId.value,items:selected.value});message.value=r.message||t('neo.auto2.k264e824404');items.value=[];selected.value=[];planId.value='';});}
async function discard(){await run(async()=>{if(planId.value)await props.execute('resource.updates.discard',{planId:planId.value});items.value=[];selected.value=[];planId.value='';});}
</script>
