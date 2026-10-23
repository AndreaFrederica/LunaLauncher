<template>
  <div class="pcl-instance-sections">
    <section v-if="section === 'install'" class="pcl-section-card pcl-install-section">
      <div class="pcl-install-head">
        <div>
          <span class="pcl-kicker">INSTANCE INSTALLATION</span>
          <h2>安装</h2>
          <p>按 Luna 的组件顺序查看和调整当前实例的 Minecraft 与加载器。</p>
        </div>
        <PclButton size="sm" icon="refresh-cw" :disabled="busy" @click="emit('refresh')">刷新</PclButton>
      </div>
      <div class="pcl-install-list">
        <article v-for="item in installItems" :key="item.id" class="pcl-install-item" :class="{ expanded: expanded === item.id }">
          <button class="pcl-install-item__head" type="button" @click="expanded = expanded === item.id ? '' : item.id">
            <PclIcon :name="item.icon" :size="18" />
            <span class="pcl-install-item__name">{{ item.label }}</span>
            <span class="pcl-install-item__version">{{ componentFor(item.id)?.version || '未安装' }}</span>
            <PclIcon :name="expanded === item.id ? 'chevron-up' : 'chevron-down'" :size="15" />
          </button>
          <div v-if="expanded === item.id" class="pcl-install-item__body">
            <p>{{ item.description }}</p>
            <div v-if="componentFor(item.id)" class="pcl-install-current">
              <span>当前版本：{{ componentFor(item.id)?.version || '未知' }}</span>
              <PclCheckbox
                :model-value="componentFor(item.id)?.enabled !== false"
                label="启用"
                :disabled="busy || !componentFor(item.id)?.canDisable"
                @update:model-value="emit('component-enabled', componentFor(item.id), $event)"
              />
            </div>
            <div class="pcl-install-actions">
              <PclInput v-model="versions[item.id]" placeholder="输入版本号" />
              <PclButton variant="highlight" :disabled="busy || !versions[item.id]?.trim() || !componentFor(item.id)" @click="componentFor(item.id) && emit('component-version', componentFor(item.id), versions[item.id].trim())">应用版本</PclButton>
              <PclButton variant="ghost" icon="search" @click="emit('browse', item.uid)">浏览可用版本</PclButton>
            </div>
          </div>
        </article>
      </div>
      <PclHint theme="blue">安装项的顺序和可选状态与 Luna 实例安装页保持一致；版本变更会通过启动器组件 API 写入实例。</PclHint>
    </section>

    <section v-else-if="section === 'export'" class="pcl-section-card">
      <div class="section-heading"><div><span class="pcl-kicker">INSTANCE EXPORT</span><h2>导出实例</h2><p>选择要打包的内容，然后导出为整合包文件。</p></div></div>
      <div class="export-form"><q-input v-model="exportName" outlined label="整合包名称"/><q-input v-model="exportVersion" outlined label="整合包版本"/><q-input v-model="exportOutput" outlined label="输出 ZIP 路径" placeholder="例如：D:/Export/pack.zip"/><div class="export-grid"><label v-for="item in exportItems" :key="item.key" class="export-option"><q-checkbox v-model="selectedExport" :val="item.key" color="primary"/><span><strong>{{ item.label }}</strong><small>{{ item.description }}</small></span></label></div><q-card flat class="export-advanced"><q-card-section><div class="export-advanced__title"><span>高级选项</span><q-toggle v-model="bundleFiles" label="打包外部文件"/><q-toggle v-model="includeLauncher" label="包含启动器配置"/></div><p>导出的内容会遵循当前实例的文件结构，并保留 Mod、资源包、存档和截图等选项。</p></q-card-section></q-card><q-btn color="primary" icon="archive" label="开始导出" :disable="busy || !exportName.trim() || !exportOutput.trim()" @click="emit('export', { name: exportName.trim(), version: exportVersion.trim(), output: exportOutput.trim(), items: selectedExport, bundleFiles, includeLauncher })"/></div>
    </section>

    <section v-else-if="section === 'settings'" class="pcl-section-card">
      <div class="section-heading"><div><span class="pcl-kicker">INSTANCE SETTINGS</span><h2>实例设置</h2><p>这些选项只作用于当前实例，布局参照 Luna 的实例设置页。</p></div></div>
      <div class="setting-grid"><div v-for="item in instanceSettings" :key="item.key" class="setting-line"><div><strong>{{ item.label }}</strong><small>{{ item.description }}</small></div><q-toggle v-if="item.type === 'boolean'" :model-value="settingValue(item.key)" :label="settingValue(item.key) ? '启用' : '关闭'" :disable="busy" @update:model-value="save(item.key, $event)"/><q-input v-else outlined dense :model-value="settingValue(item.key)" :label="item.label" :disable="busy" @update:model-value="save(item.key, $event)"/></div></div>
      <q-banner class="pcl-hint"><span class="material-icons">tune</span>更完整的实例参数仍可在启动器设置 API 中读取；此页优先呈现 Luna 用户最常用的启动、Java 和内存选项。</q-banner>
    </section>

    <section v-else-if="section === 'java'" class="pcl-section-card"><div class="section-heading"><div><span class="pcl-kicker">INSTANCE JAVA</span><h2>Java 设置</h2><p>选择此实例使用的 Java，并检测运行时兼容性。</p></div></div><div class="java-layout"><div class="java-summary"><span class="material-icons">coffee</span><div><strong>{{ settingValue('JavaPath') || '跟随启动器设置' }}</strong><small>当前 Java 路径</small></div><q-btn outline label="检测 Java" icon="search" :disable="busy" @click="emit('save',{key:'JavaPath',value:settingValue('JavaPath')})"/></div><div class="setting-grid"><div v-for="item in javaSettings" :key="item.key" class="setting-line"><div><strong>{{ item.label }}</strong><small>{{ item.description }}</small></div><q-input outlined dense :model-value="settingValue(item.key)" :disable="busy" @update:model-value="save(item.key,$event)"/></div></div></div></section>

    <PclMemoryPanel v-else-if="section === 'memory'" :settings="settings || []" :busy="busy" :execute="execute || (async () => ({}))" @save="emit('save', $event)" />

    <section v-else-if="section === 'advanced'" class="pcl-section-card"><div class="section-heading"><div><span class="pcl-kicker">INSTANCE ADVANCED</span><h2>高级启动</h2><p>对应 PCL 的渲染器、JVM 参数、游戏参数和启动前命令。</p></div></div><div class="advanced-form"><label>渲染器<select :value="settingValue('Renderer')" @change="save('Renderer',($event.target as HTMLSelectElement).value)"><option value="">跟随全局设置</option><option value="default">游戏默认</option><option value="software">软件渲染</option><option value="directx12">DirectX 12</option><option value="vulkan">Vulkan</option></select></label><label>JVM 参数<textarea :value="settingValue('JvmArgs')" rows="4" @change="save('JvmArgs',($event.target as HTMLTextAreaElement).value)" placeholder="留空表示跟随全局设置"/></label><label>游戏参数<input :value="settingValue('GameArgs')" @change="save('GameArgs',($event.target as HTMLInputElement).value)" placeholder="留空表示跟随全局设置"/></label><label>启动前命令<input :value="settingValue('PreLaunchCommand')" @change="save('PreLaunchCommand',($event.target as HTMLInputElement).value)"/></label><div class="advanced-checks"><q-toggle :model-value="Boolean(settingValue('IgnoreJavaCompatibility'))" label="忽略 Java 兼容性检查" @update:model-value="save('IgnoreJavaCompatibility',$event)"/><q-toggle :model-value="Boolean(settingValue('DisableFileVerification'))" label="禁用文件校验" @update:model-value="save('DisableFileVerification',$event)"/><q-toggle :model-value="Boolean(settingValue('UseGlobalProxy'))" label="使用全局代理" @update:model-value="save('UseGlobalProxy',$event)"/></div></div></section>

    <section v-else-if="section === 'server'" class="pcl-section-card"><div class="section-heading"><div><span class="pcl-kicker">SERVER</span><h2>服务端</h2><p>管理服务端地址、EULA、属性和运行终端。</p></div><div class="server-actions"><q-btn flat icon="refresh" label="读取配置" :disable="busy || serverLoading" @click="loadServer"/><q-btn color="primary" icon="play_arrow" label="启动服务端" :disable="busy || serverLoading || !eulaAgreed" @click="startServer"/></div></div><q-banner v-if="serverError" class="bg-red-1 text-red-9 q-mt-md">{{ serverError }}</q-banner><div class="server-grid"><q-card flat bordered class="server-card"><q-card-section><div class="server-card-title">EULA</div><q-toggle v-model="eulaAgreed" label="我同意 Minecraft 服务端 EULA" :disable="busy || serverLoading" @update:model-value="saveEula"/><p class="pcl-muted">服务端必须写入 eula=true 后才能启动。</p></q-card-section></q-card><q-card flat bordered class="server-card"><q-card-section><div class="server-card-title">server.properties</div><div class="server-properties"><label>服务器端口<input v-model="serverPort" type="number" min="1" max="65535" @change="saveProperty('server-port',serverPort)"/></label><label>最大玩家<input v-model="serverMaxPlayers" type="number" min="1" @change="saveProperty('max-players',serverMaxPlayers)"/></label><label>游戏模式<select v-model="serverGamemode" @change="saveProperty('gamemode',serverGamemode)"><option value="survival">生存</option><option value="creative">创造</option><option value="adventure">冒险</option><option value="spectator">旁观</option></select></label><label>白名单<select v-model="serverWhitelist" @change="saveProperty('white-list',serverWhitelist)"><option value="true">启用</option><option value="false">关闭</option></select></label></div></q-card-section></q-card></div><q-card flat bordered class="server-console"><q-card-section><div class="server-card-title"><span>服务端控制台</span><q-btn flat icon="terminal" label="打开实例终端" @click="emit('console')"/></div><pre>{{ consoleText || '服务端尚未启动，控制台输出会显示在这里。' }}</pre><div class="console-command"><input v-model="serverCommand" placeholder="输入服务端命令，例如 list" @keyup.enter="sendServerCommand"/><q-btn color="primary" label="发送" :disable="busy || !serverCommand.trim()" @click="sendServerCommand"/></div></q-card-section></q-card></section>
  </div>
</template>
<script setup lang="ts">
import { computed, onMounted, reactive, ref, watch } from 'vue';
import PclButton from '../../components/PclButton.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclInput from '../../components/PclInput.vue';
type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props=defineProps<{section:string;components:any[];settings?:any[];busy:boolean;instance?:string;execute?:Execute}>();
const emit=defineEmits<{refresh:[];browse:[uid:string];'component-enabled':[component:any,enabled:boolean];'component-version':[component:any,version:string];export:[payload:any];save:[payload:{key:string;value:any}];console:[]}>();
const expanded=ref('minecraft'), versions=reactive<Record<string,string>>({}), exportName=ref(''), exportVersion=ref('1.0'), exportOutput=ref(''), bundleFiles=ref(false), includeLauncher=ref(false), selectedExport=ref(['game','gameSettings','gameProfile','mod','resourcepacks','shaderpacks','saves','screenshots']);
const serverLoading=ref(false), serverError=ref(''), eulaAgreed=ref(false), serverPort=ref(25565), serverMaxPlayers=ref(20), serverGamemode=ref('survival'), serverWhitelist=ref('false'), serverCommand=ref(''), consoleText=ref('');
const installItems=[{id:'minecraft',label:'Minecraft',icon:'box',uid:'net.minecraft',description:'选择实例使用的 Minecraft 客户端版本。'},{id:'forge',label:'Forge',icon:'anvil',uid:'net.minecraftforge',description:'安装 Forge 加载器及其依赖。'},{id:'neoforge',label:'NeoForge',icon:'cat',uid:'net.neoforged',description:'安装 NeoForge 加载器。'},{id:'cleanroom',label:'Cleanroom',icon:'flask-conical',uid:'com.cleanroommc:cleanroom',description:'安装 Cleanroom 加载器。'},{id:'fabric',label:'Fabric',icon:'scroll',uid:'net.fabricmc.fabric-loader',description:'安装 Fabric 加载器。'},{id:'legacy-fabric',label:'Legacy Fabric',icon:'scroll',uid:'net.legacyfabric.loader',description:'安装旧版 Fabric 加载器。'},{id:'fabric-api',label:'Fabric API',icon:'puzzle',uid:'net.fabricmc.fabric-api',description:'安装 Fabric API。'},{id:'optifine',label:'OptiFine',icon:'gauge',uid:'optifine',description:'安装 OptiFine 或 OptiFabric。'},{id:'liteloader',label:'LiteLoader',icon:'egg',uid:'com.mumfrey:liteloader',description:'安装 LiteLoader。'}];
const exportItems=[{key:'game',label:'游戏核心',description:'Minecraft 客户端与加载器'},{key:'gameSettings',label:'游戏设置',description:'options.txt 等设置文件'},{key:'gameProfile',label:'游戏配置',description:'实例组件配置'},{key:'mod',label:'模组',description:'mods 文件夹和禁用模组'},{key:'resourcepacks',label:'资源包',description:'resourcepacks 文件夹'},{key:'shaderpacks',label:'光影',description:'shaderpacks 文件夹'},{key:'saves',label:'存档',description:'saves 文件夹'},{key:'screenshots',label:'截图',description:'screenshots 文件夹'},{key:'servers',label:'服务器列表',description:'servers.dat'}];
const instanceSettings=[{key:'InstanceAccountId',label:'登录账户',description:'此实例使用的账户',type:'string'},{key:'InstanceWindowWidth',label:'窗口宽度',description:'启动时的窗口宽度',type:'string'},{key:'InstanceWindowHeight',label:'窗口高度',description:'启动时的窗口高度',type:'string'},{key:'OverrideJavaArgs',label:'自定义 Java 参数',description:'覆盖默认 JVM 参数',type:'boolean'},{key:'Fullscreen',label:'全屏启动',description:'启动后进入全屏模式',type:'boolean'}];
const javaSettings=[{key:'JavaPath',label:'Java 路径',description:'留空时自动选择兼容版本'},{key:'JavaArguments',label:'Java 参数',description:'实例专用 JVM 参数'}];
function componentFor(id:string){return props.components.find(c=>{const x=String(c.id||'').toLowerCase();return id==='minecraft'?x.includes('minecraft'):x.includes(id.replace('-',''));});}
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
    if(!task || task.state!=='succeeded')throw new Error(task?.error||'服务端启动失败');
    consoleText.value='服务端已请求启动，请通过控制台查看输出。';
  }catch(e){serverError.value=e instanceof Error?e.message:String(e);}finally{serverLoading.value=false;}
}
async function sendServerCommand(){ const command=serverCommand.value.trim();if(!command||!props.execute||!props.instance)return;try{await props.execute('server.console.command',{instance:props.instance,command});consoleText.value += `${consoleText.value?'\n':''}> ${command}`;serverCommand.value='';}catch(e){serverError.value=e instanceof Error?e.message:String(e);} }
watch(()=>[props.section,props.instance],([section])=>{if(section==='server')void loadServer();});
onMounted(()=>{if(props.section==='server')void loadServer();});
</script>
<style scoped>
.pcl-instance-sections{margin-top:22px}.pcl-section-card{padding:24px;border:1px solid #d5e0e8;background:#fff;border-radius:7px;box-shadow:0 4px 13px #22445f12;color:#405b72}.section-heading{display:flex;justify-content:space-between;align-items:flex-start;padding-bottom:20px;border-bottom:1px solid #e1e8ee}.section-heading h2{margin:5px 0;color:#315a79;font-size:24px}.section-heading p{margin:0;color:#83929f;font-size:13px}.pcl-kicker{color:#4284b2;font-size:10px;font-weight:800;letter-spacing:.15em}.install-list{display:flex;flex-direction:column;gap:10px;margin-top:20px}.install-card{border:1px solid #d9e4eb;border-radius:6px;overflow:hidden;background:#fbfcfd}.install-card.expanded{border-color:#75a8ca;box-shadow:0 2px 8px #3c7eaa1c}.install-card__head{display:flex;align-items:center;gap:12px;width:100%;height:54px;padding:0 16px;border:0;background:transparent;color:#4c6c83;text-align:left;cursor:pointer}.install-card__logo{width:28px;color:#68a0c5;font-size:21px}.install-card__name{font-weight:650}.install-card__version{margin-left:auto;color:#8c9da9;font-size:12px}.install-card__arrow{margin-left:12px;color:#86a0b1}.install-card__body{padding:14px 20px 18px;border-top:1px solid #e4ebef;background:#fff;color:#748794}.install-card__body p{margin:0 0 12px}.component-current{display:flex;justify-content:space-between;align-items:center;padding:8px 10px;background:#f3f7fa;border-radius:4px}.install-actions{display:flex;gap:9px;align-items:center;margin-top:12px}.install-actions .q-input{width:220px}.pcl-hint{margin-top:18px;border:1px solid #d7e6ef;background:#f2f8fc!important;color:#698296}.export-form{display:flex;flex-direction:column;gap:13px;max-width:850px;margin-top:20px}.export-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:9px}.export-option{display:flex;align-items:flex-start;gap:5px;padding:11px;border:1px solid #dbe5eb;border-radius:5px;background:#fbfcfd;cursor:pointer}.export-option span{display:flex;flex-direction:column;gap:3px}.export-option strong{color:#4a687f;font-size:13px}.export-option small{color:#91a0aa;font-size:11px}.export-advanced{border:1px solid #dbe5eb!important;background:#f8fafb!important}.export-advanced__title{display:flex;align-items:center;gap:20px;flex-wrap:wrap;color:#4b6a82;font-weight:650}.export-advanced p{margin:8px 0 0;color:#8b9ba7;font-size:12px}.setting-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px 18px;margin-top:20px}.setting-line{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:13px;border:1px solid #dbe5eb;border-radius:5px;background:#fbfcfd}.setting-line strong,.setting-line small{display:block}.setting-line strong{color:#4e6b80;font-size:13px}.setting-line small{margin-top:4px;color:#96a3ad;font-size:11px}.setting-line .q-input{width:150px}.server-empty{text-align:center;padding:75px 20px;color:#8898a4}.server-empty .material-icons{font-size:48px;color:#79a6c1}.server-empty h3{margin:10px 0 5px;color:#496a82}.server-empty p{margin:0 0 18px}@media(max-width:700px){.setting-grid,.export-grid{grid-template-columns:1fr}.install-actions{flex-wrap:wrap}.install-actions .q-input{width:100%}}
.java-layout{margin-top:18px}.java-summary{display:flex;align-items:center;gap:10px;padding:14px;border:1px solid #dce7ec;border-radius:6px;background:#f7fafb}.java-summary>.material-icons{font-size:28px;color:#6598b7}.java-summary>div{display:flex;flex-direction:column;flex:1}.java-summary small{margin-top:3px;color:#8e9faa;font-size:11px}.memory-card{margin-top:20px;padding:22px;border:1px solid #dbe6eb;border-radius:6px;background:#f8fbfc}.memory-values{display:grid;grid-template-columns:auto 1fr auto 1fr;align-items:end;gap:8px 12px}.memory-values strong{color:#4e7998;font-size:22px}.memory-values span{color:#8d9da7;font-size:11px}.memory-range{width:100%;margin:24px 0 12px;accent-color:#4d91bd}.memory-bar{height:7px;overflow:hidden;border-radius:4px;background:#dce8ee}.memory-bar span{display:block;height:100%;background:linear-gradient(90deg,#68a8cd,#3d83b7);transition:width .25s}.memory-card p{margin:14px 0 0;color:#8a9aa5;font-size:11px}.advanced-form{display:flex;flex-direction:column;gap:14px;margin-top:18px}.advanced-form label{display:flex;align-items:center;gap:14px;color:#627a8c;font-size:12px}.advanced-form input,.advanced-form select,.advanced-form textarea{flex:1;min-width:0;padding:9px 10px;border:1px solid #cbdbe5;border-radius:5px;background:#fff;color:#4f687b;font:inherit}.advanced-form textarea{resize:vertical}.advanced-checks{display:flex;flex-wrap:wrap;gap:8px 20px;padding-top:6px;border-top:1px solid #e1e8ed}@media(max-width:700px){.memory-values{grid-template-columns:1fr 1fr}.advanced-form label{display:block}.advanced-form label input,.advanced-form label select,.advanced-form label textarea{display:block;width:100%;margin-top:5px}}
.server-actions{display:flex;gap:8px;flex-wrap:wrap}.server-grid{display:grid;grid-template-columns:.8fr 1.2fr;gap:14px;margin-top:18px}.server-card,.server-console{border-color:#dbe5eb!important;background:#fbfcfd!important}.server-card-title{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px;color:#4a687f;font-weight:650}.pcl-muted{color:#8a9aa5;font-size:11px}.server-properties{display:grid;grid-template-columns:1fr 1fr;gap:10px}.server-properties label{display:flex;flex-direction:column;gap:5px;color:#637b8d;font-size:11px}.server-properties input,.server-properties select,.console-command input{height:34px;padding:0 9px;border:1px solid #cbdbe5;border-radius:5px;background:#fff;color:#4f687b}.server-console{margin-top:14px}.server-console pre{min-height:150px;max-height:300px;margin:0;padding:12px;overflow:auto;border-radius:5px;background:#101923;color:#c7d4df;font:12px/1.55 Consolas,monospace;white-space:pre-wrap}.console-command{display:flex;gap:8px;margin-top:10px}.console-command input{flex:1}@media(max-width:700px){.server-grid{grid-template-columns:1fr}.server-properties{grid-template-columns:1fr}}
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
  font-size: 19px;
}

.pcl-install-head p {
  margin: 0;
  color: var(--pcl-muted);
  font-size: 11px;
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
  font-size: 12px;
  font-weight: 600;
}

.pcl-install-item__version {
  margin-left: auto;
  color: var(--pcl-muted);
  font-size: 11px;
}

.pcl-install-item__body {
  padding: 10px 12px 13px;
  border-top: 1px solid var(--pcl-border);
  background: var(--pcl-c8);
}

.pcl-install-item__body p {
  margin: 0 0 9px;
  color: var(--pcl-muted);
  font-size: 11px;
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
  font-size: 11px;
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
</style>
