<template>
<q-layout view="hHh Lpr fFf" class="neo-shell text-white">
 <q-header class="neo-header"><q-toolbar class="neo-toolbar"><q-btn flat round icon="menu" class="lt-md" @click="drawerOpen=!drawerOpen"/><q-avatar square size="34px" class="brand-mark"><q-icon name="auto_awesome"/></q-avatar><q-toolbar-title class="brand-title">Luna <span>Neo</span><small>{{ buildLabel }}</small></q-toolbar-title><div class="header-status"><q-icon name="circle" size="9px" color="positive"/> API 已连接</div><q-btn flat round icon="refresh" :disable="busy" @click="refresh" /></q-toolbar></q-header>
 <q-drawer v-model="drawerOpen" :behavior="$q.screen.lt.md ? 'mobile' : 'desktop'" :width="248" class="neo-drawer">
  <div class="drawer-brand"><q-avatar square size="42px" class="brand-mark"><q-icon name="auto_awesome"/></q-avatar><div><strong>Luna Neo</strong><small>现代启动器</small></div></div>
  <div class="drawer-label">工作区</div><q-list class="nav-list">
   <q-item v-for="item in pages" :key="item.id" clickable :active="page===item.id" active-class="nav-active" @click="navigate(item.id);drawerOpen=false"><q-item-section avatar><q-icon :name="item.icon"/></q-item-section><q-item-section>{{ item.label }}</q-item-section></q-item>
   <q-item v-if="selected" clickable :active="page==='manage'" active-class="nav-active" @click="navigate('manage');drawerOpen=false"><q-item-section avatar><q-icon name="tune"/></q-item-section><q-item-section>实例管理</q-item-section></q-item>
  </q-list><div class="drawer-footer"><q-icon name="info"/> Neo UI {{ buildLabel }}</div>
 </q-drawer>
 <q-page-container><q-page padding class="neo-page">
 <q-banner v-if="error" class="bg-red-9 q-mb-md">{{ error }}</q-banner>
 <q-banner v-if="notice" class="bg-teal-9 q-mb-md">{{ notice }}</q-banner>
 <q-linear-progress v-if="busy" indeterminate class="q-mb-md" />
 <div v-if="page==='instances'">
  <div class="workspace-heading"><div><div class="eyebrow">MY WORKSPACE</div><h1>实例库</h1><p>管理你的 Minecraft 世界、整合包和运行环境。</p></div><div class="heading-actions"><q-btn unelevated color="primary" icon="add" label="添加实例" @click="navigate('create')" /><q-btn flat icon="restore" label="恢复删除" :disable="busy" @click="run('instance.undo-delete')" /></div></div>
  <div class="library-toolbar"><q-input dark borderless v-model="search" placeholder="搜索实例、版本或分组" class="library-search"><template #prepend><q-icon name="search"/></template></q-input><div class="library-count"><strong>{{ instances.length }}</strong> 个实例</div></div>
  <p v-if="!instances.length && !busy && !error">还没有实例，点击添加实例创建或导入。</p>
  <div class="row q-col-gutter-lg"><div class="col-12 col-sm-6 col-lg-4" v-for="i in instances.filter(x => (x.name || x.id).toLowerCase().includes(search.toLowerCase()))" :key="i.id">
   <q-card class="instance-card"><div class="instance-art"><q-icon name="sports_esports" size="34px"/><span>{{ i.running ? '运行中' : '就绪' }}</span></div><q-card-section><div class="instance-name">{{ i.name }}</div><div class="instance-meta"><q-icon name="folder"/> {{ i.group || '未分组' }}</div><div class="instance-id">{{ i.id }}</div></q-card-section>
   <q-card-actions class="instance-actions"><q-btn unelevated color="primary" icon="play_arrow" label="启动" :disable="busy" @click="launch(i)" /><q-btn flat round icon="more_horiz" :disable="busy" @click="openInstance(i)" /></q-card-actions></q-card>
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
  <q-btn color="primary" label="启动并查看日志" :disable="busy" @click="launch(selected)"/>
  <q-btn v-for="a in actions" :key="a.op" :label="a.label" :disable="busy" @click="instanceAction(a.op,a.field,a.label)"/>
  <q-btn label="实例设置" @click="scope='instance'; page='settings'; refresh()"/>
  <q-btn color="negative" label="删除到回收站" :disable="busy" @click="askDelete('instance.delete',{instance:selected.id},selected.name)"/>
 </div><q-input dark outlined autogrow v-model="notes" label="实例备注"/><q-btn class="q-mt-md" label="保存备注" :disable="busy" @click="run('instance.set-notes',{instance:selected.id,notes})"/>
 <p class="text-grey-4" style="overflow-wrap:anywhere">实例目录：{{ instanceDetails?.root || '尚未读取' }}<br>游戏目录：{{ instanceDetails?.gameRoot || '尚未读取' }}</p>
 <h6 class="q-mb-md">版本组件</h6>
 <q-banner v-if="componentError" class="bg-red-9 q-my-sm">{{ componentError }}</q-banner>
 <q-list bordered separator><q-item v-for="c in components" :key="c.id"><q-item-section><q-item-label>{{ c.name || c.id }} · {{ c.version }} {{ c.custom ? '（自定义）' : '' }}</q-item-label><q-item-label caption class="text-grey-4">{{ c.id }} · {{ c.enabled ? '已启用' : '已禁用' }}</q-item-label><q-item-label v-for="(p,index) in c.problems" :key="index" class="text-orange">{{ p.description }}</q-item-label></q-item-section><q-item-section side><q-btn v-if="c.canChangeVersion" flat label="更改版本" :disable="busy" @click="changeComponentVersion(c)"/><q-btn v-if="c.canDisable" flat :label="c.enabled?'禁用':'启用'" :disable="busy" @click="run('instance.component.set-enabled',{instance:selected.id,component:c.id,enabled:!c.enabled})"/></q-item-section></q-item></q-list>
 <h6 class="q-mb-md">资源管理</h6>
 <q-select dark outlined v-model="resourceKind" :options="resourceKinds" emit-value map-options label="资源类型" @update:model-value="loadResources" />
 <div class="row q-gutter-sm q-my-md"><q-input class="col" dark outlined v-model="resourceSource" label="资源本地完整路径或下载 URL"/><q-btn label="安装资源" :disable="busy || !resourceSource" @click="run('resource.install',{instance:selected.id,kind:resourceKind,source:resourceSource})"/></div>
 <q-linear-progress v-if="resourcesLoading" indeterminate class="q-my-sm"/>
 <q-banner v-if="resourceError" class="bg-red-9 q-my-sm">资源读取失败：{{ resourceError }}</q-banner>
 <p v-else-if="!resourcesLoading && !resources.length">当前目录中没有此类资源。请核对上方实例目录是否与老 UI 相同。</p>
 <p v-else-if="!resourcesLoading">共 {{ resources.length }} 项资源</p>
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
 <q-card v-if="selected && page==='manage'" class="bg-grey-9 q-mt-lg">
  <q-card-section><div class="text-h6">{{ selected.name }} · 日志</div>
   <q-tabs v-model="logMode" align="left"><q-tab name="live" label="实时控制台"/><q-tab name="files" label="历史日志"/></q-tabs>
   <template v-if="logMode==='live'"><div class="row items-center q-gutter-sm"><q-btn flat label="连接控制台" :disable="busy" @click="connectConsole(selected)"/><q-btn flat label="清空显示" @click="consoleLines=[]"/><q-checkbox dark v-model="followLog" label="自动滚动"/><span>{{ consoleState }}</span></div>
    <pre ref="consoleElement" class="console" tabindex="0">{{ consoleLines.join('\n') || '启动时自动连接；也可以连接已运行实例的控制台。' }}</pre>
   </template>
   <template v-else><div class="row q-gutter-sm q-my-md"><q-select class="col" dark outlined v-model="logFile" :options="logFiles.map(f=>({label:f.name,value:f.path}))" emit-value map-options label="日志文件"/><q-btn label="刷新文件" :disable="busy" @click="loadLogFiles"/><q-btn label="读取" :disable="busy || !logFile" @click="readLogFile"/></div><pre class="console" tabindex="0">{{ fileLog || '选择文件后点击读取。' }}</pre></template>
  </q-card-section>
 </q-card>
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
import { nextTick, onMounted, onUnmounted, ref } from 'vue';
import { launcher } from './boot/launcher-api';
const buildLabel = '界面修订 2026-09-19.3';
const components=ref<any[]>([]),componentError=ref('');
async function loadComponents(){componentError.value='';components.value=[];try{const r=await call('instance.components.list',{instance:selected.value.id});components.value=r.components;}catch(e){componentError.value=message(e);}}
async function changeComponentVersion(c:any){const v=await ask('更改 '+(c.name||c.id)+' 版本',c.version);if(v!==null && v.trim())await run('instance.component.set-version',{instance:selected.value.id,component:c.id,version:v.trim()});}
const instanceDetails=ref<any>(null),resourcesLoading=ref(false),resourceError=ref('');
const logMode=ref('live'),logFiles=ref<any[]>([]),logFile=ref(''),fileLog=ref('');
const consoleLines=ref<string[]>([]),consoleState=ref('未连接'),followLog=ref(true),consoleElement=ref<HTMLElement|null>(null);
let consoleSubscription='',consoleInstance='',launchInFlight=false;
async function appendConsole(text:string){
 consoleLines.value.push(text);
 if(consoleLines.value.length>3000)consoleLines.value.splice(0,consoleLines.value.length-3000);
 await nextTick();if(followLog.value && consoleElement.value)consoleElement.value.scrollTop=consoleElement.value.scrollHeight;
}
function receiveStream(batch:any){
 if(batch.subscriptionId!==consoleSubscription)return;
 if(batch.dropped)void appendConsole(`[日志缓冲区已省略 ${batch.dropped} 条消息]`);
 for(const event of batch.events || []){
  if(event.kind==='console.line')void appendConsole(event.text);
  else if(event.kind==='console.data')void appendConsole(new TextDecoder().decode(Uint8Array.from(atob(event.data),c=>c.charCodeAt(0))));
  else if(event.kind==='instance.state'){
   consoleState.value=event.running?'客户端运行中':'客户端已停止';
   void appendConsole(`[${consoleState.value}]`);
  }else if(event.kind==='console.reset')consoleState.value=event.available?'控制台已连接':'等待启动';
 }
}
async function connectConsole(i:any){
 if(consoleSubscription && consoleInstance===i.id)return;
 try{
  if(consoleSubscription)await call('event.unsubscribe',{subscriptionId:consoleSubscription});
  consoleSubscription='';consoleInstance=i.id;consoleLines.value=[];
  const sub=await call('instance.console.subscribe',{instance:i.id});consoleSubscription=sub.subscriptionId;
  consoleState.value='控制台已连接';
 }catch(e){error.value=message(e);consoleState.value='连接失败';}
}
async function launch(i:any){
 if(busy.value)return;
 await openInstance(i);logMode.value='live';await connectConsole(i);
 if(!consoleSubscription)return;
 busy.value=true;launchInFlight=true;error.value='';notice.value='';
 try{
  const result=await call('instance.launch',{instance:i.id});
  notice.value=`启动请求已返回（PID ${result.pid}），请查看控制台确认客户端运行状态。`;
 }catch(e){error.value=message(e);void appendConsole('[启动失败] '+error.value);}
 finally{busy.value=false;launchInFlight=false;device.value=null;status.value='';}
}
async function loadLogFiles(){try{logFiles.value=await call('instance.log.list',{instance:selected.value.id});}catch(e){error.value=message(e);}}
async function readLogFile(){try{const r=await call('instance.log.read',{instance:selected.value.id,file:logFile.value,maxBytes:1048576});fileLog.value=r.content+(r.truncated?'\n[文件超过 1 MiB，仅显示前部分]':'');}catch(e){error.value=message(e);}}
const resources=ref<any[]>([]),resourceKind=ref('mods'),resourceSource=ref('');
const resourceKinds=[{label:'模组',value:'mods'},{label:'资源包',value:'resourcepacks'},{label:'光影包',value:'shaderpacks'},{label:'材质包',value:'texturepacks'},{label:'数据包',value:'datapacks'},{label:'投影',value:'schematics'},{label:'Yes Steve Model',value:'yesstevemodels'},{label:'Customizable Player Models',value:'customplayermodels'}];
const pages=[{id:'instances',label:'实例库',icon:'apps'},{id:'create',label:'添加实例',icon:'add_box'},{id:'accounts',label:'账户管理',icon:'person'},{id:'settings',label:'设置',icon:'settings'}];
const drawerOpen=ref(true), page=ref('instances'), busy=ref(false), error=ref(''), notice=ref(''), status=ref(''), search=ref(''), username=ref(''), notes=ref('');
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
let resourceRequest=0;
async function loadResources(){const request=++resourceRequest;const id=selected.value.id,kind=resourceKind.value;resources.value=[];resourceError.value='';resourcesLoading.value=true;try{const data=await call('resource.list',{instance:id,kind});if(request===resourceRequest){if(!Array.isArray(data))throw new Error('资源接口返回格式错误');resources.value=data;}}catch(e){if(request===resourceRequest)resourceError.value=message(e);}finally{if(request===resourceRequest)resourcesLoading.value=false;}}
async function refresh(){error.value='';try{if(page.value==='accounts')accounts.value=await call('account.list');else if(page.value==='settings')settings.value=await call('settings.list',settingScope());else if(page.value==='manage'){await loadResources();await loadComponents();await loadLogFiles();}else instances.value=await call('instance.list');}catch(e){error.value=message(e);}}
async function navigate(id:string){if(busy.value)return;page.value=id;error.value='';notice.value='';if(id==='settings')scope.value='launcher';await refresh();}
async function run(op:string,p:Record<string,unknown>={}){if(busy.value)return;busy.value=true;error.value='';notice.value='';device.value=null;try{await call(op,p);notice.value='操作已完成';await refresh();}catch(e){error.value=message(e);}finally{busy.value=false;status.value='';device.value=null;}}
async function create(){const p:any={...creation.value};for(const k of Object.keys(p))if(!p[k])delete p[k];if(createMode.value==='import'){p.type='import';}await run(createMode.value==='import'?'instance.import':'instance.create',p);if(!error.value){page.value='instances';await refresh();}}
async function openInstance(i:any){
 if(selected.value?.id!==i.id){
  if(consoleSubscription){try{await call('event.unsubscribe',{subscriptionId:consoleSubscription});}catch(e){error.value=message(e);}}
  consoleSubscription='';consoleInstance='';consoleLines.value=[];consoleState.value='未连接';logFile.value='';fileLog.value='';logFiles.value=[];
 }
 selected.value=i;page.value='manage';notes.value='';error.value='';instanceDetails.value=null;
 try{const info=await call('instance.info',{instance:i.id});instanceDetails.value=info;notes.value=info.notes || '';}catch(e){error.value=message(e);}
 await loadResources();await loadComponents();await loadLogFiles();
}
async function instanceAction(op:string,field:string|undefined,label:string){const p:any={instance:selected.value.id};if(field){const v=await ask(label,field==='group'?(selected.value.group||''):selected.value.name);if(v===null)return;p[field]=v;}await run('instance.'+op,p);}
async function askDelete(op:string,p:any,name:string){if(await ask('确认移除 '+name+'？','',true))await run(op,{...p,confirm:true});}
async function editSetting(s:any){const value=await ask('编辑 '+s.key,JSON.stringify(s.value));if(value===null)return;try{await run('settings.set',{...settingScope(),key:s.key,value:JSON.parse(value)});}catch(e){error.value='请输入有效的 JSON 值，例如 true、1024 或 "文本"';}}
async function cancel(){try{await call('task.cancel');}catch(e){error.value=message(e);}}
let unlisten:(()=>void)|undefined,unlistenStream:(()=>void)|undefined,unlistenExit:(()=>void)|undefined;
onMounted(async()=>{try{unlistenStream=await launcher.onStream(receiveStream);unlistenExit=await launcher.onExit(()=>{consoleSubscription='';consoleState.value='后端已退出';error.value='启动器后端已退出，请刷新重连。';});unlisten=await launcher.onEvent(async(raw:any)=>{if(raw.kind==='input'){const opts=raw.choices?.map((x:any,n:number)=>({label:typeof x==='string'?x:(x.name||x.label||JSON.stringify(x)),value:n}));const v=await ask(raw.prompt,'',false,opts||null,raw.secret);try{await launcher.respond(v===null?{interactionId:raw.interactionId,cancel:true}:{interactionId:raw.interactionId,value:v});}catch(e){error.value=message(e);}}else if(raw.kind==='device_code'){device.value=raw.data || raw;}else if(raw.kind==='status'){if(launchInFlight)void appendConsole('[启动] '+(raw.message || raw.data?.message || ''));status.value=raw.message || raw.data?.message || (typeof raw.data==='string'?raw.data:'正在执行');}else if(raw.kind==='task'){status.value=raw.data?.status||raw.data?.state||'正在执行';}});await refresh();}catch(e){error.value=message(e);}});
onUnmounted(()=>{unlisten?.();unlistenStream?.();unlistenExit?.();});
</script>
<style scoped>
.form{max-width:720px}.console{height:360px;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;background:#0a0e16;padding:16px;font-size:12px;user-select:text;border:1px solid rgba(255,255,255,.06);border-radius:12px}.workspace-heading{display:flex;justify-content:space-between;align-items:flex-end;gap:24px;margin:12px 0 28px}.workspace-heading h1{margin:3px 0 6px;font-size:32px;letter-spacing:-.03em}.workspace-heading p{margin:0;color:var(--neo-muted)}.eyebrow{color:var(--neo-primary);font-size:11px;font-weight:700;letter-spacing:.14em}.heading-actions{display:flex;gap:10px}.library-toolbar{display:flex;align-items:center;gap:20px;padding:6px 14px;margin-bottom:22px;background:var(--neo-panel);border:1px solid var(--neo-border);border-radius:14px}.library-search{flex:1}.library-count{color:var(--neo-muted);font-size:13px}.library-count strong{color:white;font-size:18px;margin-right:4px}.instance-card{overflow:hidden;background:var(--neo-panel)!important;border:1px solid var(--neo-border);border-radius:16px;transition:transform .18s,border-color .18s,box-shadow .18s}.instance-card:hover{transform:translateY(-3px);border-color:rgba(129,140,248,.55);box-shadow:0 14px 36px rgba(0,0,0,.25)}.instance-art{height:112px;padding:16px;display:flex;align-items:flex-end;justify-content:space-between;background:linear-gradient(135deg,#312e81,#4f46e5 50%,#0f172a);color:#fff}.instance-art span{padding:5px 9px;border-radius:20px;background:rgba(0,0,0,.3);font-size:11px}.instance-name{font-size:18px;font-weight:700}.instance-meta{display:flex;align-items:center;gap:5px;margin-top:8px;color:var(--neo-muted);font-size:13px}.instance-id{margin-top:5px;color:#64748b;font-size:11px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.instance-actions{border-top:1px solid var(--neo-border);padding:10px 14px}.instance-actions .q-btn:first-child{flex:1}.neo-page :deep(h5),.neo-page :deep(h6){letter-spacing:-.015em}.neo-page :deep(.q-card){border-radius:16px}.neo-page :deep(.q-field--outlined .q-field__control){border-radius:10px}.neo-page :deep(.q-btn){border-radius:9px;text-transform:none;font-weight:600}.neo-page :deep(.q-banner){border-radius:10px}.neo-page :deep(.q-item){border-radius:9px}.neo-page :deep(.q-tab){text-transform:none}.neo-page :deep(.q-list--bordered){border-color:var(--neo-border);border-radius:12px;overflow:hidden}
@media(max-width:700px){.workspace-heading{display:block}.heading-actions{margin-top:18px}.heading-actions .q-btn{flex:1}.library-toolbar{padding-left:10px}.library-count{display:none}}
</style>
