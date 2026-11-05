<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef-instance">
    <section v-if="instance" class="ef-instance__hero">
      <div class="ef-instance__icon">
        <img v-if="icon" :src="icon" alt="" />
        <span v-else class="material-icons">inventory_2</span>
      </div>
      <div class="ef-instance__copy">
        <p class="ef-eyebrow">// INSTANCE</p>
        <h2>{{ instance.name }}</h2>
        <p>{{ instance.version || instance.runtime?.minecraft || 'Minecraft' }}<span v-if="instance.group"> | {{ instance.group }}</span></p>
      </div>
      <div class="ef-instance__actions">
        <EfButton variant="accent" icon="play_arrow" :disabled="busy || instance.running" @click="emit('launch')">启动</EfButton>
        <EfButton icon="folder_open" :disabled="busy" @click="openFolder">文件夹</EfButton>
        <EfButton icon="refresh" :disabled="busy" @click="reloadAll">刷新</EfButton>
      </div>
    </section>

    <nav class="ef-tabs">
      <button v-for="tab in visibleTabs" :key="tab.id" type="button" :class="{ current: section === tab.id }" @click="section = tab.id">
        <small>// {{ tab.code }}</small>{{ tab.label }}
      </button>
    </nav>

    <section v-if="section === 'overview'">
      <div class="ef-instance__stats">
        <EfStat label="组件" :value="components.length" note="已安装" />
        <EfStat label="资源" :value="resources.length" note="当前类型" />
        <EfStat label="状态" :value="instance.running ? '运行' : '空闲'" :alert="instance.running" />
      </div>
      <EfCard title="实例信息">
        <div class="ef-info-grid">
          <div><span>实例目录</span><code>{{ details?.root || details?.instanceRoot || '—' }}</code></div>
          <div><span>游戏目录</span><code>{{ details?.gameRoot || details?.gameDir || '—' }}</code></div>
          <div><span>Minecraft</span><code>{{ details?.minecraftVersion || instance.version || '—' }}</code></div>
          <div><span>加载器</span><code>{{ loaderText }}</code></div>
        </div>
      </EfCard>
      <EfCard title="备注">
        <EfInput v-model="notes" multiline :rows="4" placeholder="记录这个实例的用途、模组与版本" />
        <div class="ef-instance__note-actions">
          <EfButton size="sm" variant="accent" :disabled="busy" @click="saveNotes">保存备注</EfButton>
        </div>
      </EfCard>
      <EfCard title="实例操作">
        <div class="ef-instance__ops">
          <EfButton icon="fact_check" :disabled="busy" @click="runInstance('verify')">校验文件</EfButton>
          <EfButton icon="stop_circle" :disabled="busy || !instance.running" @click="runInstance('stop')">停止</EfButton>
          <EfButton icon="content_copy" :disabled="busy" @click="promptCopy">复制实例</EfButton>
          <EfButton icon="file_archive" :disabled="busy" @click="exportInstance">导出实例</EfButton>
          <EfButton icon="create_new_folder" :disabled="busy" @click="createShortcut">创建快捷方式</EfButton>
          <EfButton variant="red" icon="delete" :disabled="busy" @click="removeInstance">删除实例</EfButton>
        </div>
      </EfCard>
    </section>

    <section v-else-if="section === 'components'">
      <div class="ef-section-bar">
        <span>{{ components.length }} 个组件</span>
        <EfButton size="sm" icon="download" :disabled="busy" @click="openResourceDownload('mods')">安装资源</EfButton>
      </div>
      <div v-if="components.length" class="ef-rows">
        <article v-for="component in components" :key="component.id" class="ef-row">
          <div class="ef-row__main">
            <strong>{{ component.name || component.id }}</strong>
            <small>{{ component.version || '—' }} | {{ component.type || component.kind || '组件' }}</small>
          </div>
          <EfCheckbox :model-value="component.enabled !== false" :disabled="busy" @update:model-value="value => toggleComponent(component, value)">
            {{ component.enabled !== false ? '启用' : '禁用' }}
          </EfCheckbox>
          <EfButton size="sm" variant="ghost" :disabled="busy" @click="changeVersion(component)">换版本</EfButton>
        </article>
      </div>
      <EfEmpty v-else title="没有组件" description="这个实例还没有安装模组或加载器组件。" icon="extension" />
    </section>

    <section v-else-if="section === 'resources'">
      <div class="ef-section-bar">
        <EfSelect v-model="resourceKind" :options="resourceKinds" />
        <EfButton size="sm" icon="download" :disabled="busy" @click="openResourceDownload(resourceKind)">下载</EfButton>
      </div>
      <div v-if="resources.length" class="ef-rows">
        <article v-for="resource in resources" :key="resource.id || resource.fileName || resource.name" class="ef-row">
          <div class="ef-row__main">
            <strong>{{ resource.name || resource.fileName }}</strong>
            <small>{{ resource.version || resource.fileName || '—' }}</small>
          </div>
          <span class="ef-row__state">{{ resource.enabled === false ? '禁用' : '启用' }}</span>
          <EfButton size="sm" variant="red" :disabled="busy" @click="removeResource(resource)">删除</EfButton>
        </article>
      </div>
      <EfEmpty v-else title="没有资源" description="当前类型下没有已安装文件。" icon="folder_open" />
    </section>

    <section v-else-if="section === 'logs'">
      <div class="ef-section-bar">
        <EfSelect v-model="logFile" :options="logOptions" placeholder="选择日志文件" />
        <EfButton size="sm" icon="refresh" :disabled="busy || !logFile" @click="readLog">读取</EfButton>
      </div>
      <EfCard title="日志内容">
        <pre class="ef-log">{{ fileLog || consoleLines.slice(-200).join('\n') || '暂无日志。' }}</pre>
      </EfCard>
    </section>

    <section v-else-if="section === 'servers'">
      <div class="ef-section-bar">
        <span>{{ servers.length }} 个服务器</span>
        <EfButton size="sm" icon="refresh" :disabled="busy" @click="loadServers">读取</EfButton>
        <EfButton size="sm" variant="accent" icon="save" :disabled="busy" @click="saveServers">保存</EfButton>
      </div>
      <div class="ef-server-form">
        <EfInput v-model="serverForm.name" placeholder="名称" />
        <EfInput v-model="serverForm.ip" placeholder="地址" />
        <EfButton icon="add" :disabled="busy || !serverForm.ip.trim()" @click="addServer">添加</EfButton>
      </div>
      <div v-if="servers.length" class="ef-rows">
        <article v-for="(server, index) in servers" :key="`${server.ip}-${index}`" class="ef-row">
          <div class="ef-row__main">
            <strong>{{ server.name || server.ip }}</strong>
            <small>{{ server.ip }}</small>
          </div>
          <span class="ef-row__state">{{ serverPing[server.ip] || '未测试' }}</span>
          <EfButton size="sm" variant="ghost" :disabled="busy" @click="pingServer(server)">测试</EfButton>
          <EfButton size="sm" variant="red" :disabled="busy" @click="removeServer(index)">删除</EfButton>
        </article>
      </div>
      <EfEmpty v-else title="没有服务器" description="添加一个服务器地址，保存到 servers.dat。" icon="dns" />
    </section>

    <section v-else-if="section === 'settings'">
      <EfSettingsList :settings="instanceSettings" :busy="busy" scope="instance" @save="saveSetting" @reset="resetSetting" />
    </section>

    <section v-else-if="section === 'provenance'">
      <EfCard title="来源与修改痕迹">
        <p class="ef-provenance-copy">{{ provenance?.mixed ? '检测到多个启动器或目录格式的修改痕迹。' : '以下结果来自目录结构和版本 JSON 证据。' }}</p>
        <div v-for="provider in (provenance?.providers || [])" :key="provider.providerId" class="ef-row">
          <div class="ef-row__main"><strong>{{ provider.displayName || provider.providerId }}</strong><small>{{ [...(provider.matchedPaths || []), ...(provider.matchedFields || [])].join(', ') || '标准版本 JSON' }}</small></div>
          <span class="ef-row__state">{{ provider.confidence }}%</span>
        </div>
      </EfCard>
    </section>
  </div>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue';
import EfButton from '../components/EfButton.vue';
import EfCard from '../components/EfCard.vue';
import EfCheckbox from '../components/EfCheckbox.vue';
import EfEmpty from '../components/feedback/EfEmpty.vue';
import EfInput from '../components/EfInput.vue';
import EfSelect from '../components/EfSelect.vue';
import EfStat from '../components/data/EfStat.vue';
import EfSettingsList from '../components/data/EfSettingsList.vue';
import { launcher } from '../../../boot/launcher-api';
import { onResourceChanged } from '../../../common/runtime/resource-events';

const props = defineProps<{
  instance: any;
  busy: boolean;
  execute: (op: string, p?: Record<string, unknown>) => Promise<any>;
  consoleLines?: string[];
  icon?: string;
}>();
const emit = defineEmits<{
  launch: [];
  folder: [];
  changed: [];
  toast: [type: 'success' | 'error' | 'info', title: string, message?: string];
}>();

const tabs = [
  { id: 'overview', label: '概览', code: '01' },
  { id: 'components', label: '组件', code: '02' },
  { id: 'resources', label: '资源', code: '03' },
  { id: 'logs', label: '日志', code: '04' },
  { id: 'servers', label: '服务器', code: '05' },
  { id: 'settings', label: '设置', code: '06' },
  { id: 'provenance', label: '来源痕迹', code: '07' },
];
const visibleTabs = computed(() => tabs.filter(tab =>
  tab.id !== 'provenance' ||
  props.instance?.format === 'vanilla-compatible' ||
  props.instance?.vanillaCompatible,
));
const section = ref('overview');
const details = ref<any>(null);
const notes = ref('');
const components = ref<any[]>([]);
const resources = ref<any[]>([]);
const resourceKind = ref('mods');
const logFiles = ref<any[]>([]);
const logFile = ref('');
const fileLog = ref('');
const instanceSettings = ref<any[]>([]);
const servers = ref<any[]>([]);
const serverRevision = ref('');
const serverForm = ref({ name: '', ip: '' });
const serverPing = ref<Record<string, string>>({});
const provenance = computed(() => details.value?.provenance || props.instance?.provenance || {});

const resourceKinds = [
  { label: '模组', value: 'mods' },
  { label: '资源包', value: 'resourcepacks' },
  { label: '光影', value: 'shaderpacks' },
  { label: '世界', value: 'saves' },
  { label: '数据包', value: 'datapacks' },
];
const logOptions = computed(() => logFiles.value.map((file: any) => ({
  label: String(file.name || file.path || file),
  value: String(file.name || file.path || file),
})));
const loaderText = computed(() => {
  const value = details.value?.loader || details.value?.modLoader || props.instance?.loader;
  if (!value) return '原版';
  return `${value}${details.value?.loaderVersion ? ` ${details.value.loaderVersion}` : ''}`;
});

async function reloadAll() {
  await Promise.all([loadInfo(), loadComponents(), loadResources(), loadLogs(), loadServers(), loadSettings()]);
}

async function loadInfo() {
  if (!props.instance?.id) return;
  try {
    details.value = await props.execute('instance.info', { instance: props.instance.id });
    notes.value = details.value?.notes || props.instance.notes || '';
  } catch { /* shell toast */ }
}
async function loadComponents() {
  if (!props.instance?.id) return;
  try {
    const result: any = await props.execute('instance.components.list', { instance: props.instance.id });
    components.value = Array.isArray(result) ? result : (result?.components || []);
  } catch { components.value = []; }
}
async function loadResources() {
  if (!props.instance?.id) return;
  try {
    const result: any = await props.execute('resource.list', { instance: props.instance.id, kind: resourceKind.value, includeIcons: false });
    resources.value = Array.isArray(result) ? result : [];
  } catch { resources.value = []; }
}
async function loadLogs() {
  if (!props.instance?.id) return;
  try {
    const result: any = await props.execute('instance.log.list', { instance: props.instance.id });
    logFiles.value = Array.isArray(result) ? result : (result?.files || []);
    if (!logFile.value && logFiles.value.length) logFile.value = String(logFiles.value[0]?.name || logFiles.value[0]?.path || logFiles.value[0]);
  } catch { logFiles.value = []; }
}
async function loadSettings() {
  if (!props.instance?.id) return;
  try {
    const result: any = await props.execute('settings.list', { scope: 'instance', instance: props.instance.id });
    instanceSettings.value = Array.isArray(result) ? result : (result?.entries || []);
  } catch { instanceSettings.value = []; }
}

async function loadServers() {
  if (!props.instance?.id) return;
  try {
    const result: any = await props.execute('instance.servers.read', { instance: props.instance.id });
    servers.value = Array.isArray(result?.servers) ? result.servers : [];
    serverRevision.value = String(result?.revision || '');
  } catch { servers.value = []; }
}
function addServer() {
  const ip = serverForm.value.ip.trim();
  if (!ip) return;
  servers.value.push({ name: serverForm.value.name.trim() || ip, ip, icon: '', acceptTextures: false });
  serverForm.value = { name: '', ip: '' };
}
function removeServer(index: number) {
  servers.value.splice(index, 1);
}
async function saveServers() {
  try {
    const result: any = await props.execute('instance.servers.write', {
      instance: props.instance.id,
      servers: servers.value,
      ifRevision: serverRevision.value,
    });
    serverRevision.value = String(result?.revision || serverRevision.value);
    emit('toast', 'success', '服务器列表已保存');
  } catch { /* shell toast */ }
}
async function pingServer(server: any) {
  serverPing.value = { ...serverPing.value, [server.ip]: '测试中…' };
  try {
    const result: any = await props.execute('server.ping', { address: server.ip });
    const online = Number(result?.players?.online ?? result?.onlinePlayers ?? 0);
    const max = Number(result?.players?.max ?? 0);
    const latency = Number(result?.latencyMs ?? 0);
    serverPing.value = { ...serverPing.value, [server.ip]: `${max > 0 ? `${online}/${max}` : online} | ${latency} ms` };
  } catch (error: any) {
    serverPing.value = { ...serverPing.value, [server.ip]: error?.message || '离线' };
  }
}

watch(() => props.instance?.id, () => { void reloadAll(); }, { immediate: true });
watch(resourceKind, () => { void loadResources(); });
const stopResourceEvents = onResourceChanged(payload => {
  if (!props.instance?.id || !payload.instanceId || String(payload.instanceId) === String(props.instance.id)) {
    void loadResources();
    if (payload.kind === 'modpacks') void loadComponents();
  }
});
onBeforeUnmount(stopResourceEvents);

async function readLog() {
  if (!props.instance?.id || !logFile.value) return;
  try {
    const result: any = await props.execute('instance.log.read', { instance: props.instance.id, file: logFile.value, maxBytes: 1048576 });
    fileLog.value = String(result?.content || '') + (result?.truncated ? '\n[日志已截断]' : '');
  } catch { /* shell toast */ }
}
async function saveNotes() {
  try {
    await props.execute('instance.set-notes', { instance: props.instance.id, notes: notes.value });
    emit('toast', 'success', '备注已保存');
    emit('changed');
  } catch { /* shell toast */ }
}
async function openFolder() {
  try { await props.execute('instance.open-folder', { instance: props.instance.id }); } catch { /* shell toast */ }
}
async function runInstance(op: string) {
  try {
    await props.execute(`instance.${op}`, { instance: props.instance.id });
    emit('toast', 'success', op === 'verify' ? '校验完成' : '操作完成');
    emit('changed');
  } catch { /* shell toast */ }
}
async function promptCopy() {
  const name = window.prompt('新实例名称', `${props.instance.name} - 副本`);
  if (!name) return;
  try {
    await props.execute('instance.copy', { instance: props.instance.id, name });
    emit('toast', 'success', '实例已复制');
    emit('changed');
  } catch { /* shell toast */ }
}
async function createShortcut() {
  try {
    await props.execute('instance.create-shortcut', { instance: props.instance.id, target: 'desktop' });
    emit('toast', 'success', '快捷方式已创建');
  } catch { /* shell toast */ }
}
async function exportInstance() {
  const output = window.prompt('导出到哪个文件？', `${props.instance.name}.zip`);
  if (!output) return;
  try {
    let task: any = await props.execute('neo.task.start', {
      operation: 'instance.export',
      priority: 'high',
      parameters: { instance: props.instance.id, output, format: 'zip', overwrite: true },
    });
    while (task && !task.finished) {
      await new Promise(resolve => setTimeout(resolve, 400));
      task = await props.execute('neo.task.status', { taskId: task.taskId });
    }
    if (task?.state !== 'succeeded') throw new Error(task?.error || '导出失败');
    emit('toast', 'success', '实例已导出', output);
  } catch (error: any) {
    emit('toast', 'error', '导出失败', error?.message || String(error));
  }
}
async function removeInstance() {
  if (!window.confirm(`删除实例“${props.instance.name}”？此操作不可撤销。`)) return;
  try {
    await props.execute('instance.delete', { instance: props.instance.id, confirm: true });
    emit('toast', 'success', '实例已删除');
    emit('changed');
  } catch { /* shell toast */ }
}
async function toggleComponent(component: any, enabled: boolean) {
  try {
    await props.execute('instance.component.set-enabled', { instance: props.instance.id, component: component.id, enabled });
    component.enabled = enabled;
  } catch { /* shell toast */ }
}
async function changeVersion(component: any) {
  const version = window.prompt(`“${component.name || component.id}”的新版本`, component.version || '');
  if (!version) return;
  try {
    await props.execute('instance.component.set-version', { instance: props.instance.id, component: component.id, version });
    emit('toast', 'success', '版本已更新');
    await loadComponents();
  } catch { /* shell toast */ }
}
async function removeResource(resource: any) {
  if (!window.confirm(`删除“${resource.name || resource.fileName}”？`)) return;
  try {
    await props.execute('resource.remove', { instance: props.instance.id, kind: resourceKind.value, resource: resource.id || resource.fileName || resource.name, confirm: true });
    emit('toast', 'success', '已删除');
    await loadResources();
  } catch { /* shell toast */ }
}
async function openResourceDownload(kind: string) {
  try { await launcher.openResourceDownload(props.instance.id, kind, props.instance.name); } catch { /* shell toast */ }
}
async function saveSetting(key: string, value: unknown) {
  try {
    await props.execute('settings.set', { scope: 'instance', instance: props.instance.id, key, value });
    emit('toast', 'success', '设置已保存', key);
    await loadSettings();
  } catch { /* shell toast */ }
}
async function resetSetting(key: string) {
  try {
    await props.execute('settings.set', { scope: 'instance', instance: props.instance.id, key, value: null });
    emit('toast', 'success', '设置已重置', key);
    await loadSettings();
  } catch { /* shell toast */ }
}
</script>

<style scoped>
.ef-instance { width: 100%; display: flex; flex-direction: column; gap: 12px; }
.ef-instance__hero {
  display: flex;
  align-items: center;
  gap: 12px;
  padding-bottom: 12px;
  border-bottom: 1px solid var(--ef-rule);
}
.ef-instance__icon {
  flex: 0 0 58px;
  width: 58px;
  height: 58px;
  display: grid;
  place-items: center;
  border: 1px solid var(--ef-rule);
  background: var(--ef-chip);
}
.ef-instance__icon img { width: 100%; height: 100%; object-fit: cover; }
.ef-instance__icon .material-icons { color: var(--ef-ink-2); }
.ef-instance__copy { flex: 1; min-width: 0; }
.ef-eyebrow {
  margin: 0 0 2px;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 8px;
  letter-spacing: .16em;
}
.ef-instance__copy h2 {
  margin: 0;
  overflow: hidden;
  font-family: var(--ef-font-ui);
  font-size: 19px;
  font-weight: 800;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-instance__copy p:last-child { margin: 3px 0 0; color: var(--ef-ink-2); font-size: 11px; }
.ef-instance__actions { display: flex; gap: 6px; flex-wrap: wrap; justify-content: flex-end; }
.ef-tabs {
  display: flex;
  border-bottom: 1px solid var(--ef-ink);
}
.ef-tabs button {
  display: flex;
  align-items: baseline;
  gap: 5px;
  padding: 6px 12px;
  border: 0;
  border-right: 1px solid var(--ef-rule);
  background: transparent;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 11px;
  font-weight: 600;
  cursor: pointer;
}
.ef-tabs button small { font-size: 8px; letter-spacing: .1em; color: var(--ef-ink-3, #8a8a86); }
.ef-tabs button:hover { background: var(--ef-chip); color: var(--ef-ink); }
.ef-tabs button.current { background: var(--ef-signal); color: var(--ef-on-signal); }
.ef-tabs button.current small { color: #4a4700; }
.ef-instance__stats { display: flex; gap: 8px; margin-bottom: 10px; overflow: auto; }
.ef-info-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(230px, 1fr)); gap: 8px; }
.ef-info-grid div { padding-left: 8px; border-left: 2px solid var(--ef-rule); }
.ef-info-grid span {
  display: block;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 8px;
  letter-spacing: .12em;
  text-transform: uppercase;
}
.ef-info-grid code {
  display: block;
  margin-top: 2px;
  overflow: hidden;
  color: var(--ef-ink);
  font-family: var(--ef-font-mono);
  font-size: 10px;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-instance__note-actions { display: flex; justify-content: flex-end; margin-top: 8px; }
.ef-instance__ops { display: flex; gap: 6px; flex-wrap: wrap; }
.ef-section-bar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  margin-bottom: 10px;
}
.ef-section-bar > span {
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .12em;
  text-transform: uppercase;
}
.ef-server-form { display: flex; gap: 6px; align-items: center; margin-bottom: 10px; }
.ef-server-form > :nth-child(1) { width: 180px; }
.ef-server-form > :nth-child(2) { flex: 1; }
.ef-rows { display: flex; flex-direction: column; }
.ef-row {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 8px 0;
  border-bottom: 1px solid var(--ef-rule);
}
.ef-row__main { flex: 1; min-width: 0; }
.ef-row__main strong {
  display: block;
  overflow: hidden;
  font-family: var(--ef-font-ui);
  font-size: 12px;
  font-weight: 700;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-row__main small {
  display: block;
  margin-top: 1px;
  color: var(--ef-ink-2);
  font-size: 9px;
}
.ef-row__state {
  padding: 1px 6px;
  border: 1px solid var(--ef-rule);
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 8px;
  text-transform: uppercase;
}
.ef-log {
  margin: 0;
  max-height: 52vh;
  overflow: auto;
  font-family: var(--ef-font-mono);
  font-size: 10px;
  line-height: 1.55;
  color: var(--ef-ink-2);
  white-space: pre-wrap;
  word-break: break-word;
}
@media (max-width: 760px) {
  .ef-instance__hero { flex-wrap: wrap; }
  .ef-instance__actions { width: 100%; justify-content: flex-start; }
}
</style>
