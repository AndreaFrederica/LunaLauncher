<template>
  <section class="pcl-launch">
    <Transition name="pcl-page" mode="out-in">
      <div v-if="!selectionOpen" key="launch" class="pcl-launch-grid">
        <div class="pcl-launch-left">
          <Transition name="pcl-launch-panel" mode="out-in">
            <div v-if="launchState === 'launching' || launchState === 'launched'" key="launching" class="pcl-launching-panel" :class="{ 'pcl-launching-panel--done': launchState === 'launched' }">
              <PclLoading />
              <strong class="pcl-launching-title">{{ launchTitle }}</strong>
              <span class="pcl-launching-name">{{ selected?.name || 'Minecraft' }}</span>
              <div class="pcl-progress-track"><span :style="{ width: `${progressPercent}%` }" /></div>
              <div class="pcl-launch-info" :class="{ 'pcl-launch-info--done': launchState === 'launched' }">
                <span><em>当前步骤</em><b>{{ launchStage || '初始化' }}</b></span>
                <span><em>登录方式</em><b>{{ launchLoginMethod || loginMethodLabel }}</b></span>
                <span><em>启动进度</em><b>{{ progressLabel }}</b></span>
                <span v-if="launchSpeed > 0"><em>下载速度</em><b>{{ formatRate(launchSpeed) }}</b></span>
              </div>
              <Transition name="pcl-hint">
                <PclHint v-if="launchState === 'launched' && launchHint" theme="blue" class="pcl-launch-trivia">{{ launchHint }}</PclHint>
              </Transition>
              <PclButton v-if="launchState === 'launching'" class="pcl-cancel-button" icon="x" @click="emit('cancel')">取消启动</PclButton>
            </div>

            <div v-else key="input" class="pcl-input-panel">
              <div class="pcl-account-area">
                <div class="pcl-login-slot">
                  <button class="pcl-login-slot__avatar" type="button" title="切换启动账户" @click="accountMenu = !accountMenu">
                    <PclAccountAvatar v-if="activeAccount?.profile?.skin?.url" :src="activeAccount.profile.skin.url" :name="activeAccount.profileName || activeAccount.displayName" />
                    <PclIcon v-else name="user" :size="26" />
                  </button>
                  <div class="pcl-login-slot__copy">
                    <strong>{{ activeAccount?.profileName || activeAccount?.displayName || '未选择账户' }}</strong>
                    <small>{{ loginMethodLabel }}</small>
                  </div>
                  <button class="pcl-account-toggle" type="button" aria-label="切换账户" @click="accountMenu = !accountMenu">
                    <PclIcon name="chevron-left" :size="14" class="pcl-account-toggle__icon" />
                  </button>
                </div>

                <div v-if="accountMenu" class="pcl-account-menu">
                  <div v-if="accounts.length" class="pcl-account-menu__list">
                    <PclListItem
                      v-for="account in accounts"
                      :key="accountKey(account)"
                      :title="account.profileName || account.displayName || '未命名账户'"
                      :info="accountTypeLabel(account) + (account.default ? ' · 默认' : '')"
                      :selected="accountKey(account) === accountId"
                      @click="chooseAccount(account)"
                    >
                      <template #icon>
                        <span v-if="account.profile?.skin?.url" class="pcl-account-face--menu">
                          <PclAccountAvatar :src="account.profile.skin.url" :name="account.profileName || account.displayName" />
                        </span>
                        <PclIcon v-else :name="account.type === 'msa' || account.type === 'Microsoft' ? 'cloud' : 'user'" :size="18" />
                      </template>
                      <template #actions>
                        <PclIcon v-if="accountKey(account) === accountId" name="circle-check" :size="16" />
                      </template>
                    </PclListItem>
                  </div>
                  <div v-else class="pcl-account-menu__empty">没有账户，请先添加一个启动账户。</div>
                  <div class="pcl-account-menu__actions">
                    <PclButton variant="ghost" icon="user-plus" @click="openAccountPage('add')">添加账户</PclButton>
                    <PclButton variant="ghost" icon="settings" @click="openAccountPage('manage')">管理账户</PclButton>
                  </div>
                </div>
              </div>

              <Transition name="pcl-launch-state" mode="out-in">
                <div v-if="launchState === 'failed' || launchState === 'cancelled'" key="failed" class="pcl-launch-error">
                  <PclHint theme="red" :closable="false">
                    <b>{{ launchState === 'cancelled' ? '已取消启动' : '启动失败' }}</b>
                    <span>{{ launchError || '请检查启动日志后重试。' }}</span>
                  </PclHint>
                  <PclButton icon="refresh-cw" @click="emit('launch')">重试</PclButton>
                </div>
                <PclButton
                  v-else
                  key="button"
                  class="pcl-launch-button"
                  variant="accent"
                  size="xl"
                  :disabled="launchButtonDisabled"
                  :data-launch-state="launchState"
                  @click="onLaunch"
                >
                  <span class="pcl-launch-button__copy">
                    <span class="pcl-launch-button__label"><PclIcon name="play" :size="18" />{{ launchButtonLabel }}</span>
                    <small v-if="selected">{{ instanceDescription }}</small>
                  </span>
                </PclButton>
              </Transition>

              <button class="pcl-instance-button" :disabled="launchState === 'loading'" @click="selectionOpen = true">
                <PclIcon name="layout-list" :size="18" />
                <span class="pcl-instance-button__text">
                  <small>当前实例</small>
                  <strong>{{ selected?.name || (instances.length ? '选择 Minecraft 实例' : '没有可用实例') }}</strong>
                </span>
                <PclIcon name="chevron-right" :size="16" class="pcl-instance-button__arrow" />
              </button>
              <div class="pcl-launch-links">
                <PclButton variant="ghost" icon="folder-open" :disabled="!selected" @click="emit('open-folder')">打开实例目录</PclButton>
                <PclButton variant="ghost" icon="settings" :disabled="!selected" @click="emit('settings')">实例设置</PclButton>
              </div>
            </div>
          </Transition>
        </div>

        <div class="pcl-launch-right">
          <PclCard v-if="hintVisible" title="Neo UI 提示">
            <template #actions>
              <PclIconButton icon="x" label="关闭提示" :size="24" @click="hintDialog = true" />
            </template>
            <p class="pcl-launch-card-copy">感谢你使用 Luna Launcher Neo UI。</p>
            <p class="pcl-launch-card-copy pcl-muted">Neo UI 正在持续完善，遇到问题可以在社区反馈。</p>
          </PclCard>

          <PclCard v-for="card in homeCards" :key="card.id" :title="card.title" class="pcl-home-card">
            <p class="pcl-home-card__content">{{ card.content }}</p>
          </PclCard>

          <PclCard title="启动日志">
            <template #actions>
              <PclIconButton icon="copy" label="复制日志" :size="24" :disabled="!logLines.length" @click="copyLog" />
              <PclIconButton icon="trash-2" label="清空日志" :size="24" :disabled="!logLines.length" @click="emit('clear-log')" />
              <PclIconButton icon="refresh-cw" label="刷新" :size="24" :disabled="busy" @click="emit('refresh')" />
            </template>
            <div ref="logElement" class="pcl-launch-log" aria-live="polite">
              <div v-if="logLines.length" v-for="(line, index) in logLines" :key="`${index}-${line}`">{{ line }}</div>
              <span v-else>{{ notice || (selected ? `已选择实例：${selected.name}` : '尚未选择实例') }}</span>
            </div>
          </PclCard>
        </div>
      </div>

      <PclInstanceSelectView v-else key="selection" :instances="instances" :icons="icons" :selected="selected" @back="selectionOpen = false" @select="selectInstance" @download="emit('download')" @action="(instance, action) => emit('instance-action', instance, action)" />
    </Transition>

    <q-dialog v-model="hintDialog">
      <PclCard title="关闭社区提示" class="pcl-hint-dialog">
        <p>请输入 <b>pclcommunity</b> 以确认关闭。</p>
        <PclInput v-model="hintCode" placeholder="pclcommunity" @enter="confirmHint" />
        <div class="pcl-actions">
          <PclButton variant="ghost" @click="hintDialog = false">取消</PclButton>
          <PclButton variant="highlight" icon="circle-check" @click="confirmHint">确认</PclButton>
        </div>
      </PclCard>
    </q-dialog>

    <div class="pcl-extra-rail">
      <PclExtraButton
        icon="list"
        label="任务中心"
        :badge="busy ? '·' : ''"
        :progress="launchState === 'launching' ? launchProgress : undefined"
        @click="emit('tasks')"
      />
      <PclExtraButton
        v-if="pclMusic.loaded"
        icon="music"
        :label="pclMusic.playing ? '暂停背景音乐' : '播放背景音乐'"
        @click="togglePclMusic"
      />
      <PclExtraButton icon="scroll-text" label="启动日志" @click="scrollToLog" />
      <PclExtraButton icon="refresh-cw" label="刷新" :disabled="busy" @click="emit('refresh')" />
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, nextTick, ref, toRefs, watch } from 'vue';
import PclInstanceSelectView from './PclInstanceSelectView.vue';
import PclCard from '../../components/PclCard.vue';
import PclButton from '../../components/PclButton.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclExtraButton from '../../components/PclExtraButton.vue';
import { neoUiPreferences } from '../../../neo-ui-preferences';
import { pclMusic, togglePclMusic } from '../../composables/usePclMusic';
import { showPclToast } from '../../composables/usePclToast';
import PclAccountAvatar from '../../components/PclAccountAvatar.vue';

type LaunchState = 'loading' | 'disabled' | 'download' | 'launch' | 'launching' | 'launched' | 'failed' | 'cancelled';
const props = defineProps<{ selected?: any; icon?: string; busy: boolean; notice?: string; error?: string; instances: any[]; icons: Record<string, string>; accounts?: any[]; accountId?: string; launchState?: LaunchState; launchProgress?: number; launchStage?: string; launchLoginMethod?: string; launchSpeed?: number; launchError?: string; launchHint?: string; logLines?: string[] }>();
const emit = defineEmits<{ launch: []; cancel: []; select: [instance: any]; 'open-folder': []; settings: []; refresh: []; download: []; tasks: []; 'clear-log': []; 'account-change': [id: string]; 'account-page': [mode: 'add' | 'manage']; 'instance-action': [instance: any, action: string] }>();
const { selected, busy, notice, error, instances, icons } = toRefs(props);
const selectionOpen = ref(false), accountMenu = ref(false), hintDialog = ref(false), hintCode = ref('');
const accounts = computed(() => props.accounts || []);
const accountId = computed(() => props.accountId || '');
const launchState = computed<LaunchState>(() => props.launchState || (props.busy ? 'loading' : props.instances.length ? (props.selected ? 'launch' : 'disabled') : 'download'));
const activeAccount = computed(() => accounts.value.find(account => accountKey(account) === props.accountId) || accounts.value.find(account => account.default) || accounts.value[0]);
const hasAccount = computed(() => Boolean(activeAccount.value));
const instanceDescription = computed(() => { const version = props.selected?.runtime?.minecraftVersion || props.selected?.runtime?.minecraft || props.selected?.minecraftVersion; return version ? `Minecraft ${version}` : (props.selected?.name || 'Minecraft 实例'); });
const loginMethodLabel = computed(() => props.launchLoginMethod || accountTypeLabel(activeAccount.value));
const launchButtonLabel = computed(() => launchState.value === 'loading' ? '正在加载' : launchState.value === 'download' ? '下载 Minecraft' : !hasAccount.value ? '添加账户' : '启动游戏');
const launchButtonDisabled = computed(() => {
  if (launchState.value === 'download') return false;
  if (launchState.value !== 'launch') return true;
  return !props.selected || Boolean(props.selected.running);
});
const progressPercent = computed(() => Math.max(0, Math.min(100, Number(props.launchProgress || 0) * 100)));
const progressLabel = computed(() => `${progressPercent.value.toFixed(2)} %`);
const launchTitle = computed(() => props.launchStage?.toLowerCase().includes('export') ? '正在导出启动脚本' : '正在启动');
const launchStage = computed(() => props.launchStage || '初始化');
const launchLoginMethod = computed(() => props.launchLoginMethod || loginMethodLabel.value);
const launchSpeed = computed(() => Number(props.launchSpeed || 0));
const launchError = computed(() => props.launchError || props.error || '');
const launchHint = computed(() => props.launchHint || '正在准备游戏环境，请稍候。');
const homeCards = computed(() => (neoUiPreferences.pclHomeCards || []).filter(card => card.title || card.content));
const logElement = ref<HTMLElement | null>(null);
function scrollToLog() {
  logElement.value?.scrollIntoView({ behavior: 'smooth', block: 'center' });
}
const logLines = computed(() => (props.logLines || []).slice(-500));

async function copyLog() {
  const text = logLines.value.join('\n');
  if (!text) return;
  try {
    await navigator.clipboard.writeText(text);
    showPclToast({ type: 'success', title: '日志已复制', message: `${logLines.value.length} 行` });
  } catch (error) {
    showPclToast({ type: 'error', title: '复制失败', message: error instanceof Error ? error.message : String(error) });
  }
}
watch(() => props.logLines?.length, async () => {
  await nextTick();
  const element = logElement.value;
  if (!element) return;
  const nearBottom = element.scrollHeight - element.scrollTop - element.clientHeight < 40;
  if (nearBottom) element.scrollTop = element.scrollHeight;
});
const hintVisible = ref(loadHintVisibility());
function loadHintVisibility() { try { return localStorage.getItem('pcl-ce-community-hint-hidden') !== '1'; } catch { return true; } }
function accountKey(account: any) { return String(account?.internalId || account?.id || account?.profileName || ''); }
function accountTypeLabel(account: any) { if (!account) return '请先添加启动账户'; const type = String(account.type || '').toLowerCase(); if (type.includes('msa') || type.includes('microsoft')) return 'Microsoft 账户'; if (type.includes('ygg')) return '第三方认证账户'; return '离线账户'; }
function chooseAccount(account: any) { accountMenu.value = false; emit('account-change', accountKey(account)); }
function openAccountPage(mode: 'add' | 'manage') { accountMenu.value = false; emit('account-page', mode); }
function selectInstance(instance: any) { selectionOpen.value = false; emit('select', instance); }
function onLaunch() {
  if (launchState.value === 'download') { emit('download'); return; }
  if (!hasAccount.value) { emit('account-page', 'add'); return; }
  emit('launch');
}
function confirmHint() { if (hintCode.value.replace(/[^a-z]/gi, '').toLowerCase().includes('pclcommunity')) { hintVisible.value = false; hintDialog.value = false; hintCode.value = ''; try { localStorage.setItem('pcl-ce-community-hint-hidden', '1'); } catch { /* Storage may be unavailable in a restricted webview. */ } } }
function formatRate(value: number) { if (!Number.isFinite(value) || value <= 0) return ''; const units = ['B/s', 'KiB/s', 'MiB/s', 'GiB/s']; let n = value, index = 0; while (n >= 1024 && index < units.length - 1) { n /= 1024; index++; } return `${n < 10 && index > 0 ? n.toFixed(1) : Math.round(n)} ${units[index]}`; }
</script>

<style scoped>
.pcl-launch {
  width: 100%;
  height: calc(100vh - 48px);
  min-height: 0;
  padding: 0;
  color: var(--pcl-text);
  background: var(--pcl-page-bg);
}

.pcl-launch-grid {
  display: grid;
  grid-template-columns: 300px minmax(0, 1fr);
  min-height: 100%;
  height: 100%;
}

.pcl-launch-left {
  position: relative;
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 0;
  padding: 0 20px 20px;
  background: var(--pcl-card);
  box-shadow: 4px 0 4px rgba(0, 0, 0, .07);
}

.pcl-input-panel,
.pcl-launching-panel {
  display: flex;
  flex-direction: column;
  flex: 1;
  min-height: 0;
}

.pcl-input-panel .pcl-account-area {
  display: flex;
  flex: 1;
  flex-direction: column;
  justify-content: center;
  min-height: 0;
}

.pcl-launch-right {
  min-width: 0;
  height: 100%;
  padding: 25px;
  overflow-y: auto;
}

.pcl-launching-panel {
  justify-content: center;
  min-height: 100%;
  height: 100%;
  gap: 2px;
}

.pcl-launching-title {
  margin-top: 4px;
  color: var(--pcl-c3);
  font-size: 20px;
  font-weight: 700;
  text-align: center;
  transform: skewX(-3deg);
}

.pcl-launching-name {
  color: var(--pcl-c3);
  font-size: 13.5px;
  text-align: center;
}

.pcl-progress-track {
  height: 4px;
  margin: 18px 30px 24px;
  overflow: hidden;
  border-radius: 1px;
  background: var(--pcl-c6);
}

.pcl-progress-track > span {
  display: block;
  height: 100%;
  background: linear-gradient(90deg, var(--pcl-c4), var(--pcl-c3));
  transition: width .18s ease-out;
}

.pcl-launch-info {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
  font-size: 12.5px;
}

.pcl-launch-info > span {
  display: inline-flex;
  gap: 7px;
  width: 250px;
  justify-content: center;
}

.pcl-launch-info em {
  color: var(--pcl-text);
  font-style: normal;
  text-align: right;
  opacity: .5;
  min-width: 62px;
}

.pcl-launch-info b {
  color: var(--pcl-text);
  font-weight: 400;
  text-align: left;
  min-width: 120px;
}

.pcl-launch-trivia {
  margin: 18px 2px 0;
  text-align: center;
}

.pcl-cancel-button {
  margin: auto 0 0;
}

.pcl-login-slot {
  display: flex;
  align-items: center;
  gap: 12px;
  width: 100%;
  padding: 10px 4px;
}

.pcl-login-slot__avatar {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 56px;
  width: 56px;
  height: 56px;
  overflow: hidden;
  padding: 0;
  border: 2px solid var(--pcl-c5);
  border-radius: 4px;
  background: var(--pcl-c7);
  color: var(--pcl-c2);
  cursor: pointer;
}

.pcl-login-slot__copy {
  display: flex;
  flex-direction: column;
  min-width: 0;
  flex: 1;
}

.pcl-login-slot__copy strong {
  overflow: hidden;
  color: var(--pcl-text);
  font-size: 14px;
  font-weight: 400;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-login-slot__copy small {
  margin-top: 2px;
  color: var(--pcl-muted);
  font-size: 12px;
}

.pcl-account-toggle {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 26px;
  height: 26px;
  padding: 0;
  border: 0;
  border-radius: 50%;
  background: transparent;
  color: var(--pcl-muted);
  cursor: pointer;
}

.pcl-account-toggle:hover {
  background: var(--pcl-hover);
  color: var(--pcl-c2);
}

.pcl-account-toggle__icon {
  transform: rotate(-90deg);
}

.pcl-account-face--menu {
  display: block;
  width: 24px;
  height: 24px;
  overflow: hidden;
  border-radius: 4px;
}

.pcl-account-menu {
  position: absolute;
  left: 20px;
  right: 20px;
  bottom: 190px;
  z-index: 20;
  overflow: hidden;
  border: 1px solid var(--pcl-c5);
  border-radius: 5px;
  background: var(--pcl-white);
  box-shadow: 0 9px 22px rgba(0, 0, 0, .3);
}

.pcl-account-menu__list {
  max-height: 220px;
  padding: 4px;
  overflow-y: auto;
}

.pcl-account-menu__empty {
  padding: 15px;
  color: var(--pcl-muted);
  font-size: 12px;
}

.pcl-account-menu__actions {
  display: flex;
  gap: 4px;
  padding: 6px;
  border-top: 1px solid var(--pcl-border);
}

.pcl-launch-button {
  width: 100%;
  min-height: 54px;
  margin-top: 10px;
}

.pcl-launch-button__copy {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 1px;
}

.pcl-launch-button__label {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  font-size: 16px;
  font-weight: 700;
}

.pcl-launch-button small {
  font-size: 10.5px;
  font-weight: 400;
  opacity: .8;
}

.pcl-launch-error {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-top: 10px;
}

.pcl-launch-error .pcl-hint {
  flex-direction: column;
  gap: 2px;
}

.pcl-instance-button {
  display: flex;
  align-items: center;
  gap: 9px;
  width: 100%;
  height: 35px;
  margin-top: 10px;
  padding: 0 10px;
  border: 1px solid var(--pcl-c5);
  border-radius: 3px;
  background: var(--pcl-half-white);
  color: var(--pcl-text);
  text-align: left;
  cursor: pointer;
  transition: border-color .12s ease, background .12s ease;
}

.pcl-instance-button:hover:not(:disabled) {
  border-color: var(--pcl-c4);
  background: var(--pcl-c7);
}

.pcl-instance-button:disabled {
  opacity: .55;
  cursor: default;
}

.pcl-instance-button__text {
  display: flex;
  flex-direction: column;
  min-width: 0;
  flex: 1;
}

.pcl-instance-button__text small {
  display: none;
}

.pcl-instance-button__text strong {
  overflow: hidden;
  font-size: 12px;
  font-weight: 400;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-instance-button__arrow {
  color: var(--pcl-muted);
}

.pcl-launch-links {
  display: flex;
  justify-content: space-between;
  gap: 2px;
  margin-top: 8px;
}

.pcl-launch-links .pcl-btn {
  height: 26px;
  padding: 0 6px;
  font-size: 12px;
}

.pcl-launch-card-copy {
  margin: 0 0 7px;
  color: var(--pcl-text);
  font-size: 13px;
  line-height: 1.8;
}

.pcl-launch-card-copy:last-child {
  margin-bottom: 0;
}

.pcl-home-card__content {
  margin: 0;
  color: var(--pcl-text);
  font-size: 12px;
  line-height: 1.75;
  white-space: pre-wrap;
}

.pcl-extra-rail {
  position: fixed;
  right: 24px;
  bottom: 24px;
  z-index: 20;
  display: flex;
  flex-direction: column;
  gap: 9px;
}

.pcl-launch-log {
  max-height: 280px;
  min-height: 180px;
  padding: 11px;
  overflow-y: auto;
  border-radius: 3px;
  background: var(--pcl-c8);
  color: var(--pcl-text);
  font: 12px/1.55 ui-monospace, Consolas, monospace;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}

.pcl-hint-dialog {
  width: 420px;
  max-width: 92vw;
}

.pcl-actions {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  margin-top: 14px;
}

.pcl-launch-panel-enter-active,
.pcl-launch-panel-leave-active,
.pcl-launch-state-enter-active,
.pcl-launch-state-leave-active {
  transition: opacity .15s ease, transform .18s cubic-bezier(.16, 1, .3, 1);
}

.pcl-launch-panel-enter-from,
.pcl-launch-state-enter-from {
  opacity: 0;
  transform: scale(1.04);
}

.pcl-launch-panel-leave-to,
.pcl-launch-state-leave-to {
  opacity: 0;
  transform: scale(.96);
}

@media (max-width: 760px) {
  .pcl-launch-grid {
    grid-template-columns: 1fr;
  }
  .pcl-launch-left {
    height: calc(100vh - 48px);
    min-height: calc(100vh - 48px);
    padding: 20px;
  }
  .pcl-launching-panel {
    min-height: calc(100vh - 96px);
  }
  .pcl-launch-right {
    padding: 16px;
  }
}
</style>
