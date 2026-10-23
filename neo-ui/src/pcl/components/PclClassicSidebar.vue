<template>
  <aside class="pcl-classic-nav">
    <div class="pcl-classic-nav__account-wrap">
      <button type="button" class="pcl-classic-nav__account" :disabled="!accounts.length" @click="accountMenu = !accountMenu">
        <span class="pcl-classic-nav__avatar">
          <PclAccountAvatar v-if="account?.profile?.skin?.url" :src="account.profile.skin.url" :name="account.profileName || account.displayName" />
          <PclIcon v-else name="user" :size="18" />
        </span>
        <span class="pcl-classic-nav__account-copy"><strong>{{ account?.profileName || account?.displayName || '未选择账户' }}</strong><small>{{ account?.type || '点击选择账户' }}</small></span>
        <PclIcon name="chevron-left" :size="12" class="pcl-classic-nav__account-chevron" :class="{ open: accountMenu }" />
      </button>
      <div v-if="accountMenu" class="pcl-classic-nav__account-menu">
        <button v-for="item in accounts" :key="accountKey(item)" type="button" :class="{ active: accountKey(item) === accountId }" @click="chooseAccount(item)">
          <span class="pcl-classic-nav__avatar pcl-classic-nav__avatar--small">
            <PclAccountAvatar v-if="item.profile?.skin?.url" :src="item.profile.skin.url" :name="item.profileName || item.displayName" />
            <PclIcon v-else name="user" :size="14" />
          </span>
          <span><strong>{{ item.profileName || item.displayName || '未命名账户' }}</strong><small>{{ item.type || '账户' }}</small></span>
          <PclIcon v-if="accountKey(item) === accountId" name="circle-check" :size="14" />
        </button>
        <div v-if="!accounts.length" class="pcl-classic-nav__empty">没有可用账户</div>
      </div>
    </div>

    <div class="pcl-classic-nav__instance">
      <img v-if="icon" :src="icon" :style="{ width: `${iconSize}px`, height: `${iconSize}px` }" :alt="instance?.name || 'Minecraft'" />
      <span v-else class="pcl-classic-nav__icon-fallback" :style="{ width: `${iconSize}px`, height: `${iconSize}px` }">
        <PclIcon name="box" :size="Math.round(iconSize * 0.45)" />
      </span>
      <strong>{{ instance?.name || '未选择实例' }}</strong>
      <small>{{ version || 'Minecraft' }}</small>
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
import { computed, ref } from 'vue';
import PclIcon from './PclIcon.vue';
import PclAccountAvatar from './PclAccountAvatar.vue';

const props = defineProps<{
  instance?: any;
  icon?: string;
  account?: any;
  accounts?: any[];
  accountId?: string;
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
  'account-change': [id: string];
}>();

const iconSize = computed(() => Number(props.iconSize) || 96);
const accountMenu = ref(false);
function accountKey(value: any) {
  return String(value?.internalId || value?.id || value?.profileName || '');
}
function chooseAccount(value: any) {
  accountMenu.value = false;
  emit('account-change', accountKey(value));
}

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

.pcl-classic-nav__instance img {
  display: block;
  border-radius: 4px;
  object-fit: contain;
  background: transparent;
}

.pcl-classic-nav__icon-fallback {
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 4px;
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

.pcl-classic-nav__account-wrap {
  position: relative;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-classic-nav__account {
  display: flex;
  align-items: center;
  gap: 8px;
  width: 100%;
  padding: 8px;
  border: 0;
  background: transparent;
  color: inherit;
  text-align: left;
  cursor: pointer;
}

.pcl-classic-nav__account:hover:not(:disabled) { background: var(--pcl-c8); }
.pcl-classic-nav__account:disabled { cursor: default; }
.pcl-classic-nav__account-chevron { margin-left: auto; color: var(--pcl-muted); transform: rotate(-90deg); transition: transform .15s ease; }
.pcl-classic-nav__account-chevron.open { transform: rotate(90deg); }

.pcl-classic-nav__account-menu {
  position: absolute;
  top: calc(100% + 3px);
  right: 0;
  left: 0;
  z-index: 20;
  max-height: 230px;
  overflow-y: auto;
  padding: 4px;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-card);
  box-shadow: var(--pcl-shadow);
}

.pcl-classic-nav__account-menu button {
  display: flex;
  align-items: center;
  gap: 7px;
  width: 100%;
  padding: 5px;
  border: 0;
  border-radius: 3px;
  background: transparent;
  color: var(--pcl-text);
  text-align: left;
  cursor: pointer;
}

.pcl-classic-nav__account-menu button:hover,
.pcl-classic-nav__account-menu button.active { background: var(--pcl-c8); }
.pcl-classic-nav__account-menu button > span:nth-child(2) { display: flex; flex-direction: column; min-width: 0; flex: 1; }
.pcl-classic-nav__account-menu button strong { overflow: hidden; font-size: calc(10px * var(--pcl-font-scale, 1)); font-weight: 500; text-overflow: ellipsis; white-space: nowrap; }
.pcl-classic-nav__account-menu button small { color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-classic-nav__avatar--small { width: 22px; height: 22px; }

.pcl-classic-nav__avatar {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 28px;
  height: 28px;
  overflow: hidden;
  border-radius: 4px;
  background: var(--pcl-c7);
  color: var(--pcl-muted);
}

.pcl-classic-nav__account-copy {
  display: flex;
  flex-direction: column;
  min-width: 0;
  flex: 1;
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
