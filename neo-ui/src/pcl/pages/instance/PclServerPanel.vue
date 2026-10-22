<template>
  <div class="pcl-server-panel">
    <PclCard title="服务端">
      <template #actions>
        <span class="pcl-server-state">{{ consoleState }}</span>
      </template>
      <div class="pcl-server-head">
        <div>
          <strong>{{ instance.name }}</strong>
          <span>{{ instance.running ? '服务端正在运行' : '服务端未运行' }}</span>
        </div>
        <div class="pcl-actions">
          <PclButton variant="highlight" icon="play" :disabled="busy || instance.running || !eula" @click="startServer">启动服务端</PclButton>
          <PclButton variant="red" icon="power" :disabled="busy || !instance.running" @click="stopServer">停止</PclButton>
          <PclButton icon="monitor-cog" :disabled="busy" @click="emit('console')">连接控制台</PclButton>
        </div>
      </div>
      <div class="pcl-server-tabs">
        <button v-for="item in tabs" :key="item.id" type="button" :class="{ active: tab === item.id }" @click="tab = item.id">{{ item.label }}</button>
      </div>
    </PclCard>

    <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

    <PclCard v-if="tab === 'console'" title="服务端控制台">
      <template #actions>
        <PclButton size="sm" variant="ghost" icon="trash-2" :disabled="!consoleLines.length" @click="emit('clear-console')">清空</PclButton>
      </template>
      <pre class="pcl-server-console">{{ consoleLines.join('\n') || '服务端尚未启动，控制台输出会显示在这里。' }}</pre>
      <div class="pcl-server-command">
        <PclInput v-model="command" placeholder="输入服务端命令，例如 list" @enter="sendCommand" />
        <PclButton variant="highlight" icon="arrow-right" :disabled="busy || !command.trim() || !instance.running" @click="sendCommand">发送</PclButton>
      </div>
    </PclCard>

    <PclCard v-else-if="tab === 'properties'" title="server.properties">
      <template #actions>
        <PclButton size="sm" variant="highlight" icon="save" :disabled="busy || loading" @click="saveProperties">保存</PclButton>
      </template>
      <PclInput v-model="propertiesText" multiline :rows="16" placeholder="server-port=25565" />
      <PclHint theme="blue">每行一个 <code>key=value</code>；保存会整体写回 server.properties。</PclHint>
    </PclCard>

    <PclCard v-else-if="tab === 'lists'" title="玩家名单">
      <div class="pcl-server-list-toolbar">
        <PclSelect v-model="listKind" :options="listOptions" @update:model-value="loadList" />
        <PclInput v-model="listInput" placeholder="玩家名或 IP" @enter="addListEntry" />
        <PclButton variant="highlight" icon="plus" :disabled="busy || !listInput.trim()" @click="addListEntry">添加</PclButton>
      </div>
      <div class="pcl-server-list">
        <PclListItem
          v-for="(entry, index) in listEntries"
          :key="entry.uuid || entry.name || entry.ip || index"
          :title="listEntryLabel(entry)"
          :info="listEntryInfo(entry)"
        >
          <template #icon><PclIcon name="user" :size="16" /></template>
          <template #actions>
            <PclIconButton icon="trash-2" label="移除" :size="24" danger :disabled="busy" @click="removeListEntry(entry)" />
          </template>
        </PclListItem>
        <div v-if="!listEntries.length" class="pcl-empty pcl-empty--compact">这个名单是空的。</div>
      </div>
    </PclCard>

    <PclCard v-else-if="tab === 'yaml'" title="服务端 YAML">
      <template #actions>
        <PclSelect v-model="yamlFile" :options="yamlOptions" @update:model-value="loadYaml" />
        <PclButton size="sm" variant="highlight" icon="save" :disabled="busy || loading" @click="saveYaml">保存</PclButton>
      </template>
      <PclInput v-model="yamlText" multiline :rows="16" placeholder="YAML 内容" />
      <PclHint theme="blue">只支持服务端根目录的 bukkit.yml 和 spigot.yml，保存时会校验 revision。</PclHint>
    </PclCard>

    <PclCard v-else title="EULA">
      <div class="pcl-server-eula">
        <PclCheckbox v-model="eula" label="我同意 Minecraft 服务端 EULA" :disabled="busy" @update:model-value="saveEula" />
        <PclHint theme="yellow">服务端必须写入 <code>eula=true</code> 后才能启动。</PclHint>
      </div>
    </PclCard>
  </div>
</template>

<script setup lang="ts">
import { onMounted, ref } from 'vue';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclSelect from '../../components/PclSelect.vue';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ instance: any; busy: boolean; execute: Execute; consoleLines: string[]; consoleState: string }>();
const emit = defineEmits<{ changed: []; console: []; 'clear-console': [] }>();

const tabs = [
  { id: 'console', label: '控制台' }, { id: 'properties', label: '属性' }, { id: 'lists', label: '名单' },
  { id: 'yaml', label: 'YAML' }, { id: 'eula', label: 'EULA' },
];
const tab = ref('console'), error = ref(''), loading = ref(false), command = ref('');
const eula = ref(false);
const propertiesText = ref('');
const listKind = ref('whitelist'), listInput = ref(''), listEntries = ref<any[]>([]);
const yamlFile = ref('bukkit.yml'), yamlText = ref(''), yamlRevision = ref('');
const listOptions = [
  { label: '白名单', value: 'whitelist' }, { label: 'OP', value: 'ops' },
  { label: '封禁玩家', value: 'banned-players' }, { label: '封禁 IP', value: 'banned-ips' },
];
const yamlOptions = [{ label: 'bukkit.yml', value: 'bukkit.yml' }, { label: 'spigot.yml', value: 'spigot.yml' }];

function message(value: unknown) {
  return value instanceof Error ? value.message : String(value);
}
function setError(value: unknown) {
  error.value = message(value);
}
function listEntryLabel(entry: any) {
  return String(entry?.name || entry?.uuid || entry?.ip || JSON.stringify(entry));
}
function listEntryInfo(entry: any) {
  return [entry?.uuid, entry?.level ? `等级 ${entry.level}` : '', entry?.created ? `创建 ${entry.created}` : ''].filter(Boolean).join(' · ') || '服务端名单条目';
}
async function loadEula() {
  const result = await props.execute('server.eula.read', { instance: props.instance.id });
  eula.value = Boolean(result?.agreed);
}
async function loadProperties() {
  const result = await props.execute('server.properties.read', { instance: props.instance.id });
  const values = result?.properties || {};
  propertiesText.value = Object.keys(values).sort().map(key => `${key}=${values[key]}`).join('\n');
}
async function loadList() {
  const result = await props.execute('server.list.read', { instance: props.instance.id, kind: listKind.value });
  listEntries.value = Array.isArray(result?.entries) ? result.entries : [];
}
async function loadYaml() {
  const result = await props.execute('server.yaml.read', { instance: props.instance.id, file: yamlFile.value });
  yamlText.value = String(result?.content || '');
  yamlRevision.value = String(result?.revision || '');
}
async function loadAll() {
  loading.value = true;
  error.value = '';
  try {
    await loadEula();
    await loadProperties();
    await loadList();
    await loadYaml();
  } catch (exception) {
    setError(exception);
  } finally {
    loading.value = false;
  }
}
async function run(action: () => Promise<void>) {
  error.value = '';
  loading.value = true;
  try { await action(); emit('changed'); }
  catch (exception) { setError(exception); }
  finally { loading.value = false; }
}
function startServer() {
  return run(async () => { await props.execute('server.start', { instance: props.instance.id }); });
}
function stopServer() {
  return run(async () => { await props.execute('instance.stop', { instance: props.instance.id }); });
}
function sendCommand() {
  if (!command.value.trim()) return;
  return run(async () => {
    await props.execute('server.console.command', { instance: props.instance.id, command: command.value.trim() });
    command.value = '';
  });
}
function saveEula() {
  return run(async () => { await props.execute('server.eula.write', { instance: props.instance.id, agreed: eula.value }); });
}
function saveProperties() {
  return run(async () => {
    const properties: Record<string, string> = {};
    for (const line of propertiesText.value.split('\n')) {
      const trimmed = line.trim();
      if (!trimmed || trimmed.startsWith('#')) continue;
      const index = trimmed.indexOf('=');
      if (index <= 0) continue;
      properties[trimmed.slice(0, index).trim()] = trimmed.slice(index + 1).trim();
    }
    await props.execute('server.properties.write', { instance: props.instance.id, properties });
  });
}
function addListEntry() {
  if (!listInput.value.trim()) return;
  return run(async () => {
    await props.execute('server.list.add', { instance: props.instance.id, kind: listKind.value, entry: listInput.value.trim() });
    listInput.value = '';
    await loadList();
  });
}
function removeListEntry(entry: any) {
  return run(async () => {
    await props.execute('server.list.remove', { instance: props.instance.id, kind: listKind.value, entry: listEntryLabel(entry), confirm: true });
    await loadList();
  });
}
function saveYaml() {
  return run(async () => {
    const result = await props.execute('server.yaml.write', { instance: props.instance.id, file: yamlFile.value, content: yamlText.value, ifRevision: yamlRevision.value });
    yamlRevision.value = String(result?.revision || yamlRevision.value);
  });
}

onMounted(() => {
  if (props.instance?.running) emit('console');
  void loadAll();
});
</script>

<style scoped>
.pcl-server-panel {
  display: flex;
  flex-direction: column;
}

.pcl-server-state {
  color: var(--pcl-muted);
  font-size: 11px;
}

.pcl-server-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  margin-bottom: 12px;
}

.pcl-server-head strong {
  display: block;
  color: var(--pcl-text);
  font-size: 14px;
}

.pcl-server-head span {
  display: block;
  margin-top: 3px;
  color: var(--pcl-muted);
  font-size: 11px;
}

.pcl-server-tabs {
  display: flex;
  flex-wrap: wrap;
  gap: 2px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-server-tabs button {
  height: 30px;
  padding: 0 12px;
  border: 0;
  border-bottom: 2px solid transparent;
  background: transparent;
  color: var(--pcl-muted);
  font: 400 12px/1 var(--pcl-font);
  cursor: pointer;
}

.pcl-server-tabs button.active {
  border-bottom-color: var(--pcl-c3);
  color: var(--pcl-c2);
}

.pcl-server-console {
  min-height: 300px;
  max-height: 52vh;
  margin: 0 0 9px;
  overflow: auto;
  padding: 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c8);
  color: var(--pcl-text);
  white-space: pre-wrap;
  font: 11px/1.55 ui-monospace, Consolas, monospace;
}

.pcl-server-command,
.pcl-server-list-toolbar {
  display: flex;
  align-items: center;
  gap: 7px;
}

.pcl-server-list-toolbar {
  margin-bottom: 10px;
}

.pcl-server-list-toolbar :deep(.pcl-input) {
  flex: 1;
}

.pcl-server-list {
  max-height: 340px;
  overflow-y: auto;
}

.pcl-server-eula {
  display: flex;
  flex-direction: column;
  gap: 10px;
}
</style>
