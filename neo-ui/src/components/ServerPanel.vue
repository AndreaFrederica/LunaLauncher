<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <q-card class="server-panel q-mt-lg"><q-card-section><div class="text-h6">{{ t('neo.auto2.k7cdb92ecd5') }}</div><q-tabs v-model="tab" align="left" outside-arrows mobile-arrows><q-tab name="console" :label="t('neo.auto2.k5bd086d22a')" icon="terminal"/><q-tab name="properties" label="server.properties" icon="settings"/><q-tab name="eula" label="EULA" icon="gavel"/><q-tab name="lists" :label="t('neo.auto2.kd3fa689df1')" icon="group"/><q-tab name="yaml" label="YAML" icon="description"/></q-tabs>
    <div v-if="tab==='console'" class="q-mt-md"><div class="row q-gutter-sm"><q-btn color="primary" :label="t('neo.auto2.ka95fa276ac')" :disable="busy" @click="act('server.start')"/><q-input class="col" dark outlined v-model="command" :label="t('neo.auto2.k130eb9314f')" @keyup.enter="sendCommand"/><q-btn :label="t('neo.auto2.k1214d633a4')" :disable="busy || !command" @click="sendCommand"/></div><pre class="console q-mt-md">{{ consoleLines.join('\n') || t('neo.auto2.ke7a39877cc') }}</pre></div>
    <div v-else-if="tab==='properties'" class="q-mt-md"><q-input dark outlined type="textarea" autogrow v-model="propertiesText" :label="t('neo.auto2.k869f91acbf')"/><div class="row q-gutter-sm q-mt-sm"><q-btn flat :label="t('neo.auto2.kb6b97ac60d')" :disable="busy" @click="readProperties"/><q-btn color="primary" :label="t('neo.auto2.kfadf24dbc5')" :disable="busy" @click="writeProperties"/></div></div>
    <div v-else-if="tab==='eula'" class="q-mt-md"><q-checkbox dark v-model="eula" :label="t('neo.auto2.k245c33b074')"/><q-btn color="primary" :label="t('neo.auto2.k69b282319d')" :disable="busy" class="q-ml-md" @click="act('server.eula.write',{agreed:eula})"/><pre class="console q-mt-md">{{ eulaText }}</pre></div>
    <div v-else-if="tab==='lists'" class="q-mt-md"><q-select dark outlined v-model="listKind" :options="['ops','whitelist','banned-players','banned-ips']" :label="t('neo.auto2.kd46f82fdf0')" @update:model-value="readList"/><div class="row q-gutter-sm q-mt-sm"><q-input class="col" dark outlined v-model="entry" :label="t('neo.auto2.kbf2d4250e4')"/><q-btn :label="t('neo.auto2.k94191ce210')" :disable="busy || !entry" @click="addEntry"/></div><q-list bordered separator class="q-mt-md"><q-item v-for="item in entries" :key="JSON.stringify(item)"><q-item-section>{{ typeof item==='string'?item:(item.name||item.ip||JSON.stringify(item)) }}</q-item-section><q-item-section side><q-btn flat color="negative" :label="t('neo.auto2.k3755f56f2f')" :disable="busy" @click="removeEntry(item)"/></q-item-section></q-item></q-list></div>
    <div v-else class="q-mt-md"><q-select dark outlined v-model="yamlFile" :options="['bukkit.yml','spigot.yml']" :label="t('neo.auto2.k49deaf7da2')"/><q-input dark outlined type="textarea" autogrow v-model="yamlText" :label="t('neo.auto2.k2dacc1ede3')" class="q-mt-sm"/><div class="row q-gutter-sm q-mt-sm"><q-btn flat :label="t('neo.auto2.kb6b97ac60d')" :disable="busy" @click="readYaml"/><q-btn color="primary" :label="t('neo.auto2.kfadf24dbc5')" :disable="busy" @click="writeYaml"/></div></div>
    <q-banner v-if="error" class="bg-red-9 q-mt-md">{{ error }}</q-banner>
  </q-card-section></q-card>
</template>
<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { onMounted, ref } from 'vue';
const props=defineProps<{instance:string;busy:boolean;execute:(op:string,p?:Record<string,unknown>)=>Promise<any>}>();
const tab=ref('console'),command=ref(''),consoleLines=ref<string[]>([]),propertiesText=ref(''),eula=ref(false),eulaText=ref(''),listKind=ref('ops'),entries=ref<any[]>([]),entry=ref(''),yamlFile=ref('bukkit.yml'),yamlText=ref(''),error=ref('');
async function attempt(action:()=>Promise<any>){error.value='';try{return await action();}catch(e){error.value=String(e);}}
function act(op:string,p:Record<string,unknown>={}){return attempt(()=>props.execute(op,{instance:props.instance,...p}));}
async function sendCommand(){if(!command.value)return;await act('server.console.command',{command:command.value});command.value='';}
async function readProperties(){await attempt(async()=>{const r=await props.execute('server.properties.read',{instance:props.instance});propertiesText.value=Object.entries(r.properties||r).map(([k,v])=>`${k}=${v}`).join('\n');});}
async function writeProperties(){const p:any={};for(const line of propertiesText.value.split(/\r?\n/)){const i=line.indexOf('=');if(i>0)p[line.slice(0,i).trim()]=line.slice(i+1);}await act('server.properties.write',{properties:p});}
async function loadEula(){await attempt(async()=>{const r=await props.execute('server.eula.read',{instance:props.instance});eula.value=Boolean(r.agreed);eulaText.value=r.content||'';});}
async function readList(){await attempt(async()=>{const r=await props.execute('server.list.read',{instance:props.instance,kind:listKind.value});entries.value=r.entries||r||[];});}
async function addEntry(){await act('server.list.add',{kind:listKind.value,entry:entry.value});entry.value='';await readList();}
async function removeEntry(item:any){const value=typeof item==='string'?item:(item.name||item.ip);await act('server.list.remove',{kind:listKind.value,entry:value,confirm:true});await readList();}
async function readYaml(){await attempt(async()=>{const r=await props.execute('server.yaml.read',{instance:props.instance,file:yamlFile.value});yamlText.value=r.content||'';});}
async function writeYaml(){await act('server.yaml.write',{file:yamlFile.value,content:yamlText.value});}
onMounted(()=>{void Promise.all([loadEula(),readProperties(),readList(),readYaml()]);});
</script>
<style scoped>.server-panel{background:var(--neo-panel)!important;border:1px solid var(--neo-border)}.console{height:260px;overflow:auto;white-space:pre-wrap;background:#090d14;padding:14px;border-radius:10px}</style>
