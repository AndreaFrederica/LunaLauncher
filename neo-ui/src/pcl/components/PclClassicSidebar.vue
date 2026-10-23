<template>
  <aside class="pcl-classic-nav">
    <div class="pcl-classic-nav__instance">
      <img v-if="icon" :src="icon" :style="{ width: `${iconSize}px`, height: `${iconSize}px` }" :alt="instance?.name || 'Minecraft'" />
      <span v-else class="pcl-classic-nav__icon-fallback" :style="{ width: `${iconSize}px`, height: `${iconSize}px` }">
        <PclIcon name="box" :size="Math.round(iconSize * 0.45)" />
      </span>
      <strong>{{ instance?.name || '未选择实例' }}</strong>
      <small>{{ version || 'Minecraft' }}</small>
    </div>

    <div v-if="account" class="pcl-classic-nav__account">
      <span class="pcl-classic-nav__avatar"><PclIcon name="user" :size="18" /></span>
      <span><strong>{{ account.profileName || account.displayName || '账户' }}</strong><small>{{ account.type || '已登录' }}</small></span>
    </div>

    <div class="pcl-classic-nav__actions">
      <button type="button" :disabled="!instance || instance.running || busy" @click="emit('launch')">
        <PclIcon name="play" :size="16" /><span>启动 (L)</span>
      </button>
      <button type="button" :disabled="!instance?.running" @click="emit('stop')">
        <PclIcon name="power" :size="15" /><span>结束进程 (K)</span>
      </button>
      <button type="button" :disabled="!instance" @click="emit('settings')">
        <PclIcon name="settings" :size="15" /><span>编辑 (E)</span>
      </button>
      <button type="button" :disabled="!instance" @click="emit('action', 'group')">
        <PclIcon name="folder-open" :size="15" /><span>改变分组 (C)</span>
      </button>
      <button type="button" :disabled="!instance" @click="emit('folder')">
        <PclIcon name="folder-open" :size="15" /><span>实例文件夹 (F)</span>
      </button>
      <button type="button" :disabled="!instance" @click="emit('action', 'export')">
        <PclIcon name="file-archive" :size="15" /><span>导出 (X)</span>
      </button>
      <button type="button" :disabled="!instance" @click="emit('action', 'copy')">
        <PclIcon name="copy" :size="15" /><span>复制 (Y)</span>
      </button>
      <button type="button" class="danger" :disabled="!instance" @click="emit('action', 'delete')">
        <PclIcon name="trash-2" :size="15" /><span>删除 (T)</span>
      </button>
      <button type="button" :disabled="!instance" @click="emit('action', 'shortcut')">
        <PclIcon name="external-link" :size="15" /><span>创建快捷方式 (S)</span>
      </button>
    </div>

    <div class="pcl-classic-nav__servers">
      <div class="pcl-classic-nav__server-title">当前实例服务器</div>
      <div v-if="serversLoading" class="pcl-classic-nav__empty">正在读取…</div>
      <button v-for="server in servers" :key="server.ip" type="button" class="pcl-classic-nav__server" @click="emit('connect', server.ip)">
        <PclIcon name="server" :size="14" />
        <span><strong>{{ server.name || server.ip }}</strong><small>{{ server.ip }}</small></span>
      </button>
      <div v-if="!serversLoading && !servers.length" class="pcl-classic-nav__empty">No Server Configured</div>
    </div>
  </aside>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import PclIcon from './PclIcon.vue';

const props = defineProps<{
  instance?: any;
  icon?: string;
  account?: any;
  servers?: any[];
  serversLoading?: boolean;
  busy?: boolean;
  iconSize?: number;
}>();

const emit = defineEmits<{
  launch: [];
  stop: [];
  settings: [];
  folder: [];
  action: [operation: string];
  connect: [address: string];
}>();

const iconSize = computed(() => Number(props.iconSize) || 96);
const version = computed(() => props.instance?.runtime?.minecraftVersion || props.instance?.runtime?.minecraft || props.instance?.minecraftVersion || 'Minecraft');
</script>

<style scoped>
.pcl-classic-nav {
  display: flex;
  flex-direction: column;
  width: 264px;
  min-width: 264px;
  height: 100%;
  padding: 22px 16px;
  overflow-y: auto;
  border-left: 1px solid var(--pcl-border);
  background: var(--pcl-card);
  color: var(--pcl-text);
}

.pcl-classic-nav__instance {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 5px;
  padding: 8px 0 14px;
  text-align: center;
}

.pcl-classic-nav__instance img,
.pcl-classic-nav__icon-fallback {
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 4px;
  object-fit: cover;
  color: var(--pcl-muted);
  background: var(--pcl-c7);
}

.pcl-classic-nav__instance strong {
  font-size: calc(13px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-classic-nav__instance small {
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-classic-nav__account {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-classic-nav__avatar {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 26px;
  height: 26px;
  border-radius: 4px;
  background: var(--pcl-c7);
  color: var(--pcl-muted);
}

.pcl-classic-nav__account > span:last-child {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.pcl-classic-nav__account strong {
  overflow: hidden;
  font-size: calc(11px * var(--pcl-font-scale, 1));
  font-weight: 500;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-classic-nav__account small {
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
}

.pcl-classic-nav__actions {
  display: flex;
  flex-direction: column;
  padding: 8px 0;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-classic-nav__actions button {
  display: flex;
  align-items: center;
  gap: 8px;
  min-height: 29px;
  padding: 4px 6px;
  border: 0;
  border-radius: 3px;
  background: transparent;
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  text-align: left;
  cursor: pointer;
}

.pcl-classic-nav__actions button:hover:not(:disabled) {
  background: var(--pcl-c8);
}

.pcl-classic-nav__actions button:disabled {
  color: var(--pcl-c5);
  cursor: not-allowed;
}

.pcl-classic-nav__actions button.danger {
  color: #e06c75;
}

.pcl-classic-nav__servers {
  padding-top: 12px;
}

.pcl-classic-nav__server-title {
  margin-bottom: 6px;
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
  text-transform: uppercase;
}

.pcl-classic-nav__server {
  display: flex;
  align-items: center;
  gap: 7px;
  width: 100%;
  padding: 5px 6px;
  border: 0;
  border-radius: 3px;
  background: transparent;
  color: var(--pcl-text);
  text-align: left;
  cursor: pointer;
}

.pcl-classic-nav__server:hover {
  background: var(--pcl-c8);
}

.pcl-classic-nav__server > span {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.pcl-classic-nav__server strong {
  overflow: hidden;
  font-size: calc(10px * var(--pcl-font-scale, 1));
  font-weight: 500;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-classic-nav__server small {
  overflow: hidden;
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-classic-nav__empty {
  padding: 8px 6px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}
</style>
