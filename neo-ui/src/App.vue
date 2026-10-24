<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
<div v-if="editorInstanceId" class="pcl-standalone-root"><PclStandaloneInstanceEditor :instance-id="editorInstanceId" /></div>
<div v-else-if="resourceDownloadInstance" class="pcl-standalone-root"><PclResourceDownloadWindow :instance-id="resourceDownloadInstance" :instance-name="resourceDownloadName" :kind="resourceDownloadKind" /></div>
<div v-else class="xmcl-app text-white">
<NeoUiChrome :connected="connected" :busy="busy" :page="page" :instance-name="selected?.name" :is-pcl-shell="isPclShell" :instances="instances" :selected="selected" :icons="icons" @refresh="refresh" @tasks="taskCenterOpen=true" @search="openGlobalSearch" @navigate="navigate" @select="openInstance" @launch="launch" @back="()=>navigate('instances')" @add="addDialog=true">
<div class="xmcl-content neo-page">
 <Transition name="pcl-route" mode="out-in"><div v-if="isPclShell" :key="page" class="pcl-route-shell">
 <PclLaunchPage v-if="page==='instances'" :instances="instances" :icons="icons" :selected="selected" :icon="selected ? icons[selected.id] : undefined" :busy="busy" :notice="notice" :error="error" :accounts="accounts" :account-id="launchAccountId" :launch-state="launchUiState" :launch-progress="launchProgress" :launch-stage="launchStage" :launch-login-method="launchLoginMethod" :launch-speed="launchSpeed"
 :launch-details="launchDetails"
 :launch-children="launchChildren" :launch-error="launchError" :log-lines="consoleLines" :execute="catalogExecute" @select="selectPclInstance" @quick-launch="quickLaunchHome" @connect-server="quickConnect" @launch-instance="menuLaunch" @edit-instance="editInstance" @open-instance-folder="openInstanceFolder" @instance-action="instanceSelectionAction" @launch="selected && launch(selected, launchAccountId)" @cancel="cancel" @download="navigate('pcl-download')" @account-change="selectLaunchAccount" @account-page="navigate('accounts')" @open-folder="openFolder" @settings="openPclInstancePage" @refresh="refresh" @tasks="taskCenterOpen=true" @clear-log="clearConsole" />

 <PclDownloadPage v-if="isPclShell && page==='pcl-download'" :selected="selected" :busy="busy" :execute="catalogExecute" :instances="instances" @refresh="refresh" @launch="navigate('instances')" @installed="onPclInstalled" @install-start="beginInstallProgress" @install-progress="installTask = $event" @install-done="endInstallProgress" />
 <PclToolsPage v-if="isPclShell && page==='pcl-tools'" :instances="instances" :selected="selected" :busy="busy" :execute="catalogExecute" @refresh="refresh" @open-instance="openInstance" />
 <PclSetupPage v-if="isPclShell && page==='pcl-settings'" :settings="settings" :busy="busy" :scope="settingScope()" :execute="catalogExecute" :accounts="accounts" @changed="refresh" />
 <PclAccountPage v-if="page==='accounts' && isPclShell" :accounts="accounts" :busy="busy" :execute="catalogExecute" @changed="refresh" @cancel-login="cancelDeviceLogin" />
 <PclInstancePage v-if="page==='manage' && selected" :instance="selected" :details="instanceDetails" :icon="icons[selected.id]" :components="components" :settings="settings" :component-error="componentError" :notes="notes" :busy="busy" :execute="catalogExecute" :console-lines="consoleLines" :console-state="consoleState" :log-files="logFiles" :file-log="fileLog" @launch="launch(selected)" @open-folder="openFolder" @settings="scope='instance';page='pcl-settings';refresh()" @delete="askDelete('instance.delete',{instance:selected.id},selected.name)" @change-icon="changeIcon" @save-notes="run('instance.set-notes',{instance:selected.id,notes:$event})" @refresh="refresh" @connect-console="connectConsole(selected)" @clear-console="consoleLines=[]" @read-log="(file)=>{logFile=file;readLogFile()}" @component-enabled="(c,enabled)=>run('instance.component.set-enabled',{instance:selected.id,component:c.id,enabled})" @component-version="installComponentVersion" @browse="page='pcl-download';refresh()" @export="exportInstance" @save-setting="(key,value)=>run('settings.set',{scope:'instance',instance:selected.id,key,value})" @install-start="beginInstallProgress" @install-progress="installTask = $event" @install-done="endInstallProgress" @action="workspaceAction" />
 </div></Transition>
 <XmclDashboard v-if="!isPclShell && page==='instances'" :instances="instances" :icons="icons" :busy="busy" @select="openInstance" @launch="launch" @batch-launch="launchMany" @add="addDialog=true" @refresh="refresh" />
 <q-banner v-if="launchQueue.length" class="bg-grey-9 q-mb-md">等待启动：{{ launchQueue.map(i=>i.name).join('、') }}<template #action><q-btn flat label="清空队列" @click="launchQueue=[]"/></template></q-banner>
 <XmclInstanceWorkspace v-if="page==='manage' && selected && !isPclShell" :instance="selected" :details="instanceDetails" :icon="icons[selected.id]" :components="components" :component-error="componentError" :notes="notes" :busy="busy" :execute="catalogExecute" :console-lines="consoleLines" :console-state="consoleState" :log-files="logFiles" :file-log="fileLog" @launch="launch(selected)" @open-folder="openFolder" @settings="scope='instance';page='settings';refresh()" @delete="askDelete('instance.delete',{instance:selected.id},selected.name)" @change-icon="changeIcon" @save-notes="run('instance.set-notes',{instance:selected.id,notes:$event})" @refresh="refresh" @connect-console="connectConsole(selected)" @clear-console="consoleLines=[]" @read-log="(file)=>{logFile=file;readLogFile()}" @component-enabled="(c,enabled)=>run('instance.component.set-enabled',{instance:selected.id,component:c.id,enabled})" @component-version="(c,version)=>run('instance.component.set-version',{instance:selected.id,component:c.id,version})" @action="workspaceAction" />
 <div v-if="page==='create'" class="form"><h5>添加实例</h5>
  <q-tabs v-model="createMode"><q-tab name="vanilla" label="Minecraft"/><q-tab name="server" label="服务端"/><q-tab name="import" label="导入整合包"/><q-tab name="catalog" label="下载整合包"/></q-tabs>
  <CatalogBrowser v-if="createMode==='catalog'" packs :busy="busy" :execute="catalogExecute" @installed="navigate('instances')"/>
  <template v-else>
  <q-input dark outlined v-model="creation.name" label="实例名称" class="q-my-md"/><q-input dark outlined v-model="creation.group" label="分组（可选）" class="q-mb-md"/>
  <template v-if="createMode==='vanilla'">
   <div class="row q-gutter-sm q-mb-md"><q-select class="col" dark outlined v-model="creation.version" :options="minecraftVersions" use-input input-debounce="0" @filter="filterMinecraft" label="Minecraft 版本" @update:model-value="creation.loaderVersion='';loaderVersions=[]"/><q-btn label="加载版本" :disable="busy" @click="loadVersions(false)"/></div>
   <q-select dark outlined v-model="creation.loader" :options="['','fabric','forge','neoforge','quilt']" label="Mod Loader（可选）" class="q-mb-md" @update:model-value="creation.loaderVersion='';loaderVersions=[]"/>
   <div v-if="creation.loader" class="row q-gutter-sm q-mb-md"><q-select class="col" dark outlined v-model="creation.loaderVersion" :options="loaderVersions" label="Loader 版本"/><q-btn label="加载兼容版本" :disable="busy || !creation.version" @click="loadVersions(true)"/></div>
  </template>
  <template v-else-if="createMode==='server'"><q-input dark outlined v-model="creation.version" label="Minecraft 服务端版本" class="q-mb-md"/><q-input dark outlined v-model="creation.source" label="服务端 Jar 路径或下载 URL（可选）" class="q-mb-md"/></template>
  <q-input v-else dark outlined v-model="creation.source" label="整合包本地完整路径或下载 URL" class="q-mb-md"/>
  <q-btn color="primary" label="创建实例" :disable="busy || !creation.name || (createMode==='vanilla' ? (!creation.version || (!!creation.loader && !creation.loaderVersion)) : (createMode==='import' && !creation.source))" @click="create"/></template>
 </div>
  <XmclAccountWorkspace v-if="page==='accounts' && !isPclShell" :accounts="accounts" :busy="busy" :execute="catalogExecute" :pcl="false" @changed="refresh" />
 <XmclStoreWorkspace v-if="page==='store'" :busy="busy" :execute="catalogExecute" @installed="refresh" />
 <XmclMultiplayerWorkspace v-if="page==='multiplayer'" :instance="selected" :busy="busy" :execute="catalogExecute" />
 <XmclSettingsShell v-if="page==='settings'" :settings="settings" :busy="busy" :scope="settingScope()" :execute="catalogExecute" @changed="refresh" />
 <ServerPanel v-if="selected && page==='manage' && (selected.type==='server' || selected.managedType==='server')" :instance="selected.id" :busy="busy" :execute="catalogExecute" />
 </div>
</NeoUiChrome>
<q-dialog v-model="deviceShown" persistent>
 <q-card :class="isPclShell ? 'pcl-dialog pcl-device-dialog' : 'device-card'">
  <q-card-section>
   <div class="pcl-kicker">MICROSOFT 登录</div>
   <h3>在浏览器中完成登录</h3>
   <p class="pcl-muted">打开下面的地址并输入设备代码。登录完成后此窗口会自动继续；不要关闭启动器。</p>
  </q-card-section>
  <q-card-section v-if="device?.code" class="pcl-device-code">{{ device.code }}</q-card-section>
  <q-card-section>
   <code class="pcl-device-url">{{ device?.url || '正在获取登录地址…' }}</code>
   <p v-if="device?.expiresIn" class="pcl-muted">代码有效期约 {{ Math.ceil(Number(device.expiresIn) / 60) }} 分钟。</p>
  </q-card-section>
  <q-card-actions align="right">
   <PclButton icon="external-link" :disabled="!device?.url" @click="openDeviceBrowser">打开浏览器</PclButton>
   <PclButton icon="copy" :disabled="!device?.code" @click="copyDeviceCode">复制代码</PclButton>
   <PclButton variant="ghost" @click="cancelDeviceLogin">关闭并取消</PclButton>
  </q-card-actions>
 </q-card>
</q-dialog>
<PclInstanceEditWindow
 v-for="(editWindow, index) in editWindows"
 :key="editWindow.key"
 :instance="editWindow.instance"
 :index="index"
 :icon="icons[editWindow.instance.id]"
 :accounts="accounts"
 :execute="call"
 @close="editWindows = editWindows.filter(item => item.key !== editWindow.key)"
 @changed="refresh"
/>
<PclTaskCenter v-if="isPclShell" v-model="taskCenterOpen" :execute="call" />
<PclInstallProgressDialog v-model="installProgressOpen" :title="installProgressTitle" :task="installTask" :cancelling="installCancelling" @cancel="cancelInstallProgress" />
<PclToastHost />
<NeoUiOverlays :add-dialog="addDialog" :busy="busy" :execute="catalogExecute" :call="call" :is-pcl-shell="isPclShell" :status="status" :task-snapshot="taskSnapshot" :task-center-open="taskCenterOpen" :search-shown="searchShown" :global-search="globalSearch" :search-results="searchResults" :icons="icons" :dialog="dialog" :prompt="prompt" :answer="answer" :choices="choices" :confirmation="confirmation" :secret="secret" @update:add-dialog="addDialog=$event" @created="refresh" @cancel="cancel" @task-updated="handleTaskUpdate" @update:task-center-open="taskCenterOpen=$event" @update:search-shown="searchShown=$event" @update:global-search="globalSearch=$event" @search-result="openSearchResult" @dialog-finish="finishDialog" @update:answer="answer=$event" />
</div>
</template>
<script setup lang="ts">
import { nextTick, onMounted, onUnmounted, ref, computed, watch } from 'vue';
import CatalogBrowser from './components/CatalogBrowser.vue';
import InstanceExtras from './components/InstanceExtras.vue';
import ServerPanel from './components/ServerPanel.vue';
import ResourceUpdatesPanel from './components/ResourceUpdatesPanel.vue';
import XmclSidebar from './xmcl-port/XmclSidebar.vue';
import XmclDashboard from './xmcl-port/XmclDashboard.vue';
import XmclInstanceWorkspace from './xmcl-port/XmclInstanceWorkspace.vue';
import XmclAddInstanceDialog from './xmcl-port/XmclAddInstanceDialog.vue';
import XmclTaskCenter from './xmcl-port/XmclTaskCenter.vue';
import XmclSettingsShell from './xmcl-port/XmclSettingsShell.vue';
import XmclResourceWorkspace from './xmcl-port/XmclResourceWorkspace.vue';
import XmclAccountWorkspace from './xmcl-port/XmclAccountWorkspace.vue';
import XmclStoreWorkspace from './xmcl-port/XmclStoreWorkspace.vue';
import XmclMultiplayerWorkspace from './xmcl-port/XmclMultiplayerWorkspace.vue';
import XmclSystemBar from './xmcl-port/XmclSystemBar.vue';
import PclDownloadPage from './pcl/pages/download/PclDownloadPage.vue';
import PclInstallProgressDialog from './pcl/pages/download/PclInstallProgressDialog.vue';
import PclTaskCenter from './pcl/components/PclTaskCenter.vue';
import PclInstanceEditWindow from './pcl/components/PclInstanceEditWindow.vue';
import PclStandaloneInstanceEditor from './pcl/pages/settings/PclStandaloneInstanceEditor.vue';
import PclResourceDownloadWindow from './pcl/pages/download/PclResourceDownloadWindow.vue';
import PclToolsPage from './pcl/pages/tools/PclToolsPage.vue';
import PclSetupPage from './pcl/pages/settings/PclSetupPage.vue';
import PclInstancePage from './pcl/pages/instance/PclInstancePage.vue';
import PclLaunchPage from './pcl/pages/launch/PclLaunchPage.vue';
import PclAccountPage from './pcl/pages/accounts/PclAccountPage.vue';
import PclButton from './pcl/components/PclButton.vue';
import PclToastHost from './pcl/components/PclToastHost.vue';
import { showPclToast } from './pcl/composables/usePclToast';
import { initPclMusic } from './pcl/composables/usePclMusic';
import NeoUiChrome from './xmcl-port/NeoUiChrome.vue';
import NeoUiOverlays from './xmcl-port/NeoUiOverlays.vue';
import { useQuasar } from 'quasar';
import { launcher } from './boot/launcher-api';
import { neoUiPreferences, persistNeoUiPreferences } from './neo-ui-preferences';
import { useNeoDialogs } from './composables/useNeoDialogs';
const buildLabel = '2026-09-20.1';
const launchQuery = new URLSearchParams(window.location.search);
const editorInstanceId = launchQuery.get('instanceEditor') || '';
const resourceDownloadInstance = launchQuery.get('resourceDownload') || '';
const resourceDownloadKind = launchQuery.get('kind') || 'mods';
const resourceDownloadName = launchQuery.get('name') || '';
const isPclShell = computed(() => neoUiPreferences.shell === 'pcl');
const quasar=useQuasar(),connected=ref(false),icons=ref<Record<string,string>>({});
function closeMobileNav(){if(quasar.screen.lt.md)drawerOpen.value=false;}
async function loadIcons(){
 const currentIds=new Set(instances.value.map((i:any)=>i.id));
 for(const id of Object.keys(icons.value))if(!currentIds.has(id))delete icons.value[id];
 // instance.icon is read-only and must also run immediately after an import or
 // pack install, while the parent operation is still settling its busy flag.
 try{const result=await call('instance.icons');for(const icon of result?.icons||[]){if(icon?.id&&icon?.dataUrl)icons.value[icon.id]=icon.dataUrl;}}
 catch{await Promise.all(instances.value.map(async(i:any)=>{try{const result=await call('instance.icon',{instance:i.id});if(result?.dataUrl)icons.value[i.id]=result.dataUrl;}catch{ /* Keep the dashboard fallback when an old backend has no icon API. */ }}));}
}
const components=ref<any[]>([]),componentError=ref('');
async function loadComponents(){componentError.value='';components.value=[];try{const r=await call('instance.components.list',{instance:selected.value.id});components.value=r.components;}catch(e){componentError.value=message(e);}}
async function changeComponentVersion(c:any){const v=await ask('更改 '+(c.name||c.id)+' 版本',c.version);if(v!==null && v.trim())await run('instance.component.set-version',{instance:selected.value.id,component:c.id,version:v.trim()});}
const editWindows=ref<any[]>([]), instanceDetails=ref<any>(null),resourcesLoading=ref(false),resourceError=ref(''),taskSnapshot=ref<any>(null),taskCenterOpen=ref(false),searchShown=ref(false),globalSearch=ref('');
const installProgressOpen=ref(false),installProgressTitle=ref('正在安装整合包'),installTask=ref<any>(null),installCancelling=ref(false);
const logMode=ref('live'),logFiles=ref<any[]>([]),logFile=ref(''),fileLog=ref('');
const consoleLines=ref<string[]>([]),consoleState=ref('未连接'),followLog=ref(true),consoleElement=ref<HTMLElement|null>(null);
const launchQueue=ref<any[]>([])
const launchTaskId=ref('');
type LaunchUiState = 'loading' | 'disabled' | 'download' | 'launch' | 'launching' | 'launched' | 'failed' | 'cancelled';
const launchUiState=ref<LaunchUiState>('loading'), launchProgress=ref(0), launchTargetProgress=ref(0), launchStage=ref('初始化'), launchLoginMethod=ref(''), launchSpeed=ref(0), launchError=ref(''), launchDetails=ref(''), launchChildren=ref<any[]>([]), launchAccountId=ref('');
const selectedAccountStorageKey='luna-neo-selected-account';
function rememberedAccountId(){try{return localStorage.getItem(selectedAccountStorageKey)||'';}catch{return '';}}
function selectLaunchAccount(id:string){launchAccountId.value=id;try{localStorage.setItem(selectedAccountStorageKey,id);}catch{/* Storage may be unavailable in a restricted webview. */}}
let launchUiResetTimer: ReturnType<typeof setTimeout> | undefined;
const detailCache=new Map<string,any>();
let launchingId='',preparingLaunch=false;
const sessions=new Map<string,{subscription:string;lines:string[];state:string}>();
let consoleSubscription='',consoleInstance='',launchInFlight=false;
function clearConsole(){
 consoleLines.value=[];
 if(consoleInstance){const saved=sessions.get(consoleInstance);if(saved)saved.lines=[];}
}
async function appendConsole(text:string,instanceId=selected.value?.id){
 if(instanceId!==selected.value?.id){const saved=sessions.get(instanceId);if(saved){saved.lines.push(text);saved.lines.splice(0,Math.max(0,saved.lines.length-3000));}return;}
 consoleLines.value.push(text);
 if(consoleLines.value.length>3000)consoleLines.value.splice(0,consoleLines.value.length-3000);
 await nextTick();if(followLog.value && consoleElement.value)consoleElement.value.scrollTop=consoleElement.value.scrollHeight;
}
function receiveStream(batch:any){
 const entry=[...sessions.entries()].find(([,v])=>v.subscription===batch.subscriptionId);
 if(!entry)return;
 const instanceId=entry[0];
 const updateRunning=(running:boolean)=>{instances.value=instances.value.map((item:any)=>item.id===instanceId?{...item,running}:item);if(selected.value?.id===instanceId)selected.value={...selected.value,running};};
 if(entry[0]!==selected.value?.id){for(const event of batch.events||[]){if(event.kind==='console.line')entry[1].lines.push(event.text);if(event.kind==='instance.state'){entry[1].state=event.running?'客户端运行中':'客户端已停止';updateRunning(Boolean(event.running));}}entry[1].lines.splice(0,Math.max(0,entry[1].lines.length-3000));return;}
 if(batch.dropped)void appendConsole(`[日志缓冲区已省略 ${batch.dropped} 条消息]`);
 for(const event of batch.events || []){
  if(event.kind==='console.line')void appendConsole(event.text);
  else if(event.kind==='console.data')void appendConsole(new TextDecoder().decode(Uint8Array.from(atob(event.data),c=>c.charCodeAt(0))));
  else if(event.kind==='instance.state'){
   consoleState.value=event.running?'客户端运行中':'客户端已停止';
   updateRunning(Boolean(event.running));
   void appendConsole(`[${consoleState.value}]`);
  }else if(event.kind==='console.reset')consoleState.value=event.available?'控制台已连接':'等待启动';
 }
}
async function connectConsole(i:any){
 if(consoleSubscription && consoleInstance===i.id)return;
 try{
  const existing=sessions.get(i.id);if(existing?.subscription){consoleSubscription=existing.subscription;consoleInstance=i.id;consoleLines.value=existing.lines;consoleState.value=existing.state;return;}
  consoleSubscription='';consoleInstance=i.id;consoleLines.value=[];
  const sub=await call('instance.console.subscribe',{instance:i.id});consoleSubscription=sub.subscriptionId;sessions.set(i.id,{subscription:consoleSubscription,lines:consoleLines.value,state:'控制台已连接'});
  consoleState.value='控制台已连接';
 }catch(e){error.value=message(e);consoleState.value='连接失败';}
}
let launchProgressTimer: ReturnType<typeof setInterval> | undefined;
function startLaunchProgressAnimation(){
 if(launchProgressTimer)return;
 launchProgressTimer=setInterval(()=>{
  if(launchUiState.value==='launched'){launchTargetProgress.value=1;launchProgress.value=1;return;}
  if(launchUiState.value!=='launching')return;
  const actual=Math.max(0,Math.min(1,launchTargetProgress.value));
  if(actual>=launchProgress.value) launchProgress.value=Math.min(actual,launchProgress.value+(actual-launchProgress.value)*0.2+0.005);
  else launchProgress.value=actual;
 },200);
}
function stopLaunchProgressAnimation(){if(launchProgressTimer){clearInterval(launchProgressTimer);launchProgressTimer=undefined;}}
function setLaunchState(state:LaunchUiState){launchUiState.value=state;if(state==='launching'||state==='launched')startLaunchProgressAnimation();else stopLaunchProgressAnimation();}
function resetLaunchUiSoon(){if(launchUiResetTimer)clearTimeout(launchUiResetTimer);launchUiResetTimer=setTimeout(()=>{if(launchUiState.value==='launched' || launchUiState.value==='failed' || launchUiState.value==='cancelled'){setLaunchState(selected.value ? 'launch' : (instances.value.length ? 'disabled' : 'download'));launchError.value='';}},2200);}
function updateLaunchProgress(value:any){
 if(!value)return;
 if(!launchInFlight && !preparingLaunch && !['launching','launched'].includes(launchUiState.value))return;
 const total=Number(value.totalProgress ?? value.total ?? 0), current=Number(value.progress ?? value.current ?? 0);
  if(total>0 && Number.isFinite(current)) launchTargetProgress.value=Math.max(0,Math.min(1,current/total));
  else if(value.finished || value.running===false) launchTargetProgress.value=1;
 launchStage.value=value.details || value.status || launchStage.value || '正在执行';
 launchDetails.value=String(value.details || '');
 launchChildren.value=Array.isArray(value.children) ? value.children : [];
 launchSpeed.value=Number(value.transferRate || 0);
 if(value.failReason || String(value.state || value.status || '').toLowerCase()==='failed'){launchError.value=value.failReason || value.details || '启动任务失败';setLaunchState('failed');}
 else if(!isTaskFinished(value) && (launchInFlight || preparingLaunch))setLaunchState('launching');
}
async function launch(i:any, accountId?:string, serverAddress?:string, restoreSelectionId?:string){
 if(busy.value){if(i.id!==launchingId&&!launchQueue.value.some(x=>x.id===i.id))launchQueue.value.push(i);return;}
 if(launchUiResetTimer)clearTimeout(launchUiResetTimer);
  launchProgress.value=0;launchTargetProgress.value=0;launchStage.value='初始化';launchSpeed.value=0;launchError.value='';launchDetails.value='';launchChildren.value=[];setLaunchState('launching');
 busy.value=true;launchingId=i.id;preparingLaunch=true;
 await openInstance(i,true);if(isPclShell.value)page.value='instances';logMode.value='live';await connectConsole(i);preparingLaunch=false;
 if(!consoleSubscription){busy.value=false;launchingId='';setLaunchState('failed');launchError.value='无法连接实例控制台';resetLaunchUiSoon();return;}
 busy.value=true;launchInFlight=true;error.value='';notice.value='';
 try{
  const parameters:any={instance:i.id};
  const validAccount=accountId && accounts.value.some((account:any)=>String(account.internalId || account.id || account.profileName || '')===accountId);
  const fallbackAccount=accounts.value.find((account:any)=>account.default) || accounts.value[0];
  const launchAccount=validAccount ? accountId : String(fallbackAccount?.internalId || fallbackAccount?.id || fallbackAccount?.profileName || '');
  if(launchAccount)parameters.account=launchAccount;
  if(serverAddress)parameters.server=serverAddress;
  let task:any=await call('neo.task.start',{operation:'instance.launch',priority:'high',parameters});
  launchTaskId.value=task?.taskId || '';
  while(task && !task.finished){
    handleTaskUpdate(task);
    await new Promise(resolve=>setTimeout(resolve,350));
    task=await call('neo.task.status',{taskId:task.taskId});
  }
  handleTaskUpdate(task);
  if(!task || task.state!=='succeeded')throw new Error(task?.error || '启动失败');
  launchTargetProgress.value=1;launchProgress.value=1;launchStage.value='等待游戏窗口';setLaunchState('launched');
  notice.value=`启动请求已返回（PID ${task.pid}），请查看控制台确认客户端运行状态。`;resetLaunchUiSoon();
 }catch(e){error.value=message(e);if(launchUiState.value!=='cancelled'){launchError.value=error.value;setLaunchState('failed');}void appendConsole('[启动失败] '+error.value,launchingId);resetLaunchUiSoon();}
 finally{busy.value=false;launchInFlight=false;launchingId='';launchTaskId.value='';device.value=null;status.value='';taskSnapshot.value=null;taskCenterOpen.value=false;if(restoreSelectionId&&restoreSelectionId!==i.id){const previous=instances.value.find((item:any)=>String(item.id)===restoreSelectionId);if(previous){selected.value=previous;rememberInstance(previous);void call('instance.info',{instance:previous.id}).then((info:any)=>{if(selected.value?.id===previous.id){instanceDetails.value=info;notes.value=info.notes||'';}}).catch(()=>{});}}}
}
function quickLaunchHome(instance:any, switchSelected:boolean){
 if(!instance?.id)return;
 const previous=selected.value?.id || '';
 void launch(instance, launchAccountId.value, undefined, switchSelected ? '' : previous);
}
async function openInstanceEditor(instance: any) {
  if (!instance?.id) return;
  try {
    await launcher.openInstanceEditor(instance.id, instance.name || '');
    return;
  } catch {
    // Browser preview and older desktop builds do not expose the native window command.
  }
  if (editWindows.value.some(window => window.instance.id === instance.id)) return;
  editWindows.value.push({ key: `edit-${Date.now()}-${Math.random().toString(36).slice(2, 6)}`, instance: { ...instance } });
}
function editInstance(instance: any) {
  if (!instance?.id) return;
  if (selected.value?.id === instance.id && !neoUiPreferences.pclEditCurrentInWindow) {
    void openInstance(instance);
    return;
  }
  void openInstanceEditor(instance);
}
function menuLaunch(instance: any) {
  if (!instance?.id) return;
  void launch(instance, launchAccountId.value, undefined, selected.value?.id || '');
}
async function openInstanceFolder(instance: any) {
  if (!instance?.id) return;
  try { await call('instance.open-folder', { instance: instance.id }); }
  catch (exception) { error.value = message(exception); }
}
function quickConnect(address:string){
 if(!selected.value?.id || !address)return;
 void launch(selected.value, launchAccountId.value, address);
}
function launchMany(items:any[]){
 const queue=items.filter((item:any)=>!item.running && item.id);
 if(!queue.length)return;
 if(busy.value){for(const item of queue)if(item.id!==launchingId&&!launchQueue.value.some(x=>x.id===item.id))launchQueue.value.push(item);return;}
 launchQueue.value=queue.slice(1);
 void launch(queue[0]);
}
async function loadLogFiles(){try{logFiles.value=await call('instance.log.list',{instance:selected.value.id});}catch(e){error.value=message(e);}}
async function readLogFile(){try{const r=await call('instance.log.read',{instance:selected.value.id,file:logFile.value,maxBytes:1048576});fileLog.value=r.content+(r.truncated?'\n[文件超过 1 MiB，仅显示前部分]':'');}catch(e){error.value=message(e);}}
const resources=ref<any[]>([]),resourceKind=ref('mods'),resourceSource=ref('');
const resourceKinds=[{label:'模组',value:'mods'},{label:'资源包',value:'resourcepacks'},{label:'光影包',value:'shaderpacks'},{label:'材质包',value:'texturepacks'},{label:'数据包',value:'datapacks'},{label:'投影',value:'schematics'},{label:'Yes Steve Model',value:'yesstevemodels'},{label:'Customizable Player Models',value:'customplayermodels'}];
const pages=[{id:'instances',label:'实例库',icon:'apps'},{id:'create',label:'添加实例',icon:'add_box'},{id:'accounts',label:'账户管理',icon:'person'},{id:'settings',label:'设置',icon:'settings'}];
const drawerOpen=ref(true), addDialog=ref(false), page=ref('instances'), busy=ref(false), error=ref(''), notice=ref(''), status=ref(''), search=ref(''), username=ref(''), notes=ref('');
let lastErrorNotification='';
watch(error,(value)=>{if(value && value!==lastErrorNotification){lastErrorNotification=value;if(isPclShell.value){showPclToast({type:'error',title:'操作失败',message:value,timeout:6000});}else{quasar.notify({type:'negative',message:value,timeout:6000,position:'top',progress:true,actions:[{icon:'close',color:'white'}]});}}});
watch(notice,(value)=>{if(!value)return;if(isPclShell.value){showPclToast({type:'success',title:'操作完成',message:value,timeout:3500});}else{quasar.notify({type:'positive',message:value,timeout:3500,position:'top',progress:true});}});
watch(busy,async value=>{if(!value && launchQueue.value.length){await nextTick();if(!busy.value){const next=launchQueue.value.shift();if(next)void launch(next);}}});
watch(isPclShell,(value)=>{scope.value='launcher';if(value && page.value==='settings')page.value='pcl-settings';else if(!value && page.value==='pcl-settings')page.value='settings';void refresh();});
const instances=ref<any[]>([]),accounts=ref<any[]>([]),settings=ref<any[]>([]),selected=ref<any>(null),device=ref<any>(null),deviceShown=ref(false);
const selectedInstanceStorageKey='luna-neo-selected-instance';
function rememberedInstanceId(){
 const preferred=String(neoUiPreferences.selectedInstanceId || '').trim();
 if(preferred)return preferred;
 try{return localStorage.getItem(selectedInstanceStorageKey)||'';}catch{return '';}
}
function rememberInstance(instance:any){
 if(!instance?.id)return;
 const id=String(instance.id);
 neoUiPreferences.selectedInstanceId=id;
 persistNeoUiPreferences();
 try{localStorage.setItem(selectedInstanceStorageKey,id);}catch{/* Storage may be unavailable in a restricted webview. */}
}
function clearRememberedInstance(){
 neoUiPreferences.selectedInstanceId='';
 persistNeoUiPreferences();
 try{localStorage.removeItem(selectedInstanceStorageKey);}catch{/* Storage may be unavailable in a restricted webview. */}
}
const searchResults=computed(()=>{const q=globalSearch.value.trim().toLowerCase();if(!q)return instances.value.slice(0,12);return instances.value.filter((i:any)=>`${i.name} ${i.id} ${i.group||''} ${i.type||''} ${i.runtime?.minecraft||i.minecraftVersion||''}`.toLowerCase().includes(q)).slice(0,20);});
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
const { dialog, prompt, answer, choices, secret, confirmation, ask, finishDialog, message } = useNeoDialogs();
const readOnlyOperations=new Set([
 'task.status','task.list','task.cancel','neo.task.status','neo.task.list','neo.task.cancel','api.describe','scheduler.status','aria2.downloads','aria2.cancel','aria2.clear-finished','instance.open-folder','instance.folder-path',
 'settings.list','runtime.info','runtime.memory','account.preset.list','authentication.helper.status',
 'language.list','launcher.update.status','component.catalog','component.versions'
]);
async function catalogExecute(op:string,p:Record<string,unknown>){
 if(busy.value && !readOnlyOperations.has(op)){taskCenterOpen.value=true;throw new Error('当前任务仍在执行，请先在任务中心查看或取消任务。');}
 if(readOnlyOperations.has(op))return call(op,p);
 busy.value=true;try{return await call(op,p);}finally{busy.value=false;status.value='';taskSnapshot.value=null;taskCenterOpen.value=false;if(op==='account.login'){deviceShown.value=false;device.value=null;}}
}
async function call(op:string,p:Record<string,unknown>={}){const r=await launcher.execute<any>(op,p);if(!r.ok)throw new Error(r.error||'操作失败');connected.value=true;return r.data;}
function settingScope(){return scope.value==='instance' ? {scope:'instance',instance:selected.value.id} : {scope:'launcher'};}
let resourceRequest=0;
async function loadResources(){const request=++resourceRequest;const id=selected.value.id,kind=resourceKind.value;resources.value=[];resourceError.value='';resourcesLoading.value=true;try{const data=await call('resource.list',{instance:id,kind});if(request===resourceRequest){if(!Array.isArray(data))throw new Error('资源接口返回格式错误');resources.value=data;}}catch(e){if(request===resourceRequest)resourceError.value=message(e);}finally{if(request===resourceRequest)resourcesLoading.value=false;}}
async function refresh(){error.value='';try{if(page.value==='accounts')accounts.value=await call('account.list');else if(page.value==='settings' || page.value==='pcl-settings')settings.value=await call('settings.list',settingScope());else if(page.value==='manage'){await loadResources();await loadComponents();await loadLogFiles();}else {instances.value=await call('instance.list');await loadIcons();if(isPclShell.value){const remembered=rememberedInstanceId();const rememberedInstance=instances.value.find((item:any)=>String(item.id)===remembered);if(!selected.value || !instances.value.some((item:any)=>item.id===selected.value.id))selected.value=rememberedInstance || instances.value[0] || null;if(selected.value)rememberInstance(selected.value);else clearRememberedInstance();accounts.value=await call('account.list');const rememberedAccount=rememberedAccountId();const defaultAccount=accounts.value.find((account:any)=>account.default);const accountKeyOf=(account:any)=>String(account?.internalId || account?.id || account?.profileName || '');const currentAccountExists=accounts.value.some((account:any)=>accountKeyOf(account)===launchAccountId.value);if(!launchAccountId.value || !currentAccountExists){const saved=accounts.value.find((account:any)=>accountKeyOf(account)===rememberedAccount);launchAccountId.value=accountKeyOf(saved) || accountKeyOf(defaultAccount) || accountKeyOf(accounts.value[0]);}}if(!launchInFlight && !['launched','failed','cancelled'].includes(launchUiState.value))setLaunchState(instances.value.length ? (selected.value ? 'launch' : 'disabled') : 'download');}}catch(e){error.value=message(e);}}
async function navigate(id:string){
 if(preparingLaunch)return;
 const target=!isPclShell.value && id==='pcl-download'?'store':!isPclShell.value && id==='pcl-tools'?'accounts':!isPclShell.value && id==='pcl-settings'?'settings':id;
 page.value=target;
 error.value='';
 notice.value='';
 if(target==='settings' || target==='pcl-settings')scope.value='launcher';
 // A background download/install must not lock the whole UI. Render the
 // target page immediately; its own refresh will queue behind the task and
 // fill in when the backend becomes free.
 if(busy.value)return;
 await refresh();
}
function openPclCreate(uid?:string,version?:string){createMode.value='vanilla';creation.value={...creation.value,name:creation.value.name || '新建实例',version:uid==='net.minecraft'?(version || ''):creation.value.version,loader:uid==='net.fabricmc.fabric-loader'?'fabric':uid==='net.minecraftforge'?'forge':uid==='net.neoforged'?'neoforge':uid==='org.quiltmc.quilt-loader'?'quilt':creation.value.loader,loaderVersion:uid && uid!=='net.minecraft'?(version || ''):creation.value.loaderVersion};page.value='create';}
function openGlobalSearch(){globalSearch.value='';searchShown.value=true;}
function openSearchResult(instance:any){searchShown.value=false;void openInstance(instance);}
function xmclTab(tab:string){if(busy.value){taskCenterOpen.value=true;return;}if(tab==='home'){page.value='manage';return;}if(tab==='mods'||tab==='resourcepacks'||tab==='shaderpacks'){resourceKind.value=tab;page.value='manage';void loadResources();return;}if(tab==='save'){page.value='manage';return;}if(tab==='settings'){scope.value='instance';page.value='settings';void refresh();}}
async function run(op:string,p:Record<string,unknown>={}){if(busy.value)return;busy.value=true;error.value='';notice.value='';device.value=null;try{await call(op,p);notice.value='操作已完成';await refresh();}catch(e){error.value=message(e);}finally{busy.value=false;status.value='';device.value=null;taskSnapshot.value=null;taskCenterOpen.value=false;}}
async function create(){const p:any={...creation.value};for(const k of Object.keys(p))if(!p[k])delete p[k];if(createMode.value==='import'){p.type='import';}else if(createMode.value==='server'){p.type='server';}await run(createMode.value==='import'?'instance.import':'instance.create',p);if(!error.value){page.value='instances';await refresh();}}
async function onPclInstalled(result:any){
 await refresh();
 const created=instances.value.find((item:any)=>String(item.id)===String(result?.id || '')) || instances.value.find((item:any)=>item.name===result?.name);
 if(created){selected.value=created;rememberInstance(created);}
 page.value='instances';
 await refresh();
}
async function openPclInstancePage(){if(!selected.value)return;await openInstance(selected.value);}
async function selectPclInstance(i:any){
 if(!i || preparingLaunch) return;
 selected.value=i;
 rememberInstance(i);
 page.value='instances';
 instanceDetails.value=null;
 notes.value='';
 error.value='';
 if(!launchInFlight && !['launched','failed','cancelled'].includes(launchUiState.value)) setLaunchState('launch');
 // Only the launch header needs the instance summary. Resource/component/log
 // scans are workspace data and must not block switching clients.
 void call('instance.info',{instance:i.id}).then(info=>{
  if(selected.value?.id===i.id){instanceDetails.value=info;notes.value=info.notes || '';}
 }).catch(()=>{/* The launch page can render from instance.list alone. */});
}
async function openInstance(i:any,forceRead=false){
 if(preparingLaunch&&!forceRead)return;
 if(busy.value&&!forceRead)return;
 if(selected.value)detailCache.set(selected.value.id,{info:instanceDetails.value,notes:notes.value,components:components.value,resources:resources.value,kind:resourceKind.value,logFiles:logFiles.value});
 if(selected.value?.id!==i.id){
  const old=sessions.get(selected.value?.id);if(old){old.lines=consoleLines.value;old.state=consoleState.value;}
  const saved=sessions.get(i.id);consoleSubscription=saved?.subscription||'';consoleInstance=i.id;consoleLines.value=saved?.lines||[];consoleState.value=saved?.state||'未连接';logFile.value='';fileLog.value='';logFiles.value=[];
 }
selected.value=i;rememberInstance(i);page.value='manage';if(!launchInFlight && !['launched','failed','cancelled'].includes(launchUiState.value))setLaunchState('launch');notes.value='';error.value='';instanceDetails.value=null;
 try{const info=await call('instance.info',{instance:i.id});instanceDetails.value=info;notes.value=info.notes || '';}catch(e){error.value=message(e);}
 await loadResources();await loadComponents();await loadLogFiles();
}
async function changeIcon(){const path=await ask('图标文件完整路径（PNG / ICO 等）');if(path===null||!path.trim())return;await run('instance.set-icon',{instance:selected.value.id,path:path.trim()});if(!error.value){const icon=await call('instance.icon',{instance:selected.value.id});icons.value[selected.value.id]=icon.dataUrl;}}
async function instanceAction(op:string,field:string|undefined,label:string){const p:any={instance:selected.value.id};if(field){const v=await ask(label,field==='group'?(selected.value.group||''):selected.value.name);if(v===null)return;p[field]=v;}await run('instance.'+op,p);}
async function workspaceAction(op:string){const labels:Record<string,string>={rename:'重命名',copy:'复制实例',group:'修改分组'};if(labels[op]){await instanceAction(op,op==='group'?'group':'name',labels[op]);return;}await instanceAction(op,undefined,op);}
async function instanceSelectionAction(instance:any,op:string){
 if(!instance?.id)return;
 if(op==='delete'){if(await ask(`确认移除 ${instance.name}？`,'',true))await run('instance.delete',{instance:instance.id,confirm:true});return;}
 const labels:Record<string,string>={rename:'重命名',group:'移动到分组',copy:'复制实例'};
 if(labels[op]){const field=op==='group'?'group':'name';const value=await ask(labels[op],field==='group'?(instance.group||''):(instance.name||''));if(value===null)return;await run(`instance.${op}`,{instance:instance.id,[field]:value});return;}
 if(op==='verify'){await run('instance.verify',{instance:instance.id});return;}
 if(op==='stop'){await run('instance.stop',{instance:instance.id});return;}
 if(op==='shortcut'){await run('instance.create-shortcut',{instance:instance.id,target:'desktop'});return;}
 if(op==='export'){await openInstance(instance);notice.value='已打开实例页，请在“导出”中完成导出。';return;}
}
async function askDelete(op:string,p:any,name:string){if(await ask('确认移除 '+name+'？','',true))await run(op,{...p,confirm:true});}
async function editSetting(s:any){const value=await ask('编辑 '+s.key,JSON.stringify(s.value));if(value===null)return;try{await run('settings.set',{...settingScope(),key:s.key,value:JSON.parse(value)});}catch(e){error.value='请输入有效的 JSON 值，例如 true、1024 或 "文本"';}}
function isTaskFinished(value:any){return Boolean(value?.finished) || value?.running === false || ['succeeded','failed','aborted','finished','completed','cancelled','canceled'].includes(String(value?.state || value?.status || '').toLowerCase());}
function handleTaskUpdate(value:any){
 if(!value)return;
 if(installProgressOpen.value)installTask.value=value;
 updateLaunchProgress(value);
 if(isTaskFinished(value)){if(launchInFlight && (value.failReason || value.successful===false)){launchError.value=value.failReason || value.details || '启动任务失败';setLaunchState('failed');}taskSnapshot.value=null;taskCenterOpen.value=false;status.value='';}
 else {taskSnapshot.value=value;status.value=value.status || value.state || '正在执行';}
}
async function installComponentVersion(component:any,version:string){
 if(!selected.value)return;
 beginInstallProgress({ title: `正在安装 ${component?.name || component?.id || '组件'}`, name: version });
 try {
  let task:any=await catalogExecute('neo.task.start',{operation:'instance.component.set-version',priority:'high',parameters:{instance:selected.value.id,component:component.id,version}});
  while(task && !task.finished){
   installTask.value=task;
   await new Promise(resolve=>setTimeout(resolve,300));
   task=await catalogExecute('neo.task.status',{taskId:task.taskId});
  }
  installTask.value=task;
  if(!task || task.state!=='succeeded')throw new Error(task?.error || '组件安装失败');
  await refresh();
 } catch(exception){error.value=message(exception);}
 finally { endInstallProgress(); }
}
function beginInstallProgress(info:any){installProgressTitle.value=info?.title || '正在安装整合包';installTask.value=null;installCancelling.value=false;installProgressOpen.value=true;}
function endInstallProgress(){
 const task=installTask.value,title=installProgressTitle.value;
 const state=String(task?.state||'').toLowerCase();
 if(state==='succeeded' && isPclShell.value){
  const completed=title.replace(/^正在/,'').replace(/：.*$/,'');
  showPclToast({type:'success',title:'操作完成',message:`${completed}完成`,timeout:3500});
 }
 installProgressOpen.value=false;installCancelling.value=false;installTask.value=null;
}

async function exportInstance(payload:any){
  if(!selected.value)return;
  beginInstallProgress({title:'正在导出实例',name:selected.value.name});
  try{
    let task:any=await call('neo.task.start',{operation:'instance.export',priority:'high',parameters:{instance:selected.value.id,output:payload.output,format:payload.format||'zip',name:payload.name,version:payload.version,summary:payload.summary,author:payload.author,optionalFiles:Boolean(payload.optionalFiles),overwrite:payload.overwrite!==false}});
    while(task && !task.finished){installTask.value=task;await new Promise(resolve=>setTimeout(resolve,400));task=await call('neo.task.status',{taskId:task.taskId});}
    installTask.value=task;
    if(!task || task.state!=='succeeded')throw new Error(task?.error||'导出失败');
    notice.value='实例已导出到 '+(payload.output||'');
  }catch(e){error.value=message(e);}
  finally{endInstallProgress();}
}
async function cancelInstallProgress(){installCancelling.value=true;try{if(installTask.value?.taskId)await launcher.execute('neo.task.cancel',{taskId:installTask.value.taskId});else await launcher.execute('task.cancel',{});}catch{/* the task may already be terminal */}}
async function cancel(){try{if(launchTaskId.value)await call('neo.task.cancel',{taskId:launchTaskId.value});else await call('task.cancel');if(launchInFlight || launchUiState.value==='launching'){setLaunchState('cancelled');launchError.value='启动已取消';resetLaunchUiSoon();}}catch(e){error.value=message(e);}}
async function openFolder(){
 if(!selected.value)return;
 try{
  // Always resolve the current instance. Cached details can belong to the
  // previously selected instance when the user switches tabs quickly.
  const info=await call('instance.get',{instance:selected.value.id});
  const path=info?.paths?.root;
  if(!path)throw new Error('实例目录尚未读取');
  await launcher.openPath(path);
  if(isPclShell.value) showPclToast({type:'success',title:'目录已打开',message:path,timeout:2500});
  else quasar.notify({type:'positive',message:`已打开实例目录：${path}`,timeout:2500,position:'top',progress:true});
 }catch(e){error.value=message(e);}
}
async function copyText(value:string){try{await navigator.clipboard?.writeText(value);notice.value='已复制到剪贴板';}catch{/* Clipboard may be unavailable in the web preview. */}}
async function cancelDeviceLogin(){
 deviceShown.value=false;device.value=null;
 try{await launcher.execute('task.cancel',{});}catch{/* the login may already have finished */}
}
async function openDeviceBrowser(){const url=String(device.value?.url || '');if(!url)return;try{await call('desktop.open-url',{url});}catch(e){error.value=message(e);}}
async function copyDeviceCode(){const code=String(device.value?.code || '');if(code)await copyText(code);}
let unlisten:(()=>void)|undefined,unlistenStream:(()=>void)|undefined,unlistenExit:(()=>void)|undefined;
onMounted(async()=>{if(editorInstanceId || resourceDownloadInstance)return;void initPclMusic();try{unlistenStream=await launcher.onStream(receiveStream);unlistenExit=await launcher.onExit((reason)=>{connected.value=false;sessions.clear();consoleSubscription='';consoleState.value='后端已退出';instances.value=instances.value.map((item:any)=>({...item,running:false}));if(selected.value)selected.value={...selected.value,running:false};error.value=reason.message || '启动器后端已退出，请刷新重连。';});unlisten=await launcher.onEvent(async(raw:any)=>{if(raw.kind==='input'){const opts=raw.choices?.map((x:any,n:number)=>({label:typeof x==='string'?x:(x.name||x.label||JSON.stringify(x)),value:n}));const v=await ask(raw.prompt,'',false,opts||null,raw.secret);try{await launcher.respond(v===null?{interactionId:raw.interactionId,cancel:true}:{interactionId:raw.interactionId,value:v});}catch(e){error.value=message(e);}}else if(raw.kind==='device_code'){device.value=raw.data || raw;deviceShown.value=true;}else if(raw.kind==='status'){const messageText=raw.message || raw.data?.message || (typeof raw.data==='string'?raw.data:'正在执行');if(launchInFlight){launchStage.value=messageText;setLaunchState('launching');void appendConsole('[启动] '+messageText,launchingId);}status.value=messageText;}else if(raw.kind==='task'){handleTaskUpdate(raw.data || null);}});await refresh();}catch(e){error.value=message(e);}});
onUnmounted(()=>{stopLaunchProgressAnimation();unlisten?.();unlistenStream?.();unlistenExit?.();});
</script>
<style scoped>
.pcl-device-dialog{width:520px;max-width:94vw;background:var(--pcl-c6)!important}
.pcl-device-code{margin:0 16px;padding:14px;border:1px solid var(--pcl-card-border);border-radius:4px;background:var(--pcl-c7);color:var(--pcl-c2);font:700 26px/1.2 ui-monospace,Consolas,monospace;letter-spacing:.18em;text-align:center;user-select:all}
.pcl-device-url{display:block;padding:8px 10px;overflow-wrap:anywhere;border:1px solid var(--pcl-card-border);border-radius:4px;background:var(--pcl-c8);color:var(--pcl-text);font:12px/1.5 ui-monospace,Consolas,monospace;user-select:all}
.device-card{width:500px;max-width:94vw;background:rgba(31,31,40,.96)!important;border:1px solid rgba(255,255,255,.12);border-radius:20px}.device-card :deep(.q-btn){text-transform:none}.device-code{padding:18px;text-align:center;border-radius:12px;background:rgba(99,102,241,.18);font:700 28px ui-monospace,SFMono-Regular,Consolas,monospace;letter-spacing:.18em}.form{max-width:720px}.console{height:360px;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;background:#0a0e16;padding:16px;font-size:12px;user-select:text;border:1px solid rgba(255,255,255,.06);border-radius:12px}.workspace-heading{display:flex;justify-content:space-between;align-items:flex-end;gap:24px;margin:12px 0 28px}.workspace-heading h1{margin:3px 0 6px;font-size:32px;letter-spacing:-.03em}.workspace-heading p{margin:0;color:var(--neo-muted)}.eyebrow{color:var(--neo-primary);font-size:11px;font-weight:700;letter-spacing:.14em}.heading-actions{display:flex;gap:10px}.library-toolbar{display:flex;align-items:center;gap:20px;padding:6px 14px;margin-bottom:22px;background:var(--neo-panel);border:1px solid var(--neo-border);border-radius:14px}.library-search{flex:1}.library-count{color:var(--neo-muted);font-size:13px}.library-count strong{color:white;font-size:18px;margin-right:4px}.instance-card{overflow:hidden;background:var(--neo-panel)!important;border:1px solid var(--neo-border);border-radius:16px;transition:transform .18s,border-color .18s,box-shadow .18s}.instance-card:hover{transform:translateY(-3px);border-color:rgba(129,140,248,.55);box-shadow:0 14px 36px rgba(0,0,0,.25)}.instance-art{height:112px;padding:16px;display:flex;align-items:flex-end;justify-content:space-between;background:linear-gradient(135deg,#312e81,#4f46e5 50%,#0f172a);color:#fff}.instance-art span{padding:5px 9px;border-radius:20px;background:rgba(0,0,0,.3);font-size:11px}.instance-name{font-size:18px;font-weight:700}.instance-meta{display:flex;align-items:center;gap:5px;margin-top:8px;color:var(--neo-muted);font-size:13px}.instance-id{margin-top:5px;color:#64748b;font-size:11px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.instance-actions{border-top:1px solid var(--neo-border);padding:10px 14px}.instance-actions .q-btn:first-child{flex:1}.neo-page :deep(h5),.neo-page :deep(h6){letter-spacing:-.015em}.neo-page :deep(.q-card){border-radius:16px}.neo-page :deep(.q-field--outlined .q-field__control){border-radius:10px}.neo-page :deep(.q-btn){border-radius:9px;text-transform:none;font-weight:600}.neo-page :deep(.q-banner){border-radius:10px}.neo-page :deep(.q-item){border-radius:9px}.neo-page :deep(.q-tab){text-transform:none}.neo-page :deep(.q-list--bordered){border-color:var(--neo-border);border-radius:12px;overflow:hidden}
.global-search{width:620px;max-width:94vw;background:rgba(29,29,38,.98)!important;border:1px solid rgba(255,255,255,.12);border-radius:16px}.global-search :deep(.q-item){min-height:58px}.global-search :deep(.q-avatar){background:rgba(255,255,255,.08)}
.pcl-route-enter-active,.pcl-route-leave-active{transition:opacity .18s,transform .35s cubic-bezier(.16,1.3,.3,1)}.pcl-route-enter-from{opacity:0;transform:translateX(20px)}.pcl-route-leave-to{opacity:0;transform:translateX(-20px)}
@media(max-width:700px){.workspace-heading{display:block}.heading-actions{margin-top:18px}.heading-actions .q-btn{flex:1}.library-toolbar{padding-left:10px}.library-count{display:none}}

.pcl-standalone-root {
  width: 100vw;
  height: 100vh;
  overflow: hidden;
  background: var(--pcl-page-bg);
}
</style>
