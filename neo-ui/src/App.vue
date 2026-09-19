<template>
<q-layout view="hHh Lpr fFf" class="bg-grey-10 text-white">
 <q-header><q-toolbar><q-toolbar-title>Luna Neo UI <span class="text-caption">{{ buildLabel }}</span></q-toolbar-title><q-btn flat label="刷新" :disable="busy" @click="refresh" /></q-toolbar>
 <q-tabs :model-value="page" @update:model-value="navigate" align="left" outside-arrows mobile-arrows>
  <q-tab v-for="item in pages" :key="item.id" :name="item.id" :icon="item.icon" :label="item.label" />
  <q-tab v-if="selected" name="manage" icon="tune" label="实例管理" />
 </q-tabs></q-header>
 <q-page-container><q-page padding>
 <q-banner v-if="error" class="bg-red-9 q-mb-md">{{ error }}</q-banner>
 <q-banner v-if="notice" class="bg-teal-9 q-mb-md">{{ notice }}</q-banner>
 <q-linear-progress v-if="busy" indeterminate class="q-mb-md" />
 <div v-if="page==='instances'">
  <div class="row items-center q-mb-lg"><h5 class="q-ma-none col">实例库</h5><q-btn color="primary" label="添加实例" @click="navigate('create')" /><q-btn flat label="恢复最近删除" :disable="busy" @click="run('instance.undo-delete')" /></div>
  <q-input dark outlined v-model="search" label="搜索实例" class="q-mb-md" />
  <p v-if="!instances.length && !busy && !error">还没有实例，点击添加实例创建或导入。</p>
  <div class="row q-col-gutter-md"><div class="col-12 col-md-6 col-xl-4" v-for="i in instances.filter(x => (x.name || x.id).toLowerCase().includes(search.toLowerCase()))" :key="i.id">
   <q-card class="bg-grey-9"><q-card-section><div class="text-h6">{{ i.name }}</div><div class="text-grey-4">{{ i.group || '未分组' }} · {{ i.id }}</div></q-card-section>
   <q-card-actions><q-btn color="primary" label="启动" :disable="busy" @click="run('instance.launch',{instance:i.id})" /><q-btn flat label="管理" :disable="busy" @click="openInstance(i)" /><q-btn flat label="打开目录" :disable="busy" @click="run('instance.open-folder',{instance:i.id})" /></q-card-actions></q-card>
  </div></div>
 </div>
 <div v-if="page==='create'" class="form"><h5>添加实例</h5>
  <q-tabs v-model="createMode"><q-tab name="vanilla" label="Minecraft"/><q-tab name="import" label="导入整合包"/></q-tabs>
  <q-input dark outlined v-model="creation.name" label="实例名称" class="q-my-md"/><q-input dark outlined v-model="creation.group" label="分组（可选）" class="q-mb-md"/>
  <template v-if="createMode==='vanilla'">
   <div class="row q-gutter-sm q-mb-md"><q-select class="col" dark outlined v-model="creation.version" :options="minecraftVersions" use-input input-debounce="0" @filter="filterMinecraft" label="Minecraft 版本" @update:model-value="creation.loaderVersion='';loaderVersions=[]"/><q-btn label="加载版本" :disable="busy" @click="loadVersions(false)"/></div>
   <q-select dark outlined v-model="creation.loader" :options="['','fabric','forge','neoforge','quilt']" label="Mod Loader（可选）" class="q-mb-md" @update:model-value="creation.loaderVersion='';loaderVersions=[]"/>
   <div v-if="creation.loader" class="row q-gutter-sm q-mb-md"><q-select class="col" dark outlined v-model="creation.loaderVersion" :options="loaderVersions" label="Loader 版本"/><q-btn label="加载兼容版本" :disable="busy || !creation.version" @click="loadVersions(true)"/></div>
  </template>
  <q-input v-else dark outlined v-model="creation.source" label="整合包本地完整路径或下载 URL" class="q-mb-md"/>
  <q-btn color="primary" label="创建实例" :disable="busy || !creation.name || (createMode==='vanilla' ? (!creation.version || (!!creation.loader && !creation.loaderVersion)) : !creation.source)" @click="create"/>
 </div>
 <div v-if="page==='manage' && selected"><h5>{{ selected.name }} · 实例管理</h5><div class="row q-gutter-sm q-mb-lg">
  <q-btn v-for="a in actions" :key="a.op" :label="a.label" :disable="busy" @click="instanceAction(a.op,a.field,a.label)"/>
  <q-btn label="实例设置" @click="scope='instance'; page='settings'; refresh()"/>
  <q-btn color="negative" label="删除到回收站" :disable="busy" @click="askDelete('instance.delete',{instance:selected.id},selected.name)"/>
 </div><q-input dark outlined autogrow v-model="notes" label="实例备注"/><q-btn class="q-mt-md" label="保存备注" :disable="busy" @click="run('instance.set-notes',{instance:selected.id,notes})"/>
 <h6 class="q-mb-md">资源管理</h6>
 <q-select dark outlined v-model="resourceKind" :options="resourceKinds" emit-value map-options label="资源类型" @update:model-value="loadResources" />
 <div class="row q-gutter-sm q-my-md"><q-input class="col" dark outlined v-model="resourceSource" label="资源本地完整路径或下载 URL"/><q-btn label="安装资源" :disable="busy || !resourceSource" @click="run('resource.install',{instance:selected.id,kind:resourceKind,source:resourceSource})"/></div>
 <p v-if="!resources.length">当前类型暂无资源。</p>
 <q-list bordered separator><q-item v-for="r in resources" :key="r.fileName"><q-item-section>{{ r.name || r.fileName }}<q-item-label caption class="text-grey-4">{{ r.fileName }} · {{ r.enabled ? '已启用' : '已禁用' }}</q-item-label></q-item-section><q-item-section side><div><q-btn flat :label="r.enabled ? '禁用' : '启用'" :disable="busy" @click="run(r.enabled?'resource.disable':'resource.enable',{instance:selected.id,kind:resourceKind,resource:r.fileName})"/><q-btn flat color="red-4" label="删除" :disable="busy" @click="askDelete('resource.remove',{instance:selected.id,kind:resourceKind,resource:r.fileName},r.fileName)"/></div></q-item-section></q-item></q-list>
 </div>
 <div v-if="page==='accounts'"><h5>账户管理</h5>
  <div class="row q-gutter-md q-mb-lg"><q-btn color="primary" label="添加 Microsoft 账户" :disable="busy" @click="run('account.login',{type:'microsoft'})"/><q-input dark outlined dense v-model="username" label="离线玩家名"/><q-btn label="添加离线账户" :disable="busy || !username" @click="run('account.login',{type:'offline',username})"/></div>
  <q-list bordered separator><q-item v-for="a in accounts" :key="a.id"><q-item-section><q-item-label>{{ a.profileName }} {{ a.default ? '（默认）' : '' }}</q-item-label><q-item-label class="text-grey-4">{{ a.type }} · {{ a.state }}</q-item-label></q-item-section><q-item-section side><div><q-btn flat label="设为默认" :disable="busy" @click="run('account.set-default',{account:a.id})"/><q-btn flat label="刷新" :disable="busy" @click="run('account.refresh',{account:a.id})"/><q-btn flat color="red-4" label="移除" :disable="busy" @click="askDelete('account.remove',{account:a.id},a.profileName)"/></div></q-item-section></q-item></q-list>
 </div>
 <div v-if="page==='settings'"><h5>{{ scope==='instance' ? selected?.name+' · 实例设置' : '启动器设置' }}</h5>
 <q-input dark outlined v-model="settingSearch" label="搜索设置名称" class="q-mb-md"/><p class="text-grey-4">设置使用后端原始名称。部分设置需要重启启动器后生效。</p>
 <q-list bordered separator><q-item v-for="s in settings.filter(x => x.key.toLowerCase().includes(settingSearch.toLowerCase()))" :key="s.key"><q-item-section><q-item-label>{{ s.key }}</q-item-label><q-item-label class="text-grey-4">{{ s.redacted ? '敏感值已隐藏' : JSON.stringify(s.value) }}</q-item-label></q-item-section><q-item-section side><div><q-btn flat label="编辑" :disable="busy || s.redacted" @click="editSetting(s)"/><q-btn flat label="恢复默认" :disable="busy" @click="run('settings.reset',{...settingScope(),key:s.key})"/></div></q-item-section></q-item></q-list>
 </div>
 <q-banner v-if="device" class="bg-blue-grey-9 q-mt-md">请在浏览器中访问 {{ device.url }}，输入代码：<strong>{{ device.code }}</strong></q-banner>
 <div v-if="status" class="q-mt-md text-grey-4">{{ status }}</div>
 <q-btn v-if="busy" flat color="orange" label="取消当前任务" @click="cancel"/>
 </q-page></q-page-container>
 <q-dialog v-model="dialog" persistent><q-card class="bg-grey-9 text-white" style="width:520px;max-width:90vw"><q-card-section class="text-h6">{{ prompt }}</q-card-section><q-card-section>
  <q-select v-if="choices" dark outlined v-model="answer" :options="choices" emit-value map-options label="请选择"/>
  <q-input v-else-if="!confirmation" dark outlined v-model="answer" :type="secret ? 'password' : 'textarea'" autofocus/>
 </q-card-section><q-card-actions align="right"><q-btn flat label="取消" @click="finishDialog(false)"/><q-btn color="primary" label="确认" @click="finishDialog(true)"/></q-card-actions></q-card></q-dialog>
</q-layout>
</template>
<script setup lang="ts">
import { onMounted, onUnmounted, ref } from 'vue';
import { launcher } from './boot/launcher-api';
const buildLabel = '界面修订 2026-09-19.2';
const resources=ref<any[]>([]),resourceKind=ref('mods'),resourceSource=ref('');
const resourceKinds=[{label:'模组',value:'mods'},{label:'资源包',value:'resourcepacks'},{label:'光影包',value:'shaderpacks'},{label:'材质包',value:'texturepacks'},{label:'数据包',value:'datapacks'},{label:'投影',value:'schematics'},{label:'Yes Steve Model',value:'yesstevemodels'},{label:'Customizable Player Models',value:'customplayermodels'}];
const pages=[{id:'instances',label:'实例库',icon:'apps'},{id:'create',label:'添加实例',icon:'add_box'},{id:'accounts',label:'账户管理',icon:'person'},{id:'settings',label:'设置',icon:'settings'}];
const page=ref('instances'), busy=ref(false), error=ref(''), notice=ref(''), status=ref(''), search=ref(''), username=ref(''), notes=ref('');
const instances=ref<any[]>([]),accounts=ref<any[]>([]),settings=ref<any[]>([]),selected=ref<any>(null),device=ref<any>(null);
const scope=ref('launcher'),settingSearch=ref(''),createMode=ref('vanilla');
const creation=ref({name:'',group:'',version:'',loader:'',loaderVersion:'',source:''});
const minecraftVersions=ref<string[]>([]),loaderVersions=ref<string[]>([]);
let allMinecraftVersions:string[]=[];
function filterMinecraft(value:string,update:(callback:()=>void)=>void){update(()=>{minecraftVersions.value=allMinecraftVersions.filter(v=>v.toLowerCase().includes(value.toLowerCase()));});}
async function loadVersions(loader:boolean){
 if(busy.value)return;
 busy.value=true;error.value='';
 try{
  const uids:Record<string,string>={fabric:'net.fabricmc.fabric-loader',forge:'net.minecraftforge',neoforge:'net.neoforged',quilt:'org.quiltmc.quilt-loader'};
  const result=await call('component.versions',loader?{uid:uids[creation.value.loader],minecraftVersion:creation.value.version}:{uid:'net.minecraft'});
  const versions=result.versions.map((v:any)=>v.version);
  if(loader)loaderVersions.value=versions;else {allMinecraftVersions=versions;minecraftVersions.value=versions;}
 }catch(e){error.value=message(e);}finally{busy.value=false;}
}
const actions=[{op:'rename',field:'name',label:'重命名'},{op:'copy',field:'name',label:'复制实例'},{op:'group',field:'group',label:'修改分组'},{op:'update',label:'更新组件'},{op:'verify',label:'校验文件'},{op:'stop',label:'停止运行'}];
const dialog=ref(false),prompt=ref(''),answer=ref<any>(''),choices=ref<any[]|null>(null),secret=ref(false),confirmation=ref(false);
let resolveDialog: ((value:any)=>void)|undefined;
function ask(title:string, value:any='', confirm=false, options:any[]|null=null, password=false):Promise<any> {
 prompt.value=title; answer.value=options?.length ? options[0].value : value; confirmation.value=confirm; choices.value=options; secret.value=password; dialog.value=true;
 return new Promise(resolve=>{resolveDialog=resolve;});
}
function finishDialog(ok:boolean){dialog.value=false;resolveDialog?.(ok ? (confirmation.value ? true : answer.value) : null);resolveDialog=undefined;}
function message(e:any){return e?.message || String(e);}
async function call(op:string,p:Record<string,unknown>={}){const r=await launcher.execute<any>(op,p);if(!r.ok)throw new Error(r.error||'操作失败');return r.data;}
function settingScope(){return scope.value==='instance' ? {scope:'instance',instance:selected.value.id} : {scope:'launcher'};}
async function loadResources(){resources.value=[];try{resources.value=await call('resource.list',{instance:selected.value.id,kind:resourceKind.value});}catch(e){error.value=message(e);}}
async function refresh(){error.value='';try{if(page.value==='accounts')accounts.value=await call('account.list');else if(page.value==='settings')settings.value=await call('settings.list',settingScope());else if(page.value==='manage')await loadResources();else instances.value=await call('instance.list');}catch(e){error.value=message(e);}}
async function navigate(id:string){if(busy.value)return;page.value=id;error.value='';notice.value='';if(id==='settings')scope.value='launcher';await refresh();}
async function run(op:string,p:Record<string,unknown>={}){if(busy.value)return;busy.value=true;error.value='';notice.value='';device.value=null;try{await call(op,p);notice.value='操作已完成';await refresh();}catch(e){error.value=message(e);}finally{busy.value=false;status.value='';device.value=null;}}
async function create(){const p:any={...creation.value};for(const k of Object.keys(p))if(!p[k])delete p[k];if(createMode.value==='import'){p.type='import';}await run(createMode.value==='import'?'instance.import':'instance.create',p);if(!error.value){page.value='instances';await refresh();}}
async function openInstance(i:any){selected.value=i;page.value='manage';notes.value='';error.value='';try{const info=await call('instance.info',{instance:i.id});notes.value=info.notes || '';await loadResources();}catch(e){error.value=message(e);}}
async function instanceAction(op:string,field:string|undefined,label:string){const p:any={instance:selected.value.id};if(field){const v=await ask(label,field==='group'?(selected.value.group||''):selected.value.name);if(v===null)return;p[field]=v;}await run('instance.'+op,p);}
async function askDelete(op:string,p:any,name:string){if(await ask('确认移除 '+name+'？','',true))await run(op,{...p,confirm:true});}
async function editSetting(s:any){const value=await ask('编辑 '+s.key,JSON.stringify(s.value));if(value===null)return;try{await run('settings.set',{...settingScope(),key:s.key,value:JSON.parse(value)});}catch(e){error.value='请输入有效的 JSON 值，例如 true、1024 或 "文本"';}}
async function cancel(){try{await call('task.cancel');}catch(e){error.value=message(e);}}
let unlisten:(()=>void)|undefined;
onMounted(async()=>{try{unlisten=await launcher.onEvent(async(raw:any)=>{if(raw.kind==='input'){const opts=raw.choices?.map((x:any,n:number)=>({label:typeof x==='string'?x:(x.name||x.label||JSON.stringify(x)),value:n}));const v=await ask(raw.prompt,'',false,opts||null,raw.secret);try{await launcher.respond(v===null?{interactionId:raw.interactionId,cancel:true}:{interactionId:raw.interactionId,value:v});}catch(e){error.value=message(e);}}else if(raw.kind==='device_code'){device.value=raw.data || raw;}else if(raw.kind==='status'){status.value=raw.message || raw.data?.message || (typeof raw.data==='string'?raw.data:'正在执行');}else if(raw.kind==='task'){status.value=raw.data?.status||raw.data?.state||'正在执行';}});await refresh();}catch(e){error.value=message(e);}});
onUnmounted(()=>unlisten?.());
</script>
<style scoped>.form{max-width:640px}h5{margin-top:12px}</style>
