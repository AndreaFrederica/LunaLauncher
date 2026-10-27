<template>
  <div class="pcl-instance-servers">
    <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

    <PclCard :title="t('server.title')">
      <template #actions>
        <PclButton size="sm" icon="refresh-cw" :disabled="busy || loading" @click="load">{{ t('server.refresh') }}</PclButton>
      </template>
      <div class="pcl-server-form">
        <PclInput v-model="form.name" :placeholder="t('server.name')" />
        <PclInput v-model="form.ip" :placeholder="t('server.address')" @enter="addOrUpdate" />
        <PclButton variant="highlight" icon="plus" :disabled="busy || !form.ip.trim()" @click="addOrUpdate">
          {{ editingIndex === null ? t('server.add') : t('server.update') }}
        </PclButton>
      </div>
      <div class="pcl-server-form pcl-server-form--options">
        <PclCheckbox v-model="form.acceptTextures" :label="t('server.acceptTextures')" />
        <PclButton v-if="editingIndex !== null" size="sm" variant="ghost" @click="resetForm">{{ t('server.cancelEdit') }}</PclButton>
      </div>

      <div class="pcl-server-list">
        <PclListItem
          v-for="(server, index) in servers"
          :key="`${server.ip}-${index}`"
          :title="server.name || server.ip"
          :info="serverInfo(server)"
          :tags="pingState[server.ip]?.version ? [pingState[server.ip].version] : []"
          @click="editServer(server, index)"
        >
          <template #icon>
            <img v-if="pingState[server.ip]?.icon || server.icon" :src="pingState[server.ip]?.icon || server.icon" class="pcl-server-icon" alt="" />
            <PclIcon v-else name="server" :size="18" />
          </template>
          <template #actions>
            <span v-if="pingState[server.ip]?.text" class="pcl-server-ping" :class="{ bad: pingState[server.ip]?.bad }">{{ pingState[server.ip].text }}</span>
            <PclIconButton icon="gauge" :label="t('server.test')" :size="24" :disabled="busy || pingState[server.ip]?.loading" @click.stop="pingServer(server)" />
            <PclIconButton icon="pencil" :label="t('server.edit')" :size="24" :disabled="busy" @click.stop="editServer(server, index)" />
            <PclIconButton icon="trash-2" :label="t('server.delete')" :size="24" danger :disabled="busy" @click.stop="removeServer(index)" />
          </template>
        </PclListItem>
        <div v-if="!servers.length" class="pcl-empty pcl-empty--compact">{{ t('server.empty') }}</div>
      </div>

      <div class="pcl-actions">
        <PclButton variant="highlight" icon="save" :disabled="busy || loading" @click="save">{{ t('server.save') }}</PclButton>
        <span class="pcl-server-path">{{ path || 'servers.dat' }}</span>
      </div>
      <PclHint theme="blue">{{ t('server.hint') }}</PclHint>
    </PclCard>
  </div>
</template>

<script setup lang="ts">
import { onMounted, reactive, ref } from 'vue';
import { useI18n } from 'vue-i18n';
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
const { t } = useI18n();

interface ServerEntry { name: string; ip: string; icon: string; acceptTextures: boolean }
const servers = ref<ServerEntry[]>([]);
const form = ref<ServerEntry>({ name: '', ip: '', icon: '', acceptTextures: false });
const editingIndex = ref<number | null>(null);
const revision = ref('');
const path = ref('');
const loading = ref(false);
const pingState = reactive<Record<string, { loading: boolean; text: string; bad: boolean; icon: string; motd: string; version: string }>>({});
function flattenMotd(value: any): string {
  if (typeof value === 'string') return value.replace(/\u00a7./g, '').trim();
  if (!value || typeof value !== 'object') return '';
  let text = typeof value.text === 'string' ? value.text : '';
  if (Array.isArray(value.extra)) for (const part of value.extra) text += flattenMotd(part);
  return text.replace(/\u00a7./g, '').trim();
}
function serverInfo(server: ServerEntry) {
  const ping = pingState[server.ip];
  const motd = ping?.motd || '';
  return motd ? `${motd} · ${server.ip}` : server.ip;
}
async function pingServer(server: ServerEntry) {
  const key = server.ip;
  pingState[key] = { loading: true, text: t('server.testing'), bad: false, icon: server.icon, motd: '', version: '' };
  try {
    const result = await props.execute('server.ping', { address: key });
    const players = result?.players || {};
    const online = Number(players.online ?? result?.onlinePlayers ?? 0);
    const max = Number(players.max ?? 0);
    const latency = Number(result?.latencyMs ?? 0);
    pingState[key] = {
      loading: false,
      text: t('server.online', { online: max > 0 ? `${online}/${max}` : online, latency }),
      bad: false,
      icon: String(result?.favicon || server.icon || ''),
      motd: flattenMotd(result?.description),
      version: String(result?.version?.name || ''),
    };
  } catch (exception) {
    pingState[key] = { loading: false, text: exception instanceof Error ? exception.message : String(exception), bad: true, icon: server.icon, motd: '', version: '' };
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
