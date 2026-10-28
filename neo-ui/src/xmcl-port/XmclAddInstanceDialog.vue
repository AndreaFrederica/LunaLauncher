<template>
  <!-- Port of XMCL AppAddInstanceDialog/StepConfig/StepperFooter. -->
  <q-dialog v-model="shown" persistent transition-show="fade" transition-hide="fade">
    <q-card class="xmcl-dialog surface-dialog-shell"><q-card-section class="xmcl-dialog-header"><div class="xmcl-dialog-title"><span class="xmcl-dialog-icon"><q-icon name="add" size="22px"/></span><div><div class="text-subtitle1">{{ t('xmcl.add.title') }}</div><small>{{ t('xmcl.add.subtitle') }}</small></div></div><q-btn flat round icon="close" @click="shown=false"/></q-card-section><q-separator inset />
      <q-tabs v-model="mode" class="q-px-lg q-pt-sm" align="left" outside-arrows mobile-arrows><q-tab name="config" :label="t('xmcl.add.new')" icon="code"/><q-tab name="server" :label="t('xmcl.add.server')" icon="dns"/><q-tab name="import" :label="t('xmcl.add.import')" icon="folder_zip"/><q-tab name="market" :label="t('xmcl.add.market')" icon="store"/></q-tabs>
      <q-card-section class="xmcl-dialog-body">
        <CatalogBrowser v-if="mode==='market'" packs :busy="busy" :execute="execute" @installed="done" />
        <template v-else><q-input dark outlined v-model="form.name" :label="t('xmcl.add.name')" autofocus class="q-mb-md"/><q-input dark outlined v-model="form.group" :label="t('xmcl.add.group')" class="q-mb-md"/>
          <template v-if="mode==='config'"><div class="row q-gutter-sm"><q-select class="col" dark outlined v-model="form.version" :options="versions" :label="t('xmcl.add.mcVersion')"/><q-btn flat :label="t('xmcl.add.refreshVersions')" :disable="busy" @click="loadVersions"/></div><q-select dark outlined v-model="form.loader" :options="['','fabric','forge','neoforge','quilt']" :label="t('xmcl.add.loader')" class="q-mt-md"/></template>
          <template v-else-if="mode==='server'"><q-input dark outlined v-model="form.version" :label="t('xmcl.add.serverVersion')" class="q-mb-md"/><q-input dark outlined v-model="form.source" :label="t('xmcl.add.serverJar')"/></template>
          <template v-else><q-input dark outlined v-model="form.source" :label="t('xmcl.add.packSource')"/><q-file dark outlined v-model="importFile" :label="t('xmcl.add.chooseFile')" accept=".zip,.mrpack,.tar.gz" class="q-mt-md" @update:model-value="readFile"/></template>
        </template><q-banner v-if="error" class="bg-red-9 q-mt-md">{{ error }}</q-banner>
      </q-card-section><q-separator inset /><q-card-actions class="xmcl-dialog-actions"><q-space/><q-btn flat rounded :label="t('xmcl.add.cancel')" @click="shown=false"/><q-btn color="primary" rounded :loading="busy" :disable="!canCreate" :label="mode==='server'?t('xmcl.add.createServer'):t('xmcl.add.createInstance')" @click="create"/></q-card-actions>
    </q-card>
  </q-dialog>
</template>
<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import CatalogBrowser from '../components/CatalogBrowser.vue';
const props=defineProps<{modelValue:boolean;busy:boolean;execute:(op:string,p?:Record<string,unknown>)=>Promise<any>}>();
const { t } = useI18n();
const emit=defineEmits<{ 'update:modelValue':[value:boolean];created:[]}>();
const shown=computed({get:()=>props.modelValue,set:(v)=>emit('update:modelValue',v)}),mode=ref('config'),versions=ref<string[]>([]),importFile=ref<File|null>(null),form=ref<any>({name:'',group:'',version:'',loader:'',source:''}),error=ref('');
const canCreate=computed(()=>Boolean(form.value.name && (mode.value==='config'?form.value.version:mode.value==='import'?form.value.source:true)));
async function loadVersions(){try{const r=await props.execute('component.versions',{uid:'net.minecraft'});versions.value=(r.versions||[]).map((v:any)=>v.version);}catch(e){error.value=String(e);}}
async function readFile(file:File|null){if(file)form.value.source=(file as any).path||file.name;}
async function create(){try{const p={...form.value,type:mode.value==='server'?'server':mode.value==='import'?'import':'vanilla'};await props.execute(mode.value==='import'?'instance.import':'instance.create',p);done();}catch(e){error.value=String(e);}}
function done(){shown.value=false;emit('created');}
watch(shown,(v)=>{if(v)void loadVersions();});
</script>
<style scoped>.xmcl-dialog{width:900px;max-width:94vw;background:rgba(34,34,40,.9)!important;color:#fff;border-radius:28px!important;backdrop-filter:blur(48px);border:1px solid rgba(255,255,255,.12)}.xmcl-dialog-header{display:flex;align-items:center;padding:24px 24px 16px}.xmcl-dialog-title{display:flex;align-items:center;gap:12px;flex:1}.xmcl-dialog-title small{color:rgba(255,255,255,.58)}.xmcl-dialog-icon{width:40px;height:40px;display:flex;align-items:center;justify-content:center;border-radius:12px;background:rgba(129,140,248,.14);color:#a5b4fc}.xmcl-dialog-body{min-height:330px;max-height:70vh;overflow:auto;padding:24px}.xmcl-dialog-actions{padding:16px 24px 24px}.xmcl-dialog :deep(.q-field__control){border-radius:12px}.xmcl-dialog :deep(.q-btn){text-transform:none}</style>
