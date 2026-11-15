<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef-accounts">
    <div class="ef-accounts__toolbar">
      <span>{{ accounts.length }} 个账户</span>
      <EfButton variant="accent" icon="person_add" @click="openLogin">添加账户</EfButton>
      <EfButton icon="sync" :disabled="busy" @click="refreshAll">刷新账户</EfButton>
    </div>

    <div v-if="accounts.length" class="ef-accounts__list">
      <article
        v-for="account in accounts"
        :key="accountRef(account)"
        class="ef-account"
        :class="{ current: accountRef(account) === active }"
        @click="select(account)"
      >
        <span class="ef-account__avatar">{{ initial(account) }}</span>
        <div class="ef-account__copy">
          <strong>{{ account.profileName || account.displayName || '未命名' }}</strong>
          <small>{{ typeLabel(account) }} | {{ stateLabel(account) }} | {{ account.default ? '默认' : '备用' }}</small>
        </div>
        <div class="ef-account__actions">
          <EfIconButton icon="arrow_upward" label="上移" :disabled="busy" @click.stop="move(account, -1)" />
          <EfIconButton icon="arrow_downward" label="下移" :disabled="busy" @click.stop="move(account, 1)" />
          <EfIconButton icon="check_circle" label="设为默认" :disabled="busy || account.default" @click.stop="setDefault(account)" />
          <EfIconButton icon="delete" label="删除" danger :disabled="busy" @click.stop="remove(account)" />
        </div>
      </article>
    </div>
    <EfEmpty v-else title="还没有账户" description="添加微软账户或离线账户后才能启动游戏。" icon="group">
      <EfButton variant="accent" icon="person_add" @click="openLogin">添加账户</EfButton>
    </EfEmpty>

    <div v-if="activeProfile" class="ef-accounts__profile">
      <EfCard title="账户资料">
        <div class="ef-profile">
          <div class="ef-profile__skin">
            <img v-if="activeProfile.skin?.url" :src="activeProfile.skin.url" alt="" />
            <span v-else class="material-icons">person</span>
          </div>
          <div class="ef-profile__copy">
            <h2>{{ activeProfile.name || accountName }}</h2>
            <p>{{ activeProfile.state || '—' }} | {{ activeProfile.canPlayMinecraft ? '可启动' : '不可启动' }}</p>
            <div class="ef-profile__fields">
              <EfField label="披风" :value="activeProfile.capes?.length || 0" />
              <EfField label="皮肤库" :value="skins.length" />
            </div>
            <div class="ef-profile__actions">
              <EfButton size="sm" icon="image" :disabled="busy" @click="installSkin">安装皮肤</EfButton>
              <EfButton size="sm" variant="ghost" icon="restart_alt" :disabled="busy" @click="resetSkin">重置皮肤</EfButton>
            </div>
          </div>
        </div>
        <div v-if="activeProfile.capes?.length" class="ef-capes">
          <button
            v-for="cape in activeProfile.capes"
            :key="cape.id"
            type="button"
            :class="{ current: cape.current }"
            @click="selectCape(cape.id)"
          >{{ cape.alias || cape.id }}</button>
          <button type="button" :class="{ current: !activeProfile.capes.some((cape: any) => cape.current) }" @click="selectCape('')">无披风</button>
        </div>
      </EfCard>
    </div>

    <EfDialog v-model="loginOpen" title="添加账户">
      <div class="ef-form">
        <label>登录方式<EfSelect v-model="login.type" :options="loginTypes" /></label>
        <label v-if="login.type !== 'microsoft'">玩家名<EfInput v-model="login.username" placeholder="输入玩家名" /></label>
        <label v-if="login.type === 'offline'">UUID<EfSelect v-model="login.offlineUuidMode" :options="offlineUuidModes" /></label>
        <label v-if="login.type === 'offline' && login.offlineUuidMode === 'custom'">自定义 UUID<EfInput v-model="login.offlineUuid" placeholder="UUID" /></label>
        <p v-if="loginStatus" class="ef-login-status">{{ loginStatus }}</p>
        <div v-if="deviceCode" class="ef-device">
          <code>{{ deviceCode }}</code>
          <EfButton size="sm" icon="open_in_new" @click="openDeviceUrl">打开登录页</EfButton>
        </div>
      </div>
      <template #actions>
        <EfButton variant="ghost" :disabled="loginBusy" @click="loginOpen = false">取消</EfButton>
        <EfButton variant="accent" :busy="loginBusy" :disabled="login.type !== 'microsoft' && !login.username" @click="submitLogin">继续</EfButton>
      </template>
    </EfDialog>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from 'vue';
import { open as openDialog } from '@tauri-apps/plugin-dialog';
import EfButton from '../components/EfButton.vue';
import EfCard from '../components/EfCard.vue';
import EfDialog from '../components/EfDialog.vue';
import EfEmpty from '../components/feedback/EfEmpty.vue';
import EfField from '../components/data/EfField.vue';
import EfIconButton from '../components/controls/EfIconButton.vue';
import EfInput from '../components/EfInput.vue';
import EfSelect from '../components/EfSelect.vue';

const props = defineProps<{
  accounts: any[];
  busy: boolean;
  execute: (op: string, p?: Record<string, unknown>) => Promise<any>;
  selectedAccount?: string;
}>();
const emit = defineEmits<{
  select: [id: string];
  changed: [];
  toast: [type: 'success' | 'error' | 'info', title: string, message?: string];
}>();

const active = ref(props.selectedAccount || '');
const activeProfile = ref<any>(null);
const skins = ref<any[]>([]);
const loginOpen = ref(false);
const loginBusy = ref(false);
const loginStatus = ref('');
const deviceCode = ref('');
const deviceUrl = ref('');
const loginTypes = [
  { label: '微软账户', value: 'microsoft' },
  { label: '离线账户', value: 'offline' },
  { label: '第三方认证', value: 'yggdrasil' },
];
const offlineUuidModes = [
  { label: '离线 UUID', value: 'offline' },
  { label: '自定义 UUID', value: 'custom' },
];
const login = ref({ type: 'microsoft', username: '', offlineUuidMode: 'offline', offlineUuid: '' });

const accountName = computed(() => props.accounts.find(account => accountRef(account) === active.value)?.profileName || '');

function accountRef(account: any) {
  return String(account?.internalId || account?.id || account?.profileName || '');
}
function initial(account: any) {
  return String(account?.profileName || account?.displayName || '?').slice(0, 1).toUpperCase();
}
function typeLabel(account: any) {
  const type = String(account?.type || '').toLowerCase();
  if (type.includes('msa') || type.includes('microsoft')) return '微软';
  if (type.includes('ygg')) return '第三方';
  if (type.includes('unified')) return '统一通行证';
  return '离线';
}
function stateLabel(account: any) {
  const state = String(account?.stateName || account?.state || 'unchecked').toLowerCase();
  return ({ online: '在线', offline: '离线', expired: '过期', errored: '错误', unchecked: '未检查', working: '刷新中' } as Record<string, string>)[state] || state;
}

function openLogin() {
  loginOpen.value = true;
  loginStatus.value = '';
  deviceCode.value = '';
  login.value = { type: 'microsoft', username: '', offlineUuidMode: 'offline', offlineUuid: '' };
}

function isFinished(task: any) {
  return Boolean(task?.finished) || ['succeeded', 'failed', 'cancelled', 'canceled', 'aborted'].includes(String(task?.state || '').toLowerCase());
}

async function submitLogin() {
  if (loginBusy.value) return;
  loginBusy.value = true;
  loginStatus.value = '准备登录';
  deviceCode.value = '';
  try {
    const parameters: Record<string, unknown> = { type: login.value.type };
    if (login.value.type === 'microsoft') parameters.useDeviceCode = true;
    else parameters.username = login.value.username;
    if (login.value.type === 'offline') {
      parameters.offlineUuidMode = login.value.offlineUuidMode;
      if (login.value.offlineUuidMode === 'custom') parameters.offlineUuid = login.value.offlineUuid;
    }
    let task: any = await props.execute('neo.task.start', { operation: 'account.login', priority: 'high', parameters });
    while (task && !isFinished(task)) {
      deviceCode.value = task.deviceCode || deviceCode.value;
      deviceUrl.value = task.deviceUrl || deviceUrl.value;
      loginStatus.value = String(task.message || task.stage || '等待登录');
      await new Promise(resolve => setTimeout(resolve, 500));
      task = await props.execute('neo.task.status', { taskId: task.taskId });
    }
    if (task?.state !== 'succeeded') throw new Error(task?.error || '登录失败');
    loginStatus.value = '登录完成';
    loginOpen.value = false;
    emit('toast', 'success', '账户已添加');
    emit('changed');
  } catch (error: any) {
    loginStatus.value = error?.message || String(error);
    emit('toast', 'error', '登录失败', loginStatus.value);
  } finally {
    loginBusy.value = false;
  }
}

async function openDeviceUrl() {
  if (!deviceUrl.value) return;
  try { await props.execute('desktop.open-url', { url: deviceUrl.value }); } catch { /* ignore */ }
}

async function select(account: any) {
  active.value = accountRef(account);
  emit('select', active.value);
  try {
    activeProfile.value = await props.execute('account.profile', { account: active.value });
    const list: any = await props.execute('account.skin.list', { account: active.value });
    skins.value = Array.isArray(list) ? list : [];
  } catch {
    activeProfile.value = null;
    skins.value = [];
  }
}

async function move(account: any, delta: number) {
  try {
    await props.execute('account.move', { account: accountRef(account), delta });
    emit('changed');
  } catch { /* shell toast */ }
}
async function setDefault(account: any) {
  try {
    await props.execute('account.set-default', { account: accountRef(account) });
    emit('select', accountRef(account));
    emit('toast', 'success', '已设为默认账户');
    emit('changed');
  } catch { /* shell toast */ }
}
async function remove(account: any) {
  if (!window.confirm(`删除账户“${account.profileName || account.displayName || '未命名'}”？`)) return;
  try {
    await props.execute('account.remove', { account: accountRef(account), confirm: true });
    emit('toast', 'success', '账户已删除');
    if (active.value === accountRef(account)) {
      active.value = '';
      activeProfile.value = null;
      skins.value = [];
    }
    emit('changed');
  } catch { /* shell toast */ }
}

async function refreshAll() {
  try {
    for (const account of props.accounts) {
      const key = accountRef(account);
      await props.execute('neo.task.start', { operation: 'account.refresh', priority: 'high', parameters: { account: key } });
    }
    emit('toast', 'success', '已提交刷新');
    emit('changed');
  } catch { /* shell toast */ }
}

async function installSkin() {
  if (!active.value) return;
  try {
    const path = await openDialog({ multiple: false, filters: [{ name: 'PNG', extensions: ['png'] }] });
    if (!path || Array.isArray(path)) return;
    await props.execute('account.skin.install', { account: active.value, path });
    emit('toast', 'success', '皮肤已安装');
    await select({ id: active.value });
  } catch { /* shell toast */ }
}
async function resetSkin() {
  if (!active.value) return;
  try {
    await props.execute('account.skin.reset', { account: active.value, confirm: true });
    emit('toast', 'success', '皮肤已重置');
    await select({ id: active.value });
  } catch { /* shell toast */ }
}
async function selectCape(cape: string) {
  if (!active.value) return;
  try {
    await props.execute('account.cape.select', { account: active.value, cape });
    emit('toast', 'success', '披风已更新');
    await select({ id: active.value });
  } catch { /* shell toast */ }
}
</script>

<style scoped>
.ef-accounts { width: 100%; }
.ef-accounts__toolbar {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 10px;
}
.ef-accounts__toolbar > span {
  flex: 1;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .12em;
  text-transform: uppercase;
}
.ef-accounts__list { border-top: 1px solid var(--ef-ink); }
.ef-account {
  display: flex;
  align-items: center;
  gap: 9px;
  padding: 8px 6px;
  border-bottom: 1px solid var(--ef-rule);
  cursor: pointer;
}
.ef-account:hover { background: var(--ef-chip); }
.ef-account.current { background: var(--ef-signal); color: var(--ef-on-signal); }
.ef-account__avatar {
  display: grid;
  place-items: center;
  width: 28px;
  height: 28px;
  background: var(--ef-ink);
  color: var(--ef-paper);
  font-family: var(--ef-font-ui);
  font-size: 13px;
  font-weight: 800;
}
.ef-account.current .ef-account__avatar { background: #161616; color: var(--ef-signal); }
.ef-account__copy { flex: 1; min-width: 0; }
.ef-account__copy strong {
  display: block;
  overflow: hidden;
  font-family: var(--ef-font-ui);
  font-size: 12px;
  font-weight: 700;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-account__copy small { display: block; margin-top: 1px; color: var(--ef-ink-2); font-size: 9px; }
.ef-account.current .ef-account__copy small { color: #4a4700; }
.ef-account__actions { display: flex; gap: 1px; }
.ef-account.current .ef-account__actions :deep(.ef-icon-btn) { color: #161616; }
.ef-accounts__profile { margin-top: 12px; }
.ef-profile { display: flex; gap: 12px; align-items: flex-start; }
.ef-profile__skin {
  flex: 0 0 84px;
  width: 84px;
  height: 104px;
  display: grid;
  place-items: center;
  border: 1px solid var(--ef-rule);
  background: var(--ef-chip);
}
.ef-profile__skin img { width: 100%; height: 100%; object-fit: contain; image-rendering: pixelated; }
.ef-profile__skin .material-icons { color: var(--ef-ink-2); font-size: 32px; }
.ef-profile__copy { min-width: 0; }
.ef-profile__copy h2 { margin: 0 0 3px; font-family: var(--ef-font-ui); font-size: 17px; font-weight: 800; }
.ef-profile__copy p { margin: 0 0 7px; color: var(--ef-ink-2); font-size: 10px; }
.ef-profile__fields { display: flex; gap: 8px; margin-bottom: 8px; }
.ef-profile__actions { display: flex; gap: 6px; flex-wrap: wrap; }
.ef-capes { display: flex; flex-wrap: wrap; gap: 5px; margin-top: 12px; padding-top: 10px; border-top: 1px solid var(--ef-rule); }
.ef-capes button {
  padding: 3px 8px;
  border: 1px solid var(--ef-rule);
  background: transparent;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 10px;
  cursor: pointer;
}
.ef-capes button:hover { border-color: var(--ef-ink); color: var(--ef-ink); }
.ef-capes button.current { border-color: transparent; background: var(--ef-signal); color: var(--ef-on-signal); }
.ef-form { display: flex; flex-direction: column; gap: 10px; min-width: min(360px, 80vw); }
.ef-form label {
  display: flex;
  flex-direction: column;
  gap: 4px;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .1em;
  text-transform: uppercase;
}
.ef-login-status { margin: 0; color: var(--ef-ink-2); font-size: 11px; line-height: 1.4; }
.ef-device {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 7px 8px;
  border-left: 2px solid var(--ef-signal);
  background: var(--ef-paper);
}
.ef-device code {
  flex: 1;
  font-family: var(--ef-font-mono);
  font-size: 14px;
  letter-spacing: .1em;
}
</style>
