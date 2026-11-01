<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
<div v-if="editorInstanceId" class="pcl-standalone-root">
 <EndfieldStandaloneEditor v-if="isEndfieldShell" :instance-id="editorInstanceId" />
 <PclStandaloneInstanceEditor v-else :instance-id="editorInstanceId" />
</div>
<div v-else-if="resourceDownloadInstance" class="pcl-standalone-root">
 <EndfieldResourceDownloadWindow v-if="isEndfieldShell" :instance-id="resourceDownloadInstance" :instance-name="resourceDownloadName" :kind="resourceDownloadKind" />
 <PclResourceDownloadWindow v-else :instance-id="resourceDownloadInstance" :instance-name="resourceDownloadName" :kind="resourceDownloadKind" />
</div>
<div v-else class="xmcl-app text-white">
<EndfieldShell v-if="isEndfieldShell" />
<NeoUiChrome v-else :connected="connected" :busy="busy" :page="page" :instance-name="selected?.name" :is-pcl-shell="isPclShell" :instances="instances" :selected="selected" :icons="icons" @refresh="refresh" @tasks="taskCenterOpen=true" @search="openGlobalSearch" @navigate="navigate" @select="openInstance" @launch="launch" @back="()=>navigate('instances')" @add="addDialog=true">
<div class="xmcl-content neo-page">
 <NeoPclPageHost v-show="isPclShell" :page="page" :route-animation="pclRouteAnimation" :instances="instances" :icons="icons" :selected="selected" :busy="busy" :notice="notice" :error="error" :accounts="accounts" :account-id="launchAccountId" :launch-state="launchUiState" :launch-progress="launchProgress" :launch-stage="launchStage" :launch-login-method="launchLoginMethod" :launch-speed="launchSpeed" :launch-details="launchDetails" :launch-children="launchChildren" :launch-error="launchError" :log-lines="consoleLines" :console-monitor-id="consoleMonitorId" :console-monitor-options="consoleMonitorOptions" :execute="catalogExecute" :settings="settings" :scope="settingScope()" :details="instanceDetails" :icon="selected ? icons[selected.id] : undefined" :components="components" :component-error="componentError" :notes="notes" :console-lines="consoleLines" :console-state="consoleState" :log-files="logFiles" :file-log="fileLog" :download-component-uid="downloadComponentUid" @select="selectPclInstance" @quick-launch="quickLaunchHome" @connect-server="quickConnect" @launch-instance="menuLaunch" @edit-instance="editInstance" @open-instance-folder="openInstanceFolder" @instance-action="instanceSelectionAction" @launch="selected && launch(selected, launchAccountId)" @cancel="cancel" @download="navigate('pcl-download')" @account-change="selectLaunchAccount" @account-page="navigate('accounts')" @open-folder="openFolder" @settings="openPclInstancePage" @refresh="refresh" @tasks="taskCenterOpen=true" @clear-log="clearConsole" @select-console-instance="selectConsoleInstance" @download-launch="navigate('instances')" @installed="onPclInstalled" @install-start="beginInstallProgress" @install-progress="installTask = $event" @install-done="endInstallProgress" @open-instance="openInstance" @changed="refresh" @cancel-login="cancelDeviceLogin" @manage-launch="selected && launch(selected)" @manage-open-folder="openFolder" @manage-settings="scope='instance';page='pcl-settings';refresh()" @delete="selected && askDelete('instance.delete',{instance:selected.id},selected.name)" @change-icon="changeIcon" @save-notes="run('instance.set-notes',{instance:selected.id,notes:$event})" @connect-console="selected && connectConsole(selected)" @clear-console="clearConsole" @read-log="(file)=>{logFile=file;readLogFile()}" @component-enabled="(component,enabled)=>run('instance.component.set-enabled',{instance:selected.id,component:component.id,enabled})" @component-version="(component,version)=>installComponentVersion(component,version)" @browse="openComponentDownload" @export="exportInstance" @save-setting="(key,value)=>run('settings.set',{scope:'instance',instance:selected.id,key,value})" @action="workspaceAction" />
 <NeoXmclPageHost v-if="!isPclShell" :page="page" :instances="instances" :icons="icons" :selected="selected" :busy="busy" :execute="catalogExecute" :launch-queue="launchQueue" :details="instanceDetails" :icon="selected ? icons[selected.id] : undefined" :components="components" :component-error="componentError" :notes="notes" :console-lines="consoleLines" :console-state="consoleState" :log-files="logFiles" :file-log="fileLog" :creation="creation" :create-mode="createMode" :accounts="accounts" :settings="settings" :scope="settingScope()" @select="openInstance" @launch="launch" @batch-launch="launchMany" @add="addDialog=true" @refresh="refresh" @clear-queue="launchQueue=[]" @open-folder="openFolder" @settings="scope='instance';page='settings';refresh()" @delete="selected && askDelete('instance.delete',{instance:selected.id},selected.name)" @change-icon="changeIcon" @save-notes="run('instance.set-notes',{instance:selected.id,notes:$event})" @connect-console="selected && connectConsole(selected)" @clear-console="clearConsole" @read-log="(file)=>{logFile=file;readLogFile()}" @component-enabled="(component,enabled)=>run('instance.component.set-enabled',{instance:selected.id,component:component.id,enabled})" @component-version="(component,version)=>run('instance.component.set-version',{instance:selected.id,component:component.id,version})" @action="workspaceAction" @create="create" @installed="refresh" @changed="refresh" />
 </div>
</NeoUiChrome>
<NeoDeviceLoginDialog v-model="deviceShown" :is-pcl-shell="isPclShell" :device="device" @open-browser="openDeviceBrowser" @copy-code="copyDeviceCode" @cancel="cancelDeviceLogin" />
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
<PclTaskCenter v-if="isPclShell && !isEndfieldShell" v-model="taskCenterOpen" :execute="call" />
<PclInstallProgressDialog v-model="installProgressOpen" :title="installProgressTitle" :task="installTask" :cancelling="installCancelling" @cancel="cancelInstallProgress" @close="closeInstallProgress" />
<PclToastHost />
<TaskInteractionDialog v-if="!editorInstanceId && !resourceDownloadInstance && !isEndfieldShell" :execute="call" :pcl="isPclShell" :legacy="legacyTaskInteraction" @legacy-finished="legacyTaskInteraction = null" />
<NeoUiSetupWizard v-if="startupState?.needsSetup && !editorInstanceId && !resourceDownloadInstance" :status="startupState" :execute="call" @changed="refresh" @complete="finishStartupSetup" />
<NeoUiOverlays v-if="!isEndfieldShell" :add-dialog="addDialog" :busy="busy" :execute="catalogExecute" :call="call" :is-pcl-shell="isPclShell" :status="status" :task-snapshot="taskSnapshot" :task-center-open="taskCenterOpen" :search-shown="searchShown" :global-search="globalSearch" :search-results="searchResults" :icons="icons" :dialog="dialog" :prompt="prompt" :answer="answer" :choices="choices" :confirmation="confirmation" :secret="secret" @update:add-dialog="addDialog=$event" @created="refresh" @cancel="cancel" @task-updated="handleTaskUpdate" @update:task-center-open="taskCenterOpen=$event" @update:search-shown="searchShown=$event" @update:global-search="globalSearch=$event" @search-result="openSearchResult" @dialog-finish="finishDialog" @update:answer="answer=$event" />
</div>
</template>
<script setup lang="ts">
import { nextTick, onMounted, onUnmounted, ref, computed, watch } from 'vue';
import { storeToRefs } from 'pinia';
import { useI18n } from 'vue-i18n';
import NeoCreateInstancePage, { type NeoCreationDraft } from './components/NeoCreateInstancePage.vue';
import NeoPclPageHost from './components/NeoPclPageHost.vue';
import NeoDeviceLoginDialog from './components/NeoDeviceLoginDialog.vue';
import NeoXmclPageHost from './components/NeoXmclPageHost.vue';
import PclInstallProgressDialog from './themes/pcl/pages/download/PclInstallProgressDialog.vue';
import PclTaskCenter from './themes/pcl/components/PclTaskCenter.vue';
import PclInstanceEditWindow from './themes/pcl/components/PclInstanceEditWindow.vue';
import PclStandaloneInstanceEditor from './themes/pcl/pages/settings/PclStandaloneInstanceEditor.vue';
import PclResourceDownloadWindow from './themes/pcl/pages/download/PclResourceDownloadWindow.vue';
import PclToastHost from './themes/pcl/components/PclToastHost.vue';
import { showPclToast } from './themes/pcl/composables/usePclToast';
import { initPclMusic } from './themes/pcl/composables/usePclMusic';
import NeoUiChrome from './themes/xmcl-port/components/NeoUiChrome.vue';
import EndfieldShell from './themes/endfield/pages/EndfieldShell.vue';
import EndfieldStandaloneEditor from './themes/endfield/pages/EndfieldStandaloneEditor.vue';
import EndfieldResourceDownloadWindow from './themes/endfield/pages/EndfieldResourceDownloadWindow.vue';
import TaskInteractionDialog from './components/TaskInteractionDialog.vue';
import NeoUiSetupWizard from './components/NeoUiSetupWizard.vue';
import NeoUiOverlays from './themes/xmcl-port/components/NeoUiOverlays.vue';
import { useQuasar } from 'quasar';
import { launcher } from './boot/launcher-api';
import { neoUiPreferences, persistNeoUiPreferences } from './neo-ui-preferences';
import { useNeoDialogs } from './composables/useNeoDialogs';
import { flattenTaskItems, taskActiveItems, taskProgressStats, taskRowsEqual } from './themes/pcl/composables/useTaskProgress';
import { useLauncherSessionStore } from './common/launcher-session';
import { recordRuntimeDebug } from './common/runtime/runtime-debug';
import { useThemeRuntime } from './common/runtime/theme-runtime';
const { t } = useI18n();
const buildLabel = '2026-09-20.1';
const launchQuery = new URLSearchParams(window.location.search);
const editorInstanceId = launchQuery.get('instanceEditor') || '';
const resourceDownloadInstance = launchQuery.get('resourceDownload') || '';
const resourceDownloadKind = launchQuery.get('kind') || 'mods';
const resourceDownloadName = launchQuery.get('name') || '';
const isPclShell = computed(() => neoUiPreferences.shell === 'pcl');
const isEndfieldShell = computed(() => neoUiPreferences.shell === 'endfield');
useThemeRuntime('pcl', isPclShell);
useThemeRuntime('xmcl', () => !isPclShell.value && !isEndfieldShell.value);
const downloadComponentUid = ref('');
const quasar=useQuasar(),connected=ref(false);
const launcherSession = useLauncherSessionStore();
const { instances, accounts, selected, icons, selectedAccountId } = storeToRefs(launcherSession);
const startupState=ref<any>(null);
function closeMobileNav(){if(quasar.screen.lt.md)drawerOpen.value=false;}
const components=ref<any[]>([]),componentError=ref('');
function sameSnapshot(left: unknown, right: unknown) { try { return JSON.stringify(left) === JSON.stringify(right); } catch { return Object.is(left, right); } }
function replaceArrayIfChanged<T>(target: { value: T[] }, next: T[]) {
  if (sameSnapshot(target.value, next)) return false;
  target.value.splice(0, target.value.length, ...next);
  return true;
}
async function loadComponents(refreshToken = refreshGeneration){componentError.value='';try{const r=await call('instance.components.list',{instance:selected.value.id});if(refreshToken===refreshGeneration){const next=Array.isArray(r?.components)?r.components:[];if(replaceArrayIfChanged(components,next))recordRuntimeDebug('store','app.components','component list patched',{count:next.length});}}catch(e){if(refreshToken===refreshGeneration)componentError.value=message(e);}}
async function changeComponentVersion(c:any){const v=await ask(t('app.changeVersion', { name: c.name || c.id }),c.version);if(v!==null && v.trim())await run('instance.component.set-version',{instance:selected.value.id,component:c.id,version:v.trim()});}
const editWindows=ref<any[]>([]), instanceDetails=ref<any>(null),resourcesLoading=ref(false),resourceError=ref(''),taskSnapshot=ref<any>(null),taskCenterOpen=ref(false),searchShown=ref(false),globalSearch=ref('');
const installProgressOpen=ref(false),installProgressTitle=ref(t('app.installingPack')),installTask=ref<any>(null),installCancelling=ref(false);
const logMode=ref('live'),logFiles=ref<any[]>([]),logFile=ref(''),fileLog=ref('');
const consoleLines=ref<string[]>([]),consoleState=ref(t('app.disconnected')),followLog=ref(true),consoleElement=ref<HTMLElement|null>(null);
const launchQueue=ref<any[]>([])
const launchTaskId=ref('');
type LaunchUiState = 'loading' | 'disabled' | 'download' | 'launch' | 'launching' | 'launched' | 'failed' | 'cancelled';
const launchUiState=ref<LaunchUiState>('loading'), launchProgress=ref(0), launchTargetProgress=ref(0), launchStage=ref(t('app.initializing')), launchLoginMethod=ref(''), launchSpeed=ref(0), launchError=ref(''), launchDetails=ref(''), launchChildren=ref<any[]>([]);
// The selected account is runtime state shared by every shell. Keep the
// launch page bound to Pinia so changing a theme never resets the selection.
const launchAccountId = selectedAccountId;
function selectLaunchAccount(id:string){launcherSession.selectAccount(id);}
let launchUiResetTimer: ReturnType<typeof setTimeout> | undefined;
const detailCache=new Map<string,any>();
let launchingId='',preparingLaunch=false;
const sessions=new Map<string,{subscription:string;lines:string[];state:string}>();
let consoleSubscription='',consoleInstance='',launchInFlight=false;
const consoleMonitorId = ref('');
const consoleMonitorOptions = computed(() => instances.value.filter((item: any) => Boolean(item?.running) || String(item?.id) === consoleMonitorId.value));
function clearConsole(){
 consoleLines.value=[];
 if(consoleInstance){const saved=sessions.get(consoleInstance);if(saved)saved.lines=[];}
}
async function appendConsole(text:string,instanceId=selected.value?.id){
 if(instanceId!==consoleInstance){const saved=sessions.get(instanceId);if(saved){saved.lines.push(text);saved.lines.splice(0,Math.max(0,saved.lines.length-3000));}return;}
 consoleLines.value.push(text);
 if(consoleLines.value.length>3000)consoleLines.value.splice(0,consoleLines.value.length-3000);
 await nextTick();if(followLog.value && consoleElement.value)consoleElement.value.scrollTop=consoleElement.value.scrollHeight;
}
function receiveStream(batch:any){
 const entry=[...sessions.entries()].find(([,v])=>v.subscription===batch.subscriptionId);
 if(!entry)return;
 const instanceId=entry[0];
  const updateRunning=(running:boolean)=>{
   // Keep the reactive objects stable. Replacing the whole array and selected
   // object makes the PCL layout re-evaluate when only `running` changed.
   const item=instances.value.find((entry:any)=>entry.id===instanceId);
   if(item && item.running!==running)launcherSession.updateInstance(instanceId, { running });
  };
 if(entry[0]!==consoleInstance){for(const event of batch.events||[]){if(event.kind==='console.line')entry[1].lines.push(event.text);if(event.kind==='console.data')entry[1].lines.push(new TextDecoder().decode(Uint8Array.from(atob(event.data),c=>c.charCodeAt(0))));if(event.kind==='instance.state'){entry[1].state=event.running?t('app.clientRunning'):t('app.clientStopped');updateRunning(Boolean(event.running));}}entry[1].lines.splice(0,Math.max(0,entry[1].lines.length-3000));return;}
 if(batch.dropped)void appendConsole(t('app.droppedLogs', { count: batch.dropped }));
 for(const event of batch.events || []){
  if(event.kind==='console.line')void appendConsole(event.text);
  else if(event.kind==='console.data')void appendConsole(new TextDecoder().decode(Uint8Array.from(atob(event.data),c=>c.charCodeAt(0))));
  else if(event.kind==='instance.state'){
   consoleState.value=event.running?t('app.clientRunning'):t('app.clientStopped');
   updateRunning(Boolean(event.running));
   void appendConsole(`[${consoleState.value}]`);
  }else if(event.kind==='console.reset')consoleState.value=event.available?t('app.consoleConnected'):t('app.waitingLaunch');
 }
}
async function connectConsole(i:any){
 if(consoleSubscription && consoleInstance===i.id)return;
 try{
  const existing=sessions.get(i.id);if(existing?.subscription){consoleSubscription=existing.subscription;consoleInstance=i.id;consoleMonitorId.value=i.id;consoleLines.value=existing.lines;consoleState.value=existing.state;return;}
  consoleSubscription='';consoleInstance=i.id;consoleMonitorId.value=i.id;consoleLines.value=[];
  const sub=await call('instance.console.subscribe',{instance:i.id});consoleSubscription=sub.subscriptionId;sessions.set(i.id,{subscription:consoleSubscription,lines:consoleLines.value,state:t('app.consoleConnected')});
  consoleState.value=t('app.consoleConnected');
 }catch(e){error.value=message(e);consoleState.value=t('app.connectionFailed');}
}
async function selectConsoleInstance(id: string) {
 const target = instances.value.find((item: any) => String(item?.id) === String(id));
 if (target) await connectConsole(target);
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
function resetLaunchUiSoon(){if(launchUiResetTimer)clearTimeout(launchUiResetTimer);launchUiResetTimer=setTimeout(()=>{if(launchUiState.value==='launched' || launchUiState.value==='cancelled'){setLaunchState(selected.value ? 'launch' : (instances.value.length ? 'disabled' : 'download'));launchError.value='';}},2200);}
function updateLaunchProgress(value:any){
 if(!value)return;
 if(launchTaskId.value && value.taskId !== launchTaskId.value)return;
 if(!launchInFlight && !preparingLaunch && !['launching','launched'].includes(launchUiState.value))return;
 const rows=flattenTaskItems(value);
 const stats=taskProgressStats(value, rows);
  if(stats.percent !== null) launchTargetProgress.value=Math.max(0,Math.min(1,stats.percent/100));
  else if(value.state==='succeeded') launchTargetProgress.value=1;
 const activeStep=taskActiveItems(rows, 1)[0];
 launchStage.value=activeStep?.label || activeStep?.status || value.details || value.status || launchStage.value || t('app.executing');
 launchDetails.value=[value.details, stats.itemCount ? stats.summary : '', stats.byteSummary].filter(Boolean).join(' · ');
 if(!taskRowsEqual(launchChildren.value, rows))launchChildren.value=rows;
 launchSpeed.value=Number(value.transferRate || 0);
 if(value.failReason || String(value.state || value.status || '').toLowerCase()==='failed'){launchError.value=value.failReason || value.details || t('app.launchTaskFailed');setLaunchState('failed');}
 else if(!isTaskFinished(value) && (launchInFlight || preparingLaunch))setLaunchState('launching');
}
async function launch(i:any, accountId?:string, serverAddress?:string, restoreSelectionId?:string){
 if(busy.value){if(i.id!==launchingId&&!launchQueue.value.some(x=>x.id===i.id))launchQueue.value.push(i);return;}
 if(launchUiResetTimer)clearTimeout(launchUiResetTimer);
  launchProgress.value=0;launchTargetProgress.value=0;launchStage.value=t('app.initializing');launchSpeed.value=0;launchError.value='';launchDetails.value='';launchChildren.value=[];setLaunchState('launching');
 busy.value=true;launchingId=i.id;preparingLaunch=true;
 await openInstance(i,true,true);if(neoUiPreferences.pclClearLaunchLogOnStart)clearConsole();if(isPclShell.value)page.value='instances';logMode.value='live';await connectConsole(i);preparingLaunch=false;
 if(!consoleSubscription){busy.value=false;launchingId='';setLaunchState('failed');launchError.value=t('app.consoleUnavailable');resetLaunchUiSoon();return;}
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
  if(!task || task.state!=='succeeded')throw new Error(task?.error || t('app.launchFailed'));
  launchTargetProgress.value=1;launchProgress.value=1;launchStage.value=t('app.waitingGameWindow');setLaunchState('launched');
  notice.value=t('app.launchReturned', { pid: task.pid });resetLaunchUiSoon();
 }catch(e){error.value=message(e);if(launchUiState.value!=='cancelled'){launchError.value=error.value;setLaunchState('failed');}void appendConsole(`[${t('app.launchFailed')}] `+error.value,launchingId);resetLaunchUiSoon();}
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
async function readLogFile(){try{const r=await call('instance.log.read',{instance:selected.value.id,file:logFile.value,maxBytes:1048576});fileLog.value=r.content+(r.truncated?'\n'+t('app.logTruncated'):'');}catch(e){error.value=message(e);}}
const resources=ref<any[]>([]),resourceKind=ref('mods'),resourceSource=ref('');
const resourceKinds=[{label:t('resource.kinds.mods'),value:'mods'},{label:t('resource.kinds.resourcepacks'),value:'resourcepacks'},{label:t('resource.kinds.shaderpacks'),value:'shaderpacks'},{label:t('resource.kinds.texturepacks'),value:'texturepacks'},{label:t('resource.kinds.datapacks'),value:'datapacks'},{label:t('resource.kinds.schematics'),value:'schematics'},{label:'Yes Steve Model',value:'yesstevemodels'},{label:'Customizable Player Models',value:'customplayermodels'}];
const pages=[{id:'instances',label:t('app.instances'),icon:'apps'},{id:'create',label:t('app.addInstance'),icon:'add_box'},{id:'accounts',label:t('nav.accounts'),icon:'person'},{id:'settings',label:t('nav.settings'),icon:'settings'}];
const drawerOpen=ref(true), addDialog=ref(false), page=ref('instances'), busy=ref(false), error=ref(''), notice=ref(''), status=ref(''), search=ref(''), username=ref(''), notes=ref('');
const pclRouteAnimation=ref('');
let pclRouteAnimationTimer: ReturnType<typeof setTimeout> | undefined;
const pclRouteOrder=['instances','pcl-download','pcl-settings','pcl-tools','accounts','manage'];
watch(page,(next,previous)=>{
 if(next===previous || !isPclShell.value || !neoUiPreferences.animations)return;
 const nextIndex=pclRouteOrder.indexOf(next),previousIndex=pclRouteOrder.indexOf(previous);
 const direction=nextIndex>=0 && previousIndex>=0 && nextIndex<previousIndex?'backward':'forward';
 pclRouteAnimation.value=`pcl-route-slide-${direction}`;
 if(pclRouteAnimationTimer)clearTimeout(pclRouteAnimationTimer);
 pclRouteAnimationTimer=setTimeout(()=>{pclRouteAnimation.value='';pclRouteAnimationTimer=undefined;},420);
});
let lastErrorNotification='';
watch(error,(value)=>{if(value && value!==lastErrorNotification){lastErrorNotification=value;if(isPclShell.value){showPclToast({type:'error',title:t('app.operationFailed'),message:value,timeout:6000});}else{quasar.notify({type:'negative',message:value,timeout:6000,position:'top',progress:true,actions:[{icon:'close',color:'white'}]});}}});
watch(notice,(value)=>{if(!value)return;if(isPclShell.value){showPclToast({type:'success',title:t('app.operationDone'),message:value,timeout:3500});}else{quasar.notify({type:'positive',message:value,timeout:3500,position:'top',progress:true});}});
watch(busy,async value=>{if(!value && launchQueue.value.length){await nextTick();if(!busy.value){const next=launchQueue.value.shift();if(next)void launch(next);}}});
watch(isPclShell,(value)=>{scope.value='launcher';if(value && page.value==='settings')page.value='pcl-settings';else if(!value && page.value==='pcl-settings')page.value='settings';void refresh();});
const settings=ref<any[]>([]),device=ref<any>(null),deviceShown=ref(false);
function rememberedInstanceId(){
 return String(launcherSession.selectedInstanceId || neoUiPreferences.selectedInstanceId || '').trim();
}
function rememberInstance(instance:any){
 if(!instance?.id)return;
 launcherSession.selectInstance(instance);
}
function clearRememberedInstance(){
 launcherSession.selectInstance(null);
}
const searchResults=computed(()=>{const q=globalSearch.value.trim().toLowerCase();if(!q)return instances.value.slice(0,12);return instances.value.filter((i:any)=>`${i.name} ${i.id} ${i.group||''} ${i.type||''} ${i.runtime?.minecraft||i.minecraftVersion||''}`.toLowerCase().includes(q)).slice(0,20);});
const scope=ref('launcher'),settingSearch=ref(''),createMode=ref('vanilla');
const creation=ref<NeoCreationDraft>({name:'',group:'',version:'',loader:'',loaderVersion:'',source:''});
const actions=[{op:'rename',field:'name',label:t('xmcl.instance.rename')},{op:'copy',field:'name',label:t('xmcl.instance.copy')},{op:'group',field:'group',label:t('xmcl.instance.group')},{op:'update',label:t('xmcl.instance.update')},{op:'verify',label:t('xmcl.instance.verify')},{op:'stop',label:t('xmcl.instance.stop')}];
const { dialog, prompt, answer, choices, secret, confirmation, ask, finishDialog, message } = useNeoDialogs();
const legacyTaskInteraction = ref<any>(null);
const readOnlyOperations=new Set([
 'task.status','task.list','task.cancel','neo.task.status','neo.task.list','neo.task.cancel','api.describe','scheduler.status','aria2.downloads','aria2.cancel','aria2.clear-finished','instance.open-folder','instance.folder-path',
  'settings.list','resource.icon','runtime.info','runtime.memory','instance.icon-library','account.preset.list','authentication.helper.status',
   'language.list','launcher.update.status','component.catalog','component.versions','special.loader.versions'
]);
async function catalogExecute(op:string,p:Record<string,unknown>){
 if(busy.value && !readOnlyOperations.has(op)){throw new Error(t('app.taskBusy'));}
 if(readOnlyOperations.has(op))return call(op,p);
 busy.value=true;try{return await call(op,p);}finally{busy.value=false;status.value='';taskSnapshot.value=null;taskCenterOpen.value=false;if(op==='account.login'){deviceShown.value=false;device.value=null;}}
}
async function call(op:string,p:Record<string,unknown>={}){const r=await launcher.execute<any>(op,p);if(!r.ok)throw new Error(r.error||t('app.operationFailed'));connected.value=true;return r.data;}
async function finishStartupSetup(){
 try{startupState.value=await call('startup.status');await refresh();}
 catch(e){error.value=message(e);}
}
function settingScope(){return scope.value==='instance' ? {scope:'instance',instance:selected.value.id} : {scope:'launcher'};}
let resourceRequest=0;
let refreshGeneration=0;
async function loadResources(refreshToken = refreshGeneration){const request=++resourceRequest;const id=selected.value.id,kind=resourceKind.value;resourceError.value='';resourcesLoading.value=true;try{const data=await call('resource.list',{instance:id,kind,includeIcons:false});if(request===resourceRequest&&refreshToken===refreshGeneration){if(!Array.isArray(data))throw new Error(t('app.resourceFormatError'));if(replaceArrayIfChanged(resources,data))recordRuntimeDebug('store','app.resources','resource list patched',{instance:id,kind,count:data.length});}}catch(e){if(request===resourceRequest&&refreshToken===refreshGeneration)resourceError.value=message(e);}finally{if(request===resourceRequest&&refreshToken===refreshGeneration)resourcesLoading.value=false;}}
async function loadLogFiles(refreshToken = refreshGeneration){try{const files=await call('instance.log.list',{instance:selected.value.id});if(refreshToken===refreshGeneration&&replaceArrayIfChanged(logFiles,Array.isArray(files)?files:[]))recordRuntimeDebug('store','app.logs','log file list patched',{count:files?.length||0});}catch(e){if(refreshToken===refreshGeneration)error.value=message(e);}}
async function refresh(){const refreshToken=++refreshGeneration;recordRuntimeDebug('refresh','App','refresh requested',{page:page.value,token:refreshToken});error.value='';try{if(page.value==='accounts'){const value=await call('account.list');if(refreshToken===refreshGeneration)launcherSession.replaceAccounts(value);}else if(page.value==='settings' || page.value==='pcl-settings'){const [value,accountValue]=await Promise.all([call('settings.list',settingScope()),call('account.list')]);if(refreshToken===refreshGeneration){if(Array.isArray(value)&&!sameSnapshot(settings.value,value))replaceArrayIfChanged(settings,value);if(Array.isArray(accountValue))launcherSession.replaceAccounts(accountValue);}}else if(page.value==='manage'){await loadResources(refreshToken);await loadComponents(refreshToken);await loadLogFiles(refreshToken);if(selected.value){const value=await call('settings.list',{scope:'instance',instance:selected.value.id});if(refreshToken===refreshGeneration&&Array.isArray(value)&&!sameSnapshot(settings.value,value))replaceArrayIfChanged(settings,value);}}else {const value=await call('instance.list');if(refreshToken!==refreshGeneration)return;launcherSession.replaceInstances(value);await launcherSession.refreshIcons();if(refreshToken!==refreshGeneration)return;const accountValue=await call('account.list');if(refreshToken!==refreshGeneration)return;launcherSession.replaceAccounts(accountValue);if(isPclShell.value){const remembered=rememberedInstanceId();const rememberedInstance=instances.value.find((item:any)=>String(item.id)===remembered);if(!selected.value)launcherSession.selectInstance(rememberedInstance || instances.value[0] || null);if(selected.value)rememberInstance(selected.value);else clearRememberedInstance();}if(!launchInFlight && !['launched','failed','cancelled'].includes(launchUiState.value))setLaunchState(instances.value.length ? (selected.value ? 'launch' : 'disabled') : 'download');}recordRuntimeDebug('refresh','App','refresh completed',{page:page.value,token:refreshToken});}catch(e){if(refreshToken===refreshGeneration){error.value=message(e);recordRuntimeDebug('error','App','refresh failed',message(e));}}}
async function navigate(id:string){
 if(preparingLaunch)return;
 if(id==='pcl-download')downloadComponentUid.value='';
 const target=!isPclShell.value && id==='pcl-download'?'store':!isPclShell.value && id==='pcl-tools'?'accounts':!isPclShell.value && id==='pcl-settings'?'settings':id;
 const persistentPclTabs = new Set(['instances','pcl-download','pcl-settings','pcl-tools']);
 const keepPclTabsAlive = isPclShell.value && persistentPclTabs.has(target);
 page.value=target;
 error.value='';
 notice.value='';
 if(target==='settings' || target==='pcl-settings')scope.value='launcher';
 // A background download/install must not lock the whole UI. Render the
 // target page immediately; its own refresh will queue behind the task and
 // fill in when the backend becomes free.
 if(keepPclTabsAlive)return;
 if(busy.value)return;
 await refresh();
}
function openComponentDownload(uid:string){
 downloadComponentUid.value=String(uid || '');
 page.value='pcl-download';
 error.value='';
 notice.value='';
 if(!busy.value)void refresh();
}
function openPclCreate(uid?:string,version?:string){createMode.value='vanilla';creation.value={...creation.value,name:creation.value.name || t('app.newInstance'),version:uid==='net.minecraft'?(version || ''):creation.value.version,loader:uid==='net.fabricmc.fabric-loader'?'fabric':uid==='net.minecraftforge'?'forge':uid==='net.neoforged'?'neoforge':uid==='org.quiltmc.quilt-loader'?'quilt':creation.value.loader,loaderVersion:uid && uid!=='net.minecraft'?(version || ''):creation.value.loaderVersion};page.value='create';}
function openGlobalSearch(){globalSearch.value='';searchShown.value=true;}
function openSearchResult(instance:any){searchShown.value=false;void openInstance(instance);}
function xmclTab(tab:string){if(busy.value)return;if(tab==='home'){page.value='manage';return;}if(tab==='mods'||tab==='resourcepacks'||tab==='shaderpacks'){resourceKind.value=tab;page.value='manage';void loadResources();return;}if(tab==='save'){page.value='manage';return;}if(tab==='settings'){scope.value='instance';page.value='settings';void refresh();}}
async function run(op:string,p:Record<string,unknown>={}){if(busy.value)return;busy.value=true;error.value='';notice.value='';device.value=null;try{await call(op,p);notice.value=t('app.operationDone');await refresh();}catch(e){error.value=message(e);}finally{busy.value=false;status.value='';device.value=null;taskSnapshot.value=null;taskCenterOpen.value=false;}}
async function create(value: NeoCreationDraft & { type?: string }){const p:any={...value};for(const k of Object.keys(p))if(!p[k])delete p[k];await run(value.type==='import'?'instance.import':'instance.create',p);if(!error.value){page.value='instances';await refresh();}}
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
async function openInstance(i:any,forceRead=false,forLaunch=false){
 if(preparingLaunch&&!forceRead)return;
 if(busy.value&&!forceRead)return;
 if(selected.value)detailCache.set(selected.value.id,{info:instanceDetails.value,notes:notes.value,components:components.value,resources:resources.value,kind:resourceKind.value,logFiles:logFiles.value});
 if(selected.value?.id!==i.id){
  const old=sessions.get(selected.value?.id);if(old){old.lines=consoleLines.value;old.state=consoleState.value;}
  const saved=sessions.get(i.id);consoleSubscription=saved?.subscription||'';consoleInstance=i.id;consoleMonitorId.value=i.id;consoleLines.value=saved?.lines||[];consoleState.value=saved?.state||t('app.disconnected');logFile.value='';fileLog.value='';logFiles.value=[];
 }
selected.value=i;rememberInstance(i);if(!forLaunch){page.value='manage';if(!launchInFlight && !['launched','failed','cancelled'].includes(launchUiState.value))setLaunchState('launch');}notes.value='';error.value='';instanceDetails.value=null;
 try{const info=await call('instance.info',{instance:i.id});instanceDetails.value=info;notes.value=info.notes || '';}catch(e){error.value=message(e);}
 try{settings.value=await call('settings.list',{scope:'instance',instance:i.id});}catch{/* Older backends may not expose instance settings yet. */}
 if(!forLaunch){await loadResources();await loadComponents();await loadLogFiles();void connectConsole(i);}
}
async function changeIcon(iconKey?: string){
 if(!selected.value)return;
 if(iconKey?.trim()){
  await run('instance.set-icon',{instance:selected.value.id,icon:iconKey.trim()});
 }else{
  const path=await ask(t('app.iconPath'));if(path===null||!path.trim())return;
  await run('instance.set-icon',{instance:selected.value.id,path:path.trim()});
 }
 if(!error.value){const icon=await call('instance.icon',{instance:selected.value.id});if(icon?.dataUrl)icons.value[selected.value.id]=icon.dataUrl;}
}
async function instanceAction(op:string,field:string|undefined,label:string){const p:any={instance:selected.value.id};if(field){const v=await ask(label,field==='group'?(selected.value.group||''):selected.value.name);if(v===null)return;p[field]=v;}await run('instance.'+op,p);}
async function workspaceAction(op:string){const labels:Record<string,string>={rename:t('xmcl.instance.rename'),copy:t('xmcl.instance.copy'),group:t('xmcl.instance.group')};if(labels[op]){await instanceAction(op,op==='group'?'group':'name',labels[op]);return;}await instanceAction(op,undefined,op);}
async function instanceSelectionAction(instance:any,op:string){
 if(!instance?.id)return;
 if(op==='delete'){if(await ask(t('app.confirmRemove', { name: instance.name }),'',true))await run('instance.delete',{instance:instance.id,confirm:true});return;}
 const labels:Record<string,string>={rename:t('xmcl.instance.rename'),group:t('app.moveGroup'),copy:t('xmcl.instance.copy')};
 if(labels[op]){const field=op==='group'?'group':'name';const value=await ask(labels[op],field==='group'?(instance.group||''):(instance.name||''));if(value===null)return;await run(`instance.${op}`,{instance:instance.id,[field]:value});return;}
 if(op==='verify'){await run('instance.verify',{instance:instance.id});return;}
 if(op==='stop'){await run('instance.stop',{instance:instance.id});return;}
 if(op==='shortcut'){await run('instance.create-shortcut',{instance:instance.id,target:'desktop'});return;}
 if(op==='export'){await openInstance(instance);notice.value=t('app.exportHint');return;}
}
async function askDelete(op:string,p:any,name:string){if(await ask(t('app.confirmRemove', { name }),'',true))await run(op,{...p,confirm:true});}
async function editSetting(s:any){const value=await ask(t('app.editSetting', { key: s.key }),JSON.stringify(s.value));if(value===null)return;try{await run('settings.set',{...settingScope(),key:s.key,value:JSON.parse(value)});}catch(e){error.value=t('app.invalidJson');}}
function isTaskFinished(value:any){return Boolean(value?.finished) || value?.running === false || ['succeeded','failed','aborted','finished','completed','cancelled','canceled'].includes(String(value?.state || value?.status || '').toLowerCase());}
function handleTaskUpdate(value:any){
 if(!value)return;
 if(launchTaskId.value && value.taskId!==launchTaskId.value)return;
 if(installProgressOpen.value && installTask.value?.taskId && value.taskId!==installTask.value.taskId)return;
 if(installProgressOpen.value && (!installTask.value?.taskId || installTask.value.taskId===value.taskId))installTask.value=value;
 updateLaunchProgress(value);
 if(isTaskFinished(value)){if(launchInFlight && (value.failReason || value.successful===false)){launchError.value=value.failReason || value.details || t('app.launchTaskFailed');setLaunchState('failed');}taskSnapshot.value=null;taskCenterOpen.value=false;status.value='';}
 else {taskSnapshot.value=value;status.value=value.status || value.state || t('app.executing');}
}
async function installComponentVersion(component:any,version:string){
 if(!selected.value)return;
 beginInstallProgress({ title: t('app.installingComponent', { name: component?.name || component?.id || t('app.component') }), name: version });
 try {
  let task:any=await catalogExecute('neo.task.start',{operation:'instance.component.set-version',priority:'high',parameters:{instance:selected.value.id,component:component.id,version,important:component.id==='net.minecraft'}});
  while(task && !task.finished){
   installTask.value=task;
   await new Promise(resolve=>setTimeout(resolve,300));
   task=await catalogExecute('neo.task.status',{taskId:task.taskId});
  }
  installTask.value=task;
  if(!task || task.state!=='succeeded')throw new Error(task?.error || t('app.componentInstallFailed'));
  await refresh();
 } catch(exception){error.value=message(exception);}
 finally { endInstallProgress(); }
}
function beginInstallProgress(info:any){installProgressTitle.value=info?.title || t('app.installingPack');installTask.value=null;installCancelling.value=false;installProgressOpen.value=true;}
function endInstallProgress(info?:any){
 const task=installTask.value,title=installProgressTitle.value;
 const state=String(task?.state||'').toLowerCase();
 const failed=task?.successful===false || ['failed','error'].includes(state) || Boolean(task?.failReason || task?.error || info?.error);
 if(failed){
  // Keep the terminal task in the dialog so the user can read the backend
  // failure instead of losing the only useful diagnostic when the child
  // installer emits its final event.
  if(info?.error){
   installTask.value={ ...(task || {}), operation: info.operation || task?.operation, name: info.name || task?.name, state: 'failed', successful: false, status: t('tasks.states.failed'), error: info.error, details: info.error };
  }
  installCancelling.value=false;
  return;
 }
 if(state==='succeeded' && isPclShell.value){
  const completed=title;
  showPclToast({type:'success',title:t('app.operationDone'),message:t('app.completed', { name: completed }),timeout:3500});
 }
 installProgressOpen.value=false;installCancelling.value=false;installTask.value=null;
}
function closeInstallProgress(){
 installProgressOpen.value=false;installCancelling.value=false;installTask.value=null;
}

async function exportInstance(payload:any){
  if(!selected.value)return;
  beginInstallProgress({title:t('app.exportingInstance'),name:selected.value.name});
  try{
    let task:any=await call('neo.task.start',{operation:'instance.export',priority:'high',parameters:{instance:selected.value.id,output:payload.output,format:payload.format||'zip',name:payload.name,version:payload.version,summary:payload.summary,author:payload.author,optionalFiles:Boolean(payload.optionalFiles),overwrite:payload.overwrite!==false}});
    while(task && !task.finished){installTask.value=task;await new Promise(resolve=>setTimeout(resolve,400));task=await call('neo.task.status',{taskId:task.taskId});}
    installTask.value=task;
    if(!task || task.state!=='succeeded')throw new Error(task?.error||t('app.exportFailed'));
    notice.value=t('app.exportedTo') + ' '+(payload.output||'');
  }catch(e){error.value=message(e);}
  finally{endInstallProgress();}
}
async function cancelInstallProgress(){installCancelling.value=true;try{if(installTask.value?.taskId)await launcher.execute('neo.task.cancel',{taskId:installTask.value.taskId});else await launcher.execute('task.cancel',{});}catch{/* the task may already be terminal */}}
async function cancel(){try{if(launchTaskId.value)await call('neo.task.cancel',{taskId:launchTaskId.value});else await call('task.cancel');if(launchInFlight || launchUiState.value==='launching'){setLaunchState('cancelled');launchError.value=t('app.launchCancelled');resetLaunchUiSoon();}}catch(e){error.value=message(e);}}
async function openFolder(){
 if(!selected.value)return;
 try{
  // Always resolve the current instance. Cached details can belong to the
  // previously selected instance when the user switches tabs quickly.
  const info=await call('instance.get',{instance:selected.value.id});
  const path=info?.paths?.root;
  if(!path)throw new Error(t('app.instanceDirUnread'));
  await launcher.openPath(path);
  if(isPclShell.value) showPclToast({type:'success',title:t('app.folderOpened'),message:path,timeout:2500});
  else quasar.notify({type:'positive',message:t('app.folderOpenedPath', { path }),timeout:2500,position:'top',progress:true});
 }catch(e){error.value=message(e);}
}
async function copyText(value:string){try{await navigator.clipboard?.writeText(value);notice.value=t('app.copiedClipboard');}catch{/* Clipboard may be unavailable in the web preview. */}}
async function cancelDeviceLogin(){
 deviceShown.value=false;device.value=null;
 try{await launcher.execute('task.cancel',{});}catch{/* the login may already have finished */}
}
async function openDeviceBrowser(){const url=String(device.value?.url || '');if(!url)return;try{await call('desktop.open-url',{url});}catch(e){error.value=message(e);}}
async function copyDeviceCode(){const code=String(device.value?.code || '');if(code)await copyText(code);}
let unlisten:(()=>void)|undefined,unlistenStream:(()=>void)|undefined,unlistenExit:(()=>void)|undefined;
 onMounted(async()=>{if(editorInstanceId || resourceDownloadInstance)return;void initPclMusic();try{unlistenStream=await launcher.onStream(receiveStream);unlistenExit=await launcher.onExit((reason)=>{connected.value=false;sessions.clear();consoleSubscription='';consoleMonitorId.value='';consoleState.value=t('app.backendExited');for(const item of instances.value)item.running=false;if(selected.value)selected.value.running=false;error.value=reason.message || t('app.backendDisconnected');});unlisten=await launcher.onEvent(async(raw:any)=>{recordRuntimeDebug('event','launcher-event','event received',{kind:raw?.kind,operation:raw?.operation});if(raw.kind==='input'){if(raw.context?.type==='restricted-downloads'){legacyTaskInteraction.value=raw;return;}const opts=raw.choices?.map((x:any,n:number)=>({label:typeof x==='string'?x:(x.name||x.label||JSON.stringify(x)),value:n}));const v=await ask(raw.prompt,'',false,opts||null,raw.secret);try{await launcher.respond(v===null?{interactionId:raw.interactionId,cancel:true}:{interactionId:raw.interactionId,value:v});}catch(e){error.value=message(e);}}else if(raw.kind==='device_code'){device.value=raw.data || raw;deviceShown.value=true;}else if(raw.kind==='status'){const messageText=raw.message || raw.data?.message || (typeof raw.data==='string'?raw.data:t('app.executing'));if(launchInFlight){launchStage.value=messageText;setLaunchState('launching');void appendConsole(`[${t('app.launchPrefix')}] ${messageText}`,launchingId);}status.value=messageText;}else if(raw.kind==='task'){handleTaskUpdate(raw.task || raw.data || null);}});await refresh();startupState.value=await call('startup.status');if(startupState.value?.needsSetup)settings.value=await call('settings.list',{scope:'launcher'});}catch(e){error.value=message(e);}});
 onUnmounted(()=>{stopLaunchProgressAnimation();if(pclRouteAnimationTimer)clearTimeout(pclRouteAnimationTimer);unlisten?.();unlistenStream?.();unlistenExit?.();});
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

@keyframes pcl-route-slide-forward {
  from { opacity: .72; transform: translateX(34px); }
  to { opacity: 1; transform: translateX(0); }
}

@keyframes pcl-route-slide-backward {
  from { opacity: .72; transform: translateX(-34px); }
  to { opacity: 1; transform: translateX(0); }
}

.pcl-route-slide-forward { animation: pcl-route-slide-forward .34s cubic-bezier(.16, 1, .3, 1); }
.pcl-route-slide-backward { animation: pcl-route-slide-backward .34s cubic-bezier(.16, 1, .3, 1); }
</style>
