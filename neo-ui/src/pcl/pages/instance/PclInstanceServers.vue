<template>
  <div class="pcl-instance-servers">
    <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

    <PclCard title="多人服务器">
      <template #actions>
        <PclButton size="sm" icon="refresh-cw" :disabled="busy || loading" @click="load">刷新</PclButton>
      </template>
      <div class="pcl-server-form">
        <PclInput v-model="form.name" placeholder="服务器名称" />
        <PclInput v-model="form.ip" placeholder="地址，例如 play.example.com:25565" @enter="addOrUpdate" />
        <PclButton variant="highlight" icon="plus" :disabled="busy || !form.ip.trim()" @click="addOrUpdate">
          {{ editingIndex === null ? '添加' : '更新' }}
        </PclButton>
      </div>
      <div class="pcl-server-form pcl-server-form--options">
        <PclCheckbox v-model="form.acceptTextures" label="接受服务器资源包" />
        <PclButton v-if="editingIndex !== null" size="sm" variant="ghost" @click="resetForm">取消编辑</PclButton>
      </div>

      <div class="pcl-server-list">
        <PclListItem
          v-for="(server, index) in servers"
          :key="`${server.ip}-${index}`"
          :title="server.name || server.ip"
          :info="serverInfo(server)"
          @click="editServer(server, index)"
        >
          <template #icon>
            <img v-if="server.icon" :src="server.icon" class="pcl-server-icon" alt="" />
            <PclIcon v-else name="server" :size="18" />
          </template>
          <template #actions>
            <span v-if="pingState[server.ip]?.text" class="pcl-server-ping" :class="{ bad: pingState[server.ip]?.bad }">{{ pingState[server.ip].text }}</span>
            <PclIconButton icon="gauge" label="测试连接" :size="24" :disabled="busy || pingState[server.ip]?.loading" @click.stop="pingServer(server)" />
            <PclIconButton icon="pencil" label="编辑" :size="24" :disabled="busy" @click.stop="editServer(server, index)" />
            <PclIconButton icon="trash-2" label="删除" :size="24" danger :disabled="busy" @click.stop="removeServer(index)" />
          </template>
        </PclListItem>
        <div v-if="!servers.length" class="pcl-empty pcl-empty--compact">servers.dat 里还没有服务器。</div>
      </div>

      <div class="pcl-actions">
        <PclButton variant="highlight" icon="save" :disabled="busy || loading" @click="save">保存名单</PclButton>
        <span class="pcl-server-path">{{ path || 'servers.dat' }}</span>
      </div>
      <PclHint theme="blue">保存会原子替换实例游戏目录下的 <code>servers.dat</code>，并带上读取时的 revision 防止覆盖外部修改。</PclHint>
    </PclCard>
  </div>
</template>

<script setup lang="ts">
import { onMounted, reactive, ref } from 'vue';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclListItem from '../../components/PclListItem.vue';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ instance: any; busy: boolean; execute: Execute }>();

interface ServerEntry { name: string; ip: string; icon: string; acceptTextures: boolean }
const servers = ref<ServerEntry[]>([]);
const form = ref<ServerEntry>({ name: '', ip: '', icon: '', acceptTextures: false });
const editingIndex = ref<number | null>(null);
const revision = ref('');
const path = ref('');
const loading = ref(false);
const pingState = reactive<Record<string, { loading: boolean; text: string; bad: boolean }>>({});
function serverInfo(server: ServerEntry) {
  const ping = pingState[server.ip];
  return ping?.text ? `${server.ip} · ${ping.text}` : server.ip;
}
async function pingServer(server: ServerEntry) {
  const key = server.ip;
  pingState[key] = { loading: true, text: '测试中…', bad: false };
  try {
    const result = await props.execute('server.ping', { address: key });
    const players = result?.players || {};
    const online = Number(players.online ?? result?.onlinePlayers ?? 0);
    const max = Number(players.max ?? 0);
    const latency = Number(result?.latencyMs ?? 0);
    pingState[key] = {
      loading: false,
      text: `${max > 0 ? `${online}/${max} · ` : `${online} 人在线 · `}${latency} ms`,
      bad: false,
    };
  } catch (exception) {
    pingState[key] = { loading: false, text: exception instanceof Error ? exception.message : String(exception), bad: true };
  }
}

const error = ref('');

function message(value: unknown) {
  return value instanceof Error ? value.message : String(value);
}

async function load() {
  loading.value = true;
  error.value = '';
  try {
    const result = await props.execute('instance.servers.read', { instance: props.instance.id });
    servers.value = Array.isArray(result?.servers) ? result.servers.map((server: any) => ({
      name: String(server.name || server.ip || ''),
      ip: String(server.ip || ''),
      icon: String(server.icon || ''),
      acceptTextures: Boolean(server.acceptTextures),
    })) : [];
    revision.value = String(result?.revision || '');
    path.value = String(result?.path || '');
  } catch (exception) {
    error.value = message(exception);
  } finally {
    loading.value = false;
  }
}

function resetForm() {
  form.value = { name: '', ip: '', icon: '', acceptTextures: false };
  editingIndex.value = null;
}

function editServer(server: ServerEntry, index: number) {
  form.value = { ...server };
  editingIndex.value = index;
}

function addOrUpdate() {
  const ip = form.value.ip.trim();
  if (!ip) return;
  const entry: ServerEntry = {
    name: form.value.name.trim() || ip,
    ip,
    icon: form.value.icon,
    acceptTextures: form.value.acceptTextures,
  };
  if (editingIndex.value === null) servers.value.push(entry);
  else servers.value.splice(editingIndex.value, 1, entry);
  resetForm();
}

function removeServer(index: number) {
  servers.value.splice(index, 1);
  if (editingIndex.value === index) resetForm();
}

async function save() {
  error.value = '';
  loading.value = true;
  try {
    const result = await props.execute('instance.servers.write', {
      instance: props.instance.id,
      servers: servers.value,
      ifRevision: revision.value,
    });
    revision.value = String(result?.revision || revision.value);
  } catch (exception) {
    error.value = message(exception);
  } finally {
    loading.value = false;
  }
}

onMounted(() => { void load(); });
</script>

<style scoped>
.pcl-server-form {
  display: flex;
  gap: 7px;
  margin-bottom: 8px;
}

.pcl-server-form :deep(.pcl-input) {
  flex: 1;
}

.pcl-server-form--options {
  align-items: center;
  justify-content: space-between;
}

.pcl-server-list {
  max-height: 360px;
  margin-bottom: 10px;
  overflow-y: auto;
}

.pcl-server-icon {
  width: 32px;
  height: 32px;
  border-radius: 3px;
  object-fit: cover;
  image-rendering: pixelated;
}

.pcl-server-path {
  min-width: 0;
  overflow: hidden;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-server-ping {
  max-width: 190px;
  overflow: hidden;
  color: var(--pcl-c3);
  font-size: calc(9px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-server-ping.bad { color: #fb7185; }
</style>
