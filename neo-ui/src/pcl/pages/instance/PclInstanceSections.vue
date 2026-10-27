<template>
  <div class="pcl-instance-sections">
    <section v-if="section === 'install'" class="pcl-section-card pcl-install-section">
      <div class="pcl-install-head">
        <div>
          <span class="pcl-kicker">{{ t('ui.instanceInstallation') }}</span>
          <h2>{{ t('instanceSections.install') }}</h2>
          <p>{{ t('instanceSections.installHint') }}</p>
        </div>
        <PclButton size="sm" icon="refresh-cw" :disabled="busy" @click="emit('refresh')">{{ t('common.refresh') }}</PclButton>
      </div>
      <PclHint v-if="componentError" theme="red" :closable="false">{{ t('instanceSections.componentError', { error: componentError }) }}</PclHint>
      <div class="pcl-install-list">
        <article v-for="item in installItems" :key="item.id" class="pcl-install-item" :class="{ expanded: expanded === item.id }">
          <button class="pcl-install-item__head" type="button" @click="expanded = expanded === item.id ? '' : item.id">
            <PclIcon :name="item.icon" :size="18" />
            <span class="pcl-install-item__name">{{ item.label }}</span>
            <span class="pcl-install-item__version">{{ componentFor(item.uid)?.version || t('instanceSections.notInstalled') }}</span>
            <PclIcon :name="expanded === item.id ? 'chevron-up' : 'chevron-down'" :size="15" />
          </button>
          <div v-if="expanded === item.id" class="pcl-install-item__body">
            <p>{{ item.description }}</p>
            <div v-if="componentFor(item.uid)" class="pcl-install-current">
              <span>{{ t('instanceSections.currentVersion', { version: componentFor(item.uid)?.version || t('instanceSections.unknown') }) }}</span>
              <PclCheckbox
                :model-value="componentFor(item.uid)?.enabled !== false"
                :label="t('instanceSections.enable')"
                :disabled="busy || !componentFor(item.uid)?.canDisable"
                @update:model-value="emit('component-enabled', componentFor(item.uid), $event)"
              />
            </div>
            <div class="pcl-install-actions">
              <PclInput v-model="versions[item.id]" :placeholder="t('instanceSections.versionPlaceholder')" />
              <PclButton variant="highlight" :disabled="busy || !versions[item.id]?.trim() || (componentFor(item.uid) && componentFor(item.uid)?.canChangeVersion === false)" @click="emit('component-version', componentFor(item.uid) || { id: item.uid, name: item.label }, versions[item.id].trim())" >{{ t('instanceSections.applyVersion') }}</PclButton>
              <PclButton variant="ghost" icon="search" @click="emit('browse', item.uid)" >{{ t('instanceSections.browseVersions') }}</PclButton>
            </div>
          </div>
        </article>
      </div>
      <PclHint theme="blue">{{ t('instanceSections.installFooter') }}</PclHint>
    </section>

    <section v-else-if="section === 'export'" class="pcl-section-card pcl-export">
      <div class="pcl-export__head">
        <div>
          <span class="pcl-kicker">{{ t('ui.instanceExport') }}</span>
          <h2>{{ t('instanceSections.export') }}</h2>
          <p>{{ t('instanceSections.exportHint') }}</p>
        </div>
      </div>
      <div class="pcl-export__body">
        <PclHint v-if="exportError" theme="red" closable @close="exportError = ''">{{ exportError }}</PclHint>
        <div class="pcl-export__grid">
          <label class="pcl-export__field">
            <span>{{ t('instanceSections.packName') }}</span>
            <PclInput v-model="exportName" :placeholder="t('instanceSections.packNamePlaceholder')" />
          </label>
          <label class="pcl-export__field">
            <span>{{ t('instanceSections.packVersion') }}</span>
            <PclInput v-model="exportVersion" :placeholder="t('instanceSections.packVersionPlaceholder')" />
          </label>
          <label class="pcl-export__field">
            <span>{{ t('instanceSections.format') }}</span>
            <PclSelect v-model="exportFormat" :options="exportFormats" />
          </label>
          <label class="pcl-export__field">
            <span>{{ t('instanceSections.outputPath') }}</span>
            <div class="pcl-path-row">
              <PclInput v-model="exportOutput" :placeholder="exportOutputPlaceholder" />
              <PclIconButton icon="folder-open" :label="t('instanceSections.chooseOutput')" :size="28" :disabled="busy" @click="chooseExportOutput" />
            </div>
          </label>
          <label v-if="exportFormat === 'modrinth'" class="pcl-export__field pcl-export__field--wide">
            <span>{{ t('instanceSections.modrinthSummary') }}</span>
            <PclInput v-model="exportSummary" :placeholder="t('instanceSections.summaryPlaceholder')" />
          </label>
          <label v-if="exportFormat === 'curseforge'" class="pcl-export__field pcl-export__field--wide">
            <span>{{ t('instanceSections.curseforgeAuthor') }}</span>
            <PclInput v-model="exportAuthor" :placeholder="t('instanceSections.authorPlaceholder')" />
          </label>
        </div>

        <div class="pcl-export__section-title">{{ t('instanceSections.packageContent') }}</div>
        <div class="pcl-export__options">
          <div v-for="item in exportItems" :key="item.key" class="pcl-export__option" @click="toggleExport(item.key, !selectedExport.includes(item.key))">
            <PclCheckbox :model-value="selectedExport.includes(item.key)" @update:model-value="toggleExport(item.key, $event)" @click.stop />
            <span><strong>{{ item.label }}</strong><small>{{ item.description }}</small></span>
          </div>
        </div>

        <div class="pcl-export__checks">
          <PclCheckbox v-model="exportOptionalFiles" :label="t('instanceSections.markOptional')" />
          <PclCheckbox v-model="exportOverwrite" :label="t('instanceSections.overwrite')" />
        </div>

        <PclHint theme="blue">
          {{ t('instanceSections.formatHint') }}
        </PclHint>

        <div class="pcl-export__actions">
          <PclButton variant="highlight" icon="archive" :disabled="busy || exporting || !canExport" @click="startExport">
            {{ exporting ? t('instanceSections.exporting') : t('instanceSections.startExport') }}
          </PclButton>
        </div>
      </div>
    </section>

    <section v-else-if="section === 'settings'" class="pcl-section-card">
      <div class="section-heading"><div><span class="pcl-kicker">{{ t('ui.instanceSettings') }}</span><h2>{{ t('instanceSections.settings') }}</h2><p>{{ t('instanceSections.settingsHint') }}</p></div></div>
      <div class="setting-grid"><div v-for="item in instanceSettings" :key="item.key" class="setting-line"><div><strong>{{ item.label }}</strong><small>{{ item.description }}</small></div><q-toggle v-if="item.type === 'boolean'" :model-value="settingValue(item.key)" :label="settingValue(item.key) ? t('instanceSections.enable') : t('instanceSections.disable')" :disable="busy" @update:model-value="save(item.key, $event)"/><q-input v-else outlined dense :model-value="settingValue(item.key)" :label="item.label" :disable="busy" @update:model-value="save(item.key, $event)"/></div></div>
      <q-banner class="pcl-hint"><span class="material-icons">tune</span>{{ t('instanceSections.settingsNotice') }}</q-banner>
    </section>

    <section v-else-if="section === 'java'" class="pcl-section-card"><div class="section-heading"><div><span class="pcl-kicker">{{ t('ui.instanceJava') }}</span><h2>{{ t('instanceSections.java') }}</h2><p>{{ t('instanceSections.javaHint') }}</p></div></div><div class="java-layout"><div class="java-summary"><span class="material-icons">coffee</span><div><strong>{{ settingValue('JavaPath') || t('instanceSections.followLauncher') }}</strong><small>{{ t('instanceSections.javaPath') }}</small></div><q-btn outline :label="t('instanceSections.detectJava')" icon="search" :disable="busy" @click="emit('save',{key:'JavaPath',value:settingValue('JavaPath')})"/></div><div class="setting-grid"><div v-for="item in javaSettings" :key="item.key" class="setting-line"><div><strong>{{ item.label }}</strong><small>{{ item.description }}</small></div><div class="pcl-path-row"><q-input outlined dense :model-value="settingValue(item.key)" :disable="busy" @update:model-value="save(item.key,$event)"/><PclIconButton v-if="item.key === 'JavaPath'" icon="folder-open" :label="t('instanceSections.chooseJava')" :size="26" :disabled="busy" @click="chooseJavaPath" /></div></div></div></div></section>

    <PclMemoryPanel v-else-if="section === 'memory'" :settings="settings || []" :busy="busy" :execute="execute || (async () => ({}))" @save="emit('save', $event)" />

    <section v-else-if="section === 'advanced'" class="pcl-section-card"><div class="section-heading"><div><span class="pcl-kicker">{{ t('ui.instanceAdvanced') }}</span><h2>{{ t('instanceSections.advanced') }}</h2><p>{{ t('instanceSections.advancedHint') }}</p></div></div><div class="advanced-form"><label>{{ t('instanceSections.renderer') }}<select :value="settingValue('Renderer')" @change="save('Renderer',($event.target as HTMLSelectElement).value)"><option value="">{{ t('instanceSections.followGlobal') }}</option><option value="default">{{ t('instanceSections.gameDefault') }}</option><option value="software">{{ t('instanceSections.software') }}</option><option value="directx12">{{ t('ui.directx12') }}</option><option value="vulkan">{{ t('ui.vulkan') }}</option></select></label><label>{{ t('instanceSections.jvmArgs') }}<textarea :value="settingValue('JvmArgs')" rows="4" @change="save('JvmArgs',($event.target as HTMLTextAreaElement).value)" :placeholder="t('instanceSections.followGlobalPlaceholder')"/></label><label>{{ t('instanceSections.gameArgs') }}<input :value="settingValue('GameArgs')" @change="save('GameArgs',($event.target as HTMLInputElement).value)" :placeholder="t('instanceSections.followGlobalPlaceholder')"/></label><label>{{ t('instanceSections.preLaunch') }}<input :value="settingValue('PreLaunchCommand')" @change="save('PreLaunchCommand',($event.target as HTMLInputElement).value)"/></label><div class="advanced-checks"><q-toggle :model-value="Boolean(settingValue('IgnoreJavaCompatibility'))" :label="t('instanceSections.ignoreJava')" @update:model-value="save('IgnoreJavaCompatibility',$event)"/><q-toggle :model-value="Boolean(settingValue('DisableFileVerification'))" :label="t('instanceSections.disableVerification')" @update:model-value="save('DisableFileVerification',$event)"/><q-toggle :model-value="Boolean(settingValue('UseGlobalProxy'))" :label="t('instanceSections.globalProxy')" @update:model-value="save('UseGlobalProxy',$event)"/></div></div></section>

    <section v-else-if="section === 'server'" class="pcl-section-card"><div class="section-heading"><div><span class="pcl-kicker">{{ t('ui.server') }}</span><h2>{{ t('instanceSections.server') }}</h2><p>{{ t('instanceSections.serverHint') }}</p></div><div class="server-actions"><q-btn flat icon="refresh" :label="t('instanceSections.readConfig')" :disable="busy || serverLoading" @click="loadServer"/><q-btn color="primary" icon="play_arrow" :label="t('instanceSections.startServer')" :disable="busy || serverLoading || !eulaAgreed" @click="startServer"/></div></div><q-banner v-if="serverError" class="bg-red-1 text-red-9 q-mt-md">{{ serverError }}</q-banner><div class="server-grid"><q-card flat bordered class="server-card"><q-card-section><div class="server-card-title">{{ t('ui.eula') }}</div><q-toggle v-model="eulaAgreed" :label="t('instanceSections.acceptEula')" :disable="busy || serverLoading" @update:model-value="saveEula"/><p class="pcl-muted">{{ t('instanceSections.eulaHint') }}</p></q-card-section></q-card><q-card flat bordered class="server-card"><q-card-section><div class="server-card-title">{{ t('ui.serverProperties') }}</div><div class="server-properties"><label>{{ t('instanceSections.serverPort') }}<input v-model="serverPort" type="number" min="1" max="65535" @change="saveProperty('server-port',serverPort)"/></label><label>{{ t('instanceSections.maxPlayers') }}<input v-model="serverMaxPlayers" type="number" min="1" @change="saveProperty('max-players',serverMaxPlayers)"/></label><label>{{ t('pcl.server.gameMode') }}<select v-model="serverGamemode" @change="saveProperty('gamemode',serverGamemode)"><option value="survival">{{ t('instanceSections.survival') }}</option><option value="creative">{{ t('instanceSections.creative') }}</option><option value="adventure">{{ t('instanceSections.adventure') }}</option><option value="spectator">{{ t('instanceSections.spectator') }}</option></select></label><label>{{ t('instanceSections.whitelist') }}<select v-model="serverWhitelist" @change="saveProperty('white-list',serverWhitelist)"><option value="true">{{ t('instanceSections.enable') }}</option><option value="false">{{ t('instanceSections.disable') }}</option></select></label></div></q-card-section></q-card></div><q-card flat bordered class="server-console"><q-card-section><div class="server-card-title"><span>{{ t('instanceSections.serverConsole') }}</span><q-btn flat icon="terminal" :label="t('instanceSections.openTerminal')" @click="emit('console')"/></div><pre>{{ consoleText || t('instanceSections.consoleHint') }}</pre><div class="console-command"><input v-model="serverCommand" :placeholder="t('instanceSections.commandPlaceholder')" @keyup.enter="sendServerCommand"/><q-btn color="primary" :label="t('instanceSections.send')" :disable="busy || !serverCommand.trim()" @click="sendServerCommand"/></div></q-card-section></q-card></section>
  </div>
</template>
<script setup lang="ts">
import { computed, onMounted, reactive, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import PclButton from '../../components/PclButton.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclInput from '../../components/PclInput.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import { pickPclDirectory, pickPclSavePath } from '../../composables/usePclFileDialog';
import PclSelect from '../../components/PclSelect.vue';
type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props=defineProps<{section:string;components:any[];componentError?:string;settings?:any[];busy:boolean;instance?:string;execute?:Execute}>();
const { t } = useI18n();
const emit=defineEmits<{refresh:[];browse:[uid:string];'component-enabled':[component:any,enabled:boolean];'component-version':[component:any,version:string];export:[payload:any];save:[payload:{key:string;value:any}];console:[]}>();
const expanded=ref('minecraft'), versions=reactive<Record<string,string>>({}), exportName=ref(''), exportVersion=ref('1.0'), exportOutput=ref(''), exportFormat=ref('zip'), exportSummary=ref(''), exportAuthor=ref(''), exportOptionalFiles=ref(false), exportOverwrite=ref(true), exportError=ref(''), exporting=ref(false), selectedExport=ref(['game','gameSettings','gameProfile','mod','resourcepacks','shaderpacks','saves','screenshots']);
const exportFormats=[{label:t('instanceSections.mmcZip'),value:'zip'},{label:t('instanceSections.modrinthPack'),value:'modrinth'},{label:t('instanceSections.curseforgePack'),value:'curseforge'}];
const exportOutputPlaceholder=computed(()=>exportFormat.value==='modrinth'?t('instanceSections.modrinthPath'):t('instanceSections.zipPath'));
const canExport=computed(()=>Boolean(exportName.value.trim() && exportOutput.value.trim() && (exportFormat.value==='zip' || exportVersion.value.trim())));
function toggleExport(key:string, enabled:boolean){selectedExport.value=enabled?[...selectedExport.value,key]:selectedExport.value.filter(item=>item!==key);}
async function chooseExportOutput() {
  const extension = exportFormat.value === 'modrinth' ? 'mrpack' : 'zip';
  const selected = await pickPclSavePath({ title: t('instanceSections.chooseOutput'), defaultPath: exportOutput.value || `${exportName.value || 'instance'}.${extension}`, filters: [{ name: exportFormat.value === 'modrinth' ? t('instanceSections.modrinthPack') : t('instanceSections.zipArchive'), extensions: [extension] }] });
  if (selected) exportOutput.value = selected;
}

async function chooseJavaPath() {
  const selected = await pickPclDirectory({ title: t('java.chooseDirectory') });
  if (selected) save('JavaPath', selected);
}

function startExport(){
  exportError.value='';
  if(!exportName.value.trim()){exportError.value=t('instanceSections.nameRequired');return;}
  if(!exportOutput.value.trim()){exportError.value=t('instanceSections.outputRequired');return;}
  if(exportFormat.value!=='zip'&&!exportVersion.value.trim()){exportError.value=t('instanceSections.versionRequired');return;}
  exporting.value=true;
  emit('export',{name:exportName.value.trim(),version:exportVersion.value.trim(),output:exportOutput.value.trim(),format:exportFormat.value,summary:exportSummary.value.trim(),author:exportAuthor.value.trim(),optionalFiles:exportOptionalFiles.value,overwrite:exportOverwrite.value,items:selectedExport.value});
  window.setTimeout(()=>{exporting.value=false;},800);
}
const serverLoading=ref(false), serverError=ref(''), eulaAgreed=ref(false), serverPort=ref(25565), serverMaxPlayers=ref(20), serverGamemode=ref('survival'), serverWhitelist=ref('false'), serverCommand=ref(''), consoleText=ref('');
const installItems=[
  {id:'minecraft',label:'Minecraft',icon:'box',uid:'net.minecraft',description:t('instanceSections.minecraftDescription')},
  {id:'forge',label:'Forge',icon:'anvil',uid:'net.minecraftforge',description:t('instanceSections.forgeDescription')},
  {id:'neoforge',label:'NeoForge',icon:'cat',uid:'net.neoforged',description:t('instanceSections.neoforgeDescription')},
  {id:'cleanroom',label:'Cleanroom',icon:'flask-conical',uid:'com.cleanroommc.cleanroom',description:t('instanceSections.cleanroomDescription')},
  {id:'fabric',label:'Fabric',icon:'scroll',uid:'net.fabricmc.fabric-loader',description:t('instanceSections.fabricDescription')},
  {id:'quilt',label:'Quilt',icon:'layers',uid:'org.quiltmc.quilt-loader',description:t('instanceSections.quiltDescription')},
  {id:'liteloader',label:'LiteLoader',icon:'egg',uid:'com.mumfrey.liteloader',description:t('instanceSections.liteloaderDescription')},
];
const exportItems=[{key:'game',label:t('instanceSections.gameCore'),description:t('instanceSections.gameCoreDescription')},{key:'gameSettings',label:t('instanceSections.gameSettings'),description:t('instanceSections.gameSettingsDescription')},{key:'gameProfile',label:t('instanceSections.gameProfile'),description:t('instanceSections.gameProfileDescription')},{key:'mod',label:t('resource.kinds.mods'),description:t('instanceSections.modsDescription')},{key:'resourcepacks',label:t('resource.kinds.resourcepacks'),description:t('instanceSections.resourcepacksDescription')},{key:'shaderpacks',label:t('resource.kinds.shaderpacks'),description:t('instanceSections.shaderpacksDescription')},{key:'saves',label:t('extras.saves'),description:t('instanceSections.savesDescription')},{key:'screenshots',label:t('extras.screenshots'),description:t('instanceSections.screenshotsDescription')},{key:'servers',label:t('instanceSections.serverList'),description:'servers.dat'}];
const instanceSettings=[{key:'InstanceAccountId',label:t('instanceSections.account'),description:t('instanceSections.accountDescription'),type:'string'},{key:'InstanceWindowWidth',label:t('instanceSections.windowWidth'),description:t('instanceSections.windowWidthDescription'),type:'string'},{key:'InstanceWindowHeight',label:t('instanceSections.windowHeight'),description:t('instanceSections.windowHeightDescription'),type:'string'},{key:'OverrideJavaArgs',label:t('instanceSections.customJavaArgs'),description:t('instanceSections.customJavaArgsDescription'),type:'boolean'},{key:'Fullscreen',label:t('instanceSections.fullscreen'),description:t('instanceSections.fullscreenDescription'),type:'boolean'}];
const javaSettings=[{key:'JavaPath',label:t('instanceSections.javaPathLabel'),description:t('instanceSections.javaPathDescription')},{key:'JavaArguments',label:t('instanceSections.javaArguments'),description:t('instanceSections.javaArgumentsDescription')}];
function componentFor(uid:string){
  const target=String(uid||'').trim().toLowerCase();
  const aliases:Record<string,string[]>={
    'net.minecraft':['net.minecraft','minecraft'],
    'net.minecraftforge':['net.minecraftforge','forge','net.minecraftforge:forge'],
    'net.neoforged':['net.neoforged','neoforge','net.neoforged:neoforge'],
    'com.cleanroommc.cleanroom':['com.cleanroommc.cleanroom','cleanroom'],
    'net.fabricmc.fabric-loader':['net.fabricmc.fabric-loader','fabric','fabric-loader'],
    'org.quiltmc.quilt-loader':['org.quiltmc.quilt-loader','quilt','quilt-loader'],
    'com.mumfrey.liteloader':['com.mumfrey.liteloader','liteloader','lite-loader'],
  };
  const accepted=aliases[target] || [target];
  return props.components.find(c=>{
    const ids=[c?.id,c?.uid,c?.component,c?.name].map(value=>String(value||'').trim().toLowerCase());
    return ids.some(id=>accepted.includes(id) || accepted.some(alias=>id.startsWith(`${alias}:`)));
  });
}
function settingValue(key:string){return props.settings?.find(s=>s.key===key)?.value ?? (key==='Fullscreen'||key==='OverrideJavaArgs'?false:'');}
function save(key:string,value:any){emit('save',{key,value});}
async function loadServer(){
  if (!props.execute || !props.instance) return;
  serverLoading.value=true; serverError.value='';
  try {
    const [eula, properties] = await Promise.all([
      props.execute('server.eula.read',{instance:props.instance}),
      props.execute('server.properties.read',{instance:props.instance}),
    ]);
    eulaAgreed.value=Boolean(eula?.agreed ?? eula?.eula);
    const values=properties?.properties || properties || {};
    serverPort.value=Number(values['server-port'] || 25565); serverMaxPlayers.value=Number(values['max-players'] || 20);
    serverGamemode.value=String(values.gamemode || 'survival'); serverWhitelist.value=String(values['white-list'] ?? 'false');
  } catch(e) { serverError.value=e instanceof Error?e.message:String(e); } finally { serverLoading.value=false; }
}
async function saveEula(value:boolean){ if(!props.execute||!props.instance)return; try{await props.execute('server.eula.write',{instance:props.instance,agreed:value});}catch(e){serverError.value=e instanceof Error?e.message:String(e);} }
async function saveProperty(key:string,value:any){ if(!props.execute||!props.instance)return; try{const current=await props.execute('server.properties.read',{instance:props.instance});const properties={...(current?.properties||current||{}),[key]:String(value)};await props.execute('server.properties.write',{instance:props.instance,properties});}catch(e){serverError.value=e instanceof Error?e.message:String(e);} }
async function startServer(){
  if(!props.execute||!props.instance)return;
  serverLoading.value=true;serverError.value='';
  try{
    let task:any=await props.execute('neo.task.start',{operation:'server.start',priority:'high',parameters:{instance:props.instance}});
    while(task && !task.finished){await new Promise(resolve=>setTimeout(resolve,300));task=await props.execute('neo.task.status',{taskId:task.taskId});}
    if(!task || task.state!=='succeeded')throw new Error(task?.error||t('instanceSections.serverStartFailed'));
    consoleText.value=t('instanceSections.serverStartRequested');
  }catch(e){serverError.value=e instanceof Error?e.message:String(e);}finally{serverLoading.value=false;}
}
async function sendServerCommand(){ const command=serverCommand.value.trim();if(!command||!props.execute||!props.instance)return;try{await props.execute('server.console.command',{instance:props.instance,command});consoleText.value += `${consoleText.value?'\n':''}> ${command}`;serverCommand.value='';}catch(e){serverError.value=e instanceof Error?e.message:String(e);} }
watch(exportFormat,value=>{
  const suffix=value==='modrinth'?'.mrpack':'.zip';
  if(!exportOutput.value)return;
  if(/\.(zip|mrpack)$/i.test(exportOutput.value))exportOutput.value=exportOutput.value.replace(/\.(zip|mrpack)$/i,suffix);
},{immediate:true});
watch(()=>[props.section,props.instance],([section])=>{if(section==='server')void loadServer();});
onMounted(()=>{if(props.section==='server')void loadServer();});
</script>
<style scoped>
.pcl-instance-sections{margin-top:22px}.pcl-section-card{padding:24px;border:1px solid #d5e0e8;background:#fff;border-radius:7px;box-shadow:0 4px 13px #22445f12;color:#405b72}.section-heading{display:flex;justify-content:space-between;align-items:flex-start;padding-bottom:20px;border-bottom:1px solid #e1e8ee}.section-heading h2{margin:5px 0;color:#315a79;font-size: calc(24px * var(--pcl-font-scale, 1))}.section-heading p{margin:0;color:#83929f;font-size: calc(13px * var(--pcl-font-scale, 1))}.pcl-kicker{color:#4284b2;font-size: calc(10px * var(--pcl-font-scale, 1));font-weight:800;letter-spacing:.15em}.install-list{display:flex;flex-direction:column;gap:10px;margin-top:20px}.install-card{border:1px solid #d9e4eb;border-radius:6px;overflow:hidden;background:#fbfcfd}.install-card.expanded{border-color:#75a8ca;box-shadow:0 2px 8px #3c7eaa1c}.install-card__head{display:flex;align-items:center;gap:12px;width:100%;height:54px;padding:0 16px;border:0;background:transparent;color:#4c6c83;text-align:left;cursor:pointer}.install-card__logo{width:28px;color:#68a0c5;font-size: calc(21px * var(--pcl-font-scale, 1))}.install-card__name{font-weight:650}.install-card__version{margin-left:auto;color:#8c9da9;font-size: calc(12px * var(--pcl-font-scale, 1))}.install-card__arrow{margin-left:12px;color:#86a0b1}.install-card__body{padding:14px 20px 18px;border-top:1px solid #e4ebef;background:#fff;color:#748794}.install-card__body p{margin:0 0 12px}.component-current{display:flex;justify-content:space-between;align-items:center;padding:8px 10px;background:#f3f7fa;border-radius:4px}.install-actions{display:flex;gap:9px;align-items:center;margin-top:12px}.install-actions .q-input{width:220px}.pcl-hint{margin-top:18px;border:1px solid #d7e6ef;background:#f2f8fc!important;color:#698296}.export-form{display:flex;flex-direction:column;gap:13px;max-width:850px;margin-top:20px}.export-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:9px}.export-option{display:flex;align-items:flex-start;gap:5px;padding:11px;border:1px solid #dbe5eb;border-radius:5px;background:#fbfcfd;cursor:pointer}.export-option span{display:flex;flex-direction:column;gap:3px}.export-option strong{color:#4a687f;font-size: calc(13px * var(--pcl-font-scale, 1))}.export-option small{color:#91a0aa;font-size: calc(11px * var(--pcl-font-scale, 1))}.export-advanced{border:1px solid #dbe5eb!important;background:#f8fafb!important}.export-advanced__title{display:flex;align-items:center;gap:20px;flex-wrap:wrap;color:#4b6a82;font-weight:650}.export-advanced p{margin:8px 0 0;color:#8b9ba7;font-size: calc(12px * var(--pcl-font-scale, 1))}.setting-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px 18px;margin-top:20px}.setting-line{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:13px;border:1px solid #dbe5eb;border-radius:5px;background:#fbfcfd}.setting-line strong,.setting-line small{display:block}.setting-line strong{color:#4e6b80;font-size: calc(13px * var(--pcl-font-scale, 1))}.setting-line small{margin-top:4px;color:#96a3ad;font-size: calc(11px * var(--pcl-font-scale, 1))}.setting-line .q-input{width:150px}.server-empty{text-align:center;padding:75px 20px;color:#8898a4}.server-empty .material-icons{font-size: calc(48px * var(--pcl-font-scale, 1));color:#79a6c1}.server-empty h3{margin:10px 0 5px;color:#496a82}.server-empty p{margin:0 0 18px}@media(max-width:700px){.setting-grid,.export-grid{grid-template-columns:1fr}.install-actions{flex-wrap:wrap}.install-actions .q-input{width:100%}}
.java-layout{margin-top:18px}.java-summary{display:flex;align-items:center;gap:10px;padding:14px;border:1px solid #dce7ec;border-radius:6px;background:#f7fafb}.java-summary>.material-icons{font-size: calc(28px * var(--pcl-font-scale, 1));color:#6598b7}.java-summary>div{display:flex;flex-direction:column;flex:1}.java-summary small{margin-top:3px;color:#8e9faa;font-size: calc(11px * var(--pcl-font-scale, 1))}.memory-card{margin-top:20px;padding:22px;border:1px solid #dbe6eb;border-radius:6px;background:#f8fbfc}.memory-values{display:grid;grid-template-columns:auto 1fr auto 1fr;align-items:end;gap:8px 12px}.memory-values strong{color:#4e7998;font-size: calc(22px * var(--pcl-font-scale, 1))}.memory-values span{color:#8d9da7;font-size: calc(11px * var(--pcl-font-scale, 1))}.memory-range{width:100%;margin:24px 0 12px;accent-color:#4d91bd}.memory-bar{height:7px;overflow:hidden;border-radius:4px;background:#dce8ee}.memory-bar span{display:block;height:100%;background:linear-gradient(90deg,#68a8cd,#3d83b7);transition:width .25s}.memory-card p{margin:14px 0 0;color:#8a9aa5;font-size: calc(11px * var(--pcl-font-scale, 1))}.advanced-form{display:flex;flex-direction:column;gap:14px;margin-top:18px}.advanced-form label{display:flex;align-items:center;gap:14px;color:#627a8c;font-size: calc(12px * var(--pcl-font-scale, 1))}.advanced-form input,.advanced-form select,.advanced-form textarea{flex:1;min-width:0;padding:9px 10px;border:1px solid #cbdbe5;border-radius:5px;background:#fff;color:#4f687b;font:inherit}.advanced-form textarea{resize:vertical}.advanced-checks{display:flex;flex-wrap:wrap;gap:8px 20px;padding-top:6px;border-top:1px solid #e1e8ed}@media(max-width:700px){.memory-values{grid-template-columns:1fr 1fr}.advanced-form label{display:block}.advanced-form label input,.advanced-form label select,.advanced-form label textarea{display:block;width:100%;margin-top:5px}}
.server-actions{display:flex;gap:8px;flex-wrap:wrap}.server-grid{display:grid;grid-template-columns:.8fr 1.2fr;gap:14px;margin-top:18px}.server-card,.server-console{border-color:#dbe5eb!important;background:#fbfcfd!important}.server-card-title{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px;color:#4a687f;font-weight:650}.pcl-muted{color:#8a9aa5;font-size: calc(11px * var(--pcl-font-scale, 1))}.server-properties{display:grid;grid-template-columns:1fr 1fr;gap:10px}.server-properties label{display:flex;flex-direction:column;gap:5px;color:#637b8d;font-size: calc(11px * var(--pcl-font-scale, 1))}.server-properties input,.server-properties select,.console-command input{height:34px;padding:0 9px;border:1px solid #cbdbe5;border-radius:5px;background:#fff;color:#4f687b}.server-console{margin-top:14px}.server-console pre{min-height:150px;max-height:300px;margin:0;padding:12px;overflow:auto;border-radius:5px;background:#101923;color:#c7d4df;font: calc(12px * var(--pcl-font-scale, 1))/1.55 Consolas,monospace;white-space:pre-wrap}.console-command{display:flex;gap:8px;margin-top:10px}.console-command input{flex:1}@media(max-width:700px){.server-grid{grid-template-columns:1fr}.server-properties{grid-template-columns:1fr}}
.pcl-install-section {
  padding: 20px;
  border-color: var(--pcl-card-border);
  background: var(--pcl-card);
  color: var(--pcl-text);
  box-shadow: var(--pcl-shadow);
}

.pcl-install-head {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 12px;
  padding-bottom: 14px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-install-head h2 {
  margin: 4px 0;
  color: var(--pcl-text);
  font-size: calc(19px * var(--pcl-font-scale, 1));
}

.pcl-install-head p {
  margin: 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-install-section .pcl-kicker {
  color: var(--pcl-c2);
}

.pcl-install-list {
  display: flex;
  flex-direction: column;
  gap: 7px;
  margin-top: 13px;
}

.pcl-install-item {
  overflow: hidden;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-half-white);
}

.pcl-install-item.expanded {
  border-color: var(--pcl-c3);
}

.pcl-install-item__head {
  display: flex;
  align-items: center;
  gap: 10px;
  width: 100%;
  height: 45px;
  padding: 0 12px;
  border: 0;
  background: transparent;
  color: var(--pcl-text);
  text-align: left;
  cursor: pointer;
}

.pcl-install-item__head:hover {
  background: var(--pcl-hover);
}

.pcl-install-item__name {
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 600;
}

.pcl-install-item__version {
  margin-left: auto;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-install-item__body {
  padding: 10px 12px 13px;
  border-top: 1px solid var(--pcl-border);
  background: var(--pcl-c8);
}

.pcl-install-item__body p {
  margin: 0 0 9px;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-install-current {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  padding: 7px 9px;
  border-radius: 4px;
  background: var(--pcl-c6);
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-install-actions {
  display: flex;
  align-items: center;
  gap: 7px;
  margin-top: 9px;
}

.pcl-install-actions :deep(.pcl-input) {
  flex: 1;
}

.pcl-install-section :deep(.pcl-hint) {
  margin-top: 12px;
}

/* Export page uses the same card/input language as the rest of Neo UI. */
.pcl-section-card.pcl-export {
  padding: 0;
  overflow: hidden;
  border: 1px solid var(--pcl-card-border);
  background: var(--pcl-card);
  color: var(--pcl-text);
  box-shadow: var(--pcl-shadow);
}

.pcl-export__head {
  padding: 20px 22px 15px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-export__head h2 {
  margin: 4px 0 3px;
  color: var(--pcl-text);
  font-size: calc(20px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-export__head p {
  margin: 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-export__body {
  display: flex;
  flex-direction: column;
  gap: 14px;
  padding: 18px 22px 20px;
}

.pcl-export__grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px 16px;
}

.pcl-export__field {
  display: flex;
  flex-direction: column;
  gap: 5px;
  min-width: 0;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-export__field--wide { grid-column: 1 / -1; }

.pcl-export__section-title {
  padding-top: 2px;
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-export__options {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
}

.pcl-export__option {
  display: flex;
  align-items: flex-start;
  gap: 8px;
  min-width: 0;
  padding: 9px 10px;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c8);
  cursor: pointer;
}

.pcl-export__option > span {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
}

.pcl-export__option strong {
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-export__option small {
  overflow: hidden;
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-export__checks {
  display: flex;
  flex-wrap: wrap;
  gap: 8px 22px;
  padding: 2px 0;
}

.pcl-export__actions {
  display: flex;
  justify-content: flex-end;
  padding-top: 2px;
}

@media (max-width: 700px) {
  .pcl-export__grid,
  .pcl-export__options { grid-template-columns: 1fr; }
}
.pcl-path-row { display: flex; align-items: center; gap: 6px; min-width: 0; }
.pcl-path-row > :first-child { flex: 1; min-width: 0; }
</style>
