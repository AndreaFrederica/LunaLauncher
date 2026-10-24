<template>
  <section class="pcl-launch">
    <Transition name="pcl-page" mode="out-in">
      <div v-if="!selectionOpen" key="launch" class="pcl-launch-grid" :class="{ 'is-classic': preferences.pclNavLayout === 'mmc-classic', 'classic-left': preferences.pclClassicSide === 'left' }">
        <div v-if="preferences.pclNavLayout !== 'mmc-classic'" class="pcl-launch-left">
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

        <PclClassicSidebar
          v-if="preferences.pclNavLayout === 'mmc-classic'"
          :instance="selected"
          :icon="selected ? icons[selected.id] : undefined"
          :account="activeAccount"
          :accounts="accounts"
          :account-id="accountId"
          :servers="servers"
          :servers-loading="serversLoading"
          :busy="busy"
          :icon-size="preferences.pclClassicIconSize"
          @launch="emit('launch')"
          @stop="selected && emit('instance-action', selected, 'stop')"
          @settings="emit('settings')"
          @folder="emit('open-folder')"
          @action="(operation) => selected && emit('instance-action', selected, operation)"
          @connect="connectServer({ ip: $event })"
          @account-change="emit('account-change', $event)"
        />

        <div class="pcl-launch-right">
          <PclCard v-if="hintVisible" title="Neo UI 提示" :style="{ order: homeCardOrder('hint') }">
            <template #actions>
              <PclIconButton icon="x" label="关闭提示" :size="24" @click="hintDialog = true" />
            </template>
            <p class="pcl-launch-card-copy">感谢你使用 Luna Launcher Neo UI。</p>
            <p class="pcl-launch-card-copy pcl-muted">Neo UI 正在持续完善，遇到问题可以在社区反馈。</p>
          </PclCard>

          <PclCard v-for="card in homeCards" :key="card.id" :title="card.title" class="pcl-home-card" :style="{ order: homeCardOrder(`custom:${card.id}`) }">
            <p class="pcl-home-card__content">{{ card.content }}</p>
          </PclCard>

          <PclCard title="启动日志" :style="{ order: homeCardOrder('log') }">
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

          <PclCard v-if="preferences.pclHomeShowInstances" title="全部实例" class="pcl-home-instances" :style="[homeInstanceStyle, { order: homeCardOrder('instances') }]">
            <template #actions>
              <PclIconButton icon="grid-2x2" label="网格排列" :size="24" :class="{ active: preferences.pclHomeInstanceLayout === 'grid' }" @click="setInstanceLayout('grid')" />
              <PclIconButton icon="list" label="单列排列" :size="24" :class="{ active: preferences.pclHomeInstanceLayout === 'list' }" @click="setInstanceLayout('list')" />
              <PclIconButton icon="refresh-cw" label="刷新实例" :size="24" :disabled="busy" @click="emit('refresh')" />
            </template>
            <div v-if="instances.length" class="pcl-home-instance-groups" :style="{ maxHeight: homeInstanceMaxHeight }">
              <div v-for="group in homeInstanceGroups" :key="group.id" class="pcl-home-instance-group">
                <div
                  v-if="preferences.pclHomeInstanceGrouping === 'grouped'"
                  class="pcl-home-instance-group__head"
                  @dragover.prevent
                  @drop.prevent="dropOnGroup(group.id)"
                >
                  <PclIcon name="folder-open" :size="13" />
                  <strong>{{ group.label }}</strong>
                  <small>{{ group.items.length }}</small>
                </div>
                <div
                  class="pcl-home-instance-list"
                  :class="`is-${preferences.pclHomeInstanceLayout}`"
                  :style="preferences.pclHomeInstanceLayout === 'grid' ? homeInstanceGridStyle : undefined"
                >
                  <button
                    v-for="item in group.items"
                    :key="item.id"
                    type="button"
                    class="pcl-home-instance"
                    :class="{ active: item.id === selected?.id, 'is-drag-over': dragOverId === item.id, 'text-below': preferences.pclHomeInstanceTextPosition === 'below' }"
                    :draggable="true"
                    :title="`双击启动 ${item.name}`"
                    @click="clickInstance(item)"
                    @dblclick="quickLaunch(item)"
                    @contextmenu.prevent.stop="openContextMenu(item, $event)"
                    @dragstart="startDrag(item, $event)"
                    @dragend="endDrag"
                    @dragover.prevent="dragOverId = item.id"
                    @dragleave="dragOverId = ''"
                    @drop.prevent="dropOnInstance(item, $event)"
                  >
                    <img v-if="icons[item.id]" :src="icons[item.id]" alt="" />
                    <span v-else class="pcl-home-instance__fallback"><PclIcon name="box" :size="20" /></span>
                    <span class="pcl-home-instance__copy">
                      <strong>{{ item.name }}</strong>
                      <small v-if="instanceSubtitle(item)">{{ instanceSubtitle(item) }}</small>
                    </span>
                    <span v-if="item.running" class="pcl-home-instance__running">运行中</span>
                    <span v-else class="pcl-home-instance__hint">{{ preferences.pclHomeInstanceSort === 'manual' ? '拖拽排序 · 双击启动' : '双击启动' }}</span>
                  </button>
                </div>
              </div>
            </div>
            <div v-else class="pcl-empty pcl-empty--compact">还没有实例。</div>
          </PclCard>

          <PclCard v-if="preferences.pclHomeShowServers" title="当前实例服务器" class="pcl-home-servers" :style="{ order: homeCardOrder('servers') }">
            <template #actions>
              <PclIconButton icon="refresh-cw" label="刷新服务器" :size="24" :disabled="busy || serversLoading" @click="loadServers" />
            </template>
            <div v-if="!selected" class="pcl-empty pcl-empty--compact">先选择一个实例。</div>
            <div v-else-if="serversLoading" class="pcl-home-server-loading"><PclLoading inline /> 正在读取服务器列表…</div>
            <div v-else-if="serversError" class="pcl-home-server-error">{{ serversError }}</div>
            <div v-else-if="servers.length" class="pcl-home-server-list">
              <button v-for="server in servers" :key="server.ip" type="button" class="pcl-home-server" @click="connectServer(server)">
                <PclIcon name="server" :size="17" />
                <span class="pcl-home-server__copy"><strong>{{ server.name || server.ip }}</strong><small>{{ server.ip }}</small></span>
                <PclIcon name="play" :size="14" class="pcl-home-server__play" />
              </button>
            </div>
            <div v-else class="pcl-empty pcl-empty--compact">servers.dat 里还没有服务器。</div>
            <p class="pcl-home-servers__hint">单击或双击服务器即可带地址启动当前实例。</p>
          </PclCard>
        </div>
      </div>

      <PclInstanceSelectView v-else key="selection" :instances="instances" :icons="icons" :selected="selected" @back="selectionOpen = false" @select="selectInstance" @download="emit('download')" @action="(instance, action) => emit('instance-action', instance, action)" />
    </Transition>

    <PclLaunchProgressDialog
      v-model="launchDialogOpen"
      :state="launchState"
      :progress="launchProgress"
      :stage="launchStage"
      :speed="launchSpeed"
      :details="launchDetails"
      :children="launchChildren"
      :error="launchError"
      :instance-name="selected?.name"
      :account-name="activeAccount?.profileName || activeAccount?.displayName"
      :login-method="launchLoginMethod"
      :hint="launchHint"
      @cancel="emit('cancel')"
      @retry="emit('launch')"
    />

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

    <div v-if="contextMenu" class="pcl-context-backdrop" @click="closeContextMenu" @contextmenu.prevent="closeContextMenu" />
    <div v-if="contextMenu" class="pcl-instance-context" :style="{ left: `${contextMenu.x}px`, top: `${contextMenu.y}px` }" @click.stop>
      <div class="pcl-instance-context__head">{{ contextMenu.item?.name || '实例操作' }}</div>
      <button
        v-for="action in contextActions(contextMenu.item)"
        :key="action.id"
        type="button"
        :class="{ danger: action.danger }"
        :disabled="action.disabled"
        @click="runContextMenu(action.id)"
      >
        <PclIcon :name="action.icon" :size="15" />
        <span>{{ action.label }}</span>
      </button>
    </div>

    <div class="pcl-extra-rail">
      <PclExtraButton
        v-if="preferences.pclFloatTasks"
        icon="list"
        label="任务中心"
        :badge="busy ? '·' : ''"
        :progress="launchState === 'launching' ? launchProgress : undefined"
        @click="emit('tasks')"
      />
      <PclExtraButton
        v-if="pclMusic.loaded && preferences.pclFloatMusic"
        icon="music"
        :label="pclMusic.playing ? '暂停背景音乐' : '播放背景音乐'"
        @click="togglePclMusic"
      />
      <PclExtraButton v-if="preferences.pclFloatLog" icon="scroll-text" label="启动日志" @click="scrollToLog" />
      <PclExtraButton v-if="preferences.pclFloatRefresh" icon="refresh-cw" label="刷新" :disabled="busy" @click="refreshHome" />
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, nextTick, ref, toRefs, watch } from 'vue';
import PclInstanceSelectView from './PclInstanceSelectView.vue';
import PclClassicSidebar from '../../components/PclClassicSidebar.vue';
import PclLaunchProgressDialog from '../../components/PclLaunchProgressDialog.vue';
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
const props = defineProps<{ selected?: any; icon?: string; busy: boolean; notice?: string; error?: string; instances: any[]; icons: Record<string, string>; accounts?: any[]; accountId?: string; launchState?: LaunchState; launchProgress?: number; launchStage?: string; launchLoginMethod?: string; launchSpeed?: number; launchDetails?: string; launchChildren?: any[]; launchError?: string; launchHint?: string; logLines?: string[]; execute?: (operation: string, parameters?: Record<string, unknown>) => Promise<any> }>();
const emit = defineEmits<{ launch: []; cancel: []; select: [instance: any]; 'open-folder': []; settings: []; refresh: []; download: []; tasks: []; 'clear-log': []; 'account-change': [id: string]; 'account-page': [mode: 'add' | 'manage']; 'instance-action': [instance: any, action: string]; 'quick-launch': [instance: any, switchSelected: boolean]; 'launch-instance': [instance: any]; 'edit-instance': [instance: any]; 'open-instance-folder': [instance: any]; 'connect-server': [address: string] }>();
const { selected, busy, notice, error, instances, icons } = toRefs(props);
const selectionOpen = ref(false), accountMenu = ref(false), hintDialog = ref(false), hintCode = ref('');
const accounts = computed(() => props.accounts || []);
const accountId = computed(() => props.accountId || '');
const launchState = computed<LaunchState>(() => props.launchState || (props.busy ? 'loading' : props.instances.length ? (props.selected ? 'launch' : 'disabled') : 'download'));
const activeAccount = computed(() => accounts.value.find(account => accountKey(account) === props.accountId) || accounts.value.find(account => account.default) || accounts.value[0]);
const launchDialogOpen = ref(false);
watch(launchState, value => {
  if (['launching', 'launched', 'failed', 'cancelled'].includes(value)) launchDialogOpen.value = true;
  if (value === 'launch') launchDialogOpen.value = false;
});

const hasAccount = computed(() => Boolean(activeAccount.value));
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
const launchDetails = computed(() => props.launchDetails || '');
const launchChildren = computed(() => Array.isArray(props.launchChildren) ? props.launchChildren : []);
const launchError = computed(() => props.launchError || props.error || '');
const launchHint = computed(() => props.launchHint || '正在准备游戏环境，请稍候。');
const homeCards = computed(() => (neoUiPreferences.pclHomeCards || []).filter(card => card.title || card.content));
const preferences = neoUiPreferences;
function instanceSubtitle(instance: any) {
  return String(instance?.runtime?.minecraftVersion || instance?.runtime?.minecraft || instance?.minecraftVersion || '').trim();
}
function homeCardOrder(key: string) {
  const index = (preferences.pclHomeCardOrder || []).indexOf(key);
  return index < 0 ? 1000 : index;
}

const homeInstanceStyle = computed(() => {
  const font = Number(preferences.pclHomeInstanceFontSize) || 13;
  const rows = Number(preferences.pclHomeInstanceRows) || 4;
  const rowHeight = Math.round(font * (preferences.pclHomeInstanceTextPosition === 'below' ? 5.2 : 2.75));
  return {
    '--pcl-home-font-size': `${font}px`,
    '--pcl-home-small-size': `${Math.max(9, Math.round(font * 0.8))}px`,
    '--pcl-home-hint-size': `${Math.max(8, Math.round(font * 0.72))}px`,
    '--pcl-home-row-height': `${rowHeight}px`,
    width: `${Math.max(40, Math.min(100, Number(preferences.pclHomeInstanceWidth) || 100))}%`,
  };
});
const homeInstanceGridStyle = computed(() => {
  if (preferences.pclHomeInstanceColumns === 'auto') {
    return { gridTemplateColumns: 'repeat(auto-fill, minmax(130px, 1fr))' };
  }
  return { gridTemplateColumns: `repeat(${preferences.pclHomeInstanceColumns}, minmax(0, 1fr))` };
});
const homeInstanceMaxHeight = computed(() => {
  const font = Number(preferences.pclHomeInstanceFontSize) || 13;
  const rows = Number(preferences.pclHomeInstanceRows) || 4;
  const rowHeight = Math.round(font * (preferences.pclHomeInstanceTextPosition === 'below' ? 5.2 : 2.75));
  return `${rowHeight * rows + 6 * Math.max(0, rows - 1) + 8}px`;
});
const orderedInstances = computed(() => {
  const list = [...props.instances];
  if (preferences.pclHomeInstanceSort === 'name') return list.sort((a, b) => String(a.name || '').localeCompare(String(b.name || ''), 'zh-CN'));
  if (preferences.pclHomeInstanceSort === 'version') return list.sort((a, b) => String(a.runtime?.minecraftVersion || a.runtime?.minecraft || a.minecraftVersion || '').localeCompare(String(b.runtime?.minecraftVersion || b.runtime?.minecraft || b.minecraftVersion || ''), 'zh-CN'));
  const rank = new Map(preferences.pclHomeInstanceOrder.map((id, index) => [id, index]));
  return list.sort((a, b) => (rank.get(a.id) ?? 100000) - (rank.get(b.id) ?? 100000));
});
const homeInstanceGroups = computed(() => {
  if (preferences.pclHomeInstanceGrouping === 'flat') return [{ id: '__all', label: '全部实例', items: orderedInstances.value }];
  const groups = new Map<string, any[]>();
  for (const item of orderedInstances.value) {
    const id = String(item.group || '').trim() || '__regular';
    const list = groups.get(id) || [];
    list.push(item);
    groups.set(id, list);
  }
  return [...groups.entries()].map(([id, items]) => ({ id, label: id === '__regular' ? '常规实例' : id, items })).sort((a, b) => a.label.localeCompare(b.label, 'zh-CN'));
});
const draggingInstance = ref<any>(null), dragOverId = ref('');
function startDrag(instance: any, event: DragEvent) {
  draggingInstance.value = instance;
  dragOverId.value = '';
  if (event.dataTransfer) { event.dataTransfer.effectAllowed = 'move'; event.dataTransfer.setData('text/plain', instance.id); }
}
function endDrag() { draggingInstance.value = null; dragOverId.value = ''; }
async function moveInstanceToGroup(instance: any, group: string) {
  if (!props.execute || !instance?.id) return;
  await props.execute('instance.group', { instance: instance.id, group });
  emit('refresh');
}
function dropOnGroup(groupId: string) {
  const instance = draggingInstance.value;
  endDrag();
  if (!instance) return;
  void moveInstanceToGroup(instance, groupId === '__regular' ? '' : groupId);
}
function dropOnInstance(target: any, _event: DragEvent) {
  const source = draggingInstance.value;
  endDrag();
  if (!source || !target || source.id === target.id) return;
  if (preferences.pclHomeInstanceGrouping === 'grouped' && String(source.group || '') !== String(target.group || '')) {
    void moveInstanceToGroup(source, String(target.group || ''));
  }
  if (preferences.pclHomeInstanceSort !== 'manual') return;
  const ids = orderedInstances.value.map((item: any) => item.id);
  const from = ids.indexOf(source.id), to = ids.indexOf(target.id);
  if (from < 0 || to < 0) return;
  ids.splice(from, 1);
  ids.splice(to, 0, source.id);
  preferences.pclHomeInstanceOrder = ids;
}
const contextMenu = ref<{ item: any; x: number; y: number } | null>(null);
function openContextMenu(instance: any, event: MouseEvent) {
  const x = Math.min(event.clientX, Math.max(0, window.innerWidth - 210));
  const y = Math.min(event.clientY, Math.max(0, window.innerHeight - 320));
  contextMenu.value = { item: instance, x, y };
}
function closeContextMenu() { contextMenu.value = null; }
function contextActions(instance: any) {
  return [
    { id: 'launch', label: '启动', icon: 'play', disabled: Boolean(instance?.running) },
    { id: 'stop', label: '结束进程', icon: 'power', disabled: !instance?.running },
    { id: 'edit', label: '编辑', icon: 'settings', disabled: false },
    { id: 'group', label: '改变分组', icon: 'folder-open', disabled: false },
    { id: 'folder', label: '实例文件夹', icon: 'folder-open', disabled: false },
    { id: 'export', label: '导出', icon: 'file-archive', disabled: false },
    { id: 'copy', label: '复制', icon: 'copy', disabled: false },
    { id: 'delete', label: '删除', icon: 'trash-2', disabled: false, danger: true },
    { id: 'shortcut', label: '创建快捷方式', icon: 'external-link', disabled: false },
  ];
}
function runContextMenu(action: string) {
  const item = contextMenu.value?.item;
  closeContextMenu();
  if (!item) return;
  if (action === 'launch') emit('launch-instance', item);
  else if (action === 'edit') emit('edit-instance', item);
  else if (action === 'folder') emit('open-instance-folder', item);
  else emit('instance-action', item, action);
}
const servers = ref<any[]>([]), serversLoading = ref(false), serversError = ref('');
async function loadServers() {
  servers.value = [];
  serversError.value = '';
  if (!props.execute || !selected.value?.id) return;
  serversLoading.value = true;
  try {
    const result = await props.execute('instance.servers.read', { instance: selected.value.id });
    servers.value = Array.isArray(result?.servers) ? result.servers.map((server: any) => ({
      name: String(server.name || server.ip || ''),
      ip: String(server.ip || ''),
    })).filter((server: any) => server.ip) : [];
  } catch (exception) {
    serversError.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    serversLoading.value = false;
  }
}
watch(() => selected.value?.id, () => { void loadServers(); }, { immediate: true });
let instanceClickTimer: ReturnType<typeof setTimeout> | undefined;
function clickInstance(instance: any) {
  if (instanceClickTimer) clearTimeout(instanceClickTimer);
  instanceClickTimer = setTimeout(() => emit('select', instance), 220);
}
function setInstanceLayout(layout: 'grid' | 'list') { preferences.pclHomeInstanceLayout = layout; }
function quickLaunch(instance: any) {
  if (instanceClickTimer) clearTimeout(instanceClickTimer);
  emit('quick-launch', instance, Boolean(preferences.pclHomeSwitchOnQuickLaunch));
}
function connectServer(server: any) {
  if (server?.ip) emit('connect-server', String(server.ip));
}

const logElement = ref<HTMLElement | null>(null);
function scrollToLog() {
  logElement.value?.scrollIntoView({ behavior: 'smooth', block: 'center' });
  showPclToast({ type: 'info', title: '启动日志', message: logLines.value.length ? '已定位到启动日志' : '暂无启动日志', timeout: 2200 });
}
function refreshHome() {
  emit('refresh');
  showPclToast({ type: 'info', title: '刷新', message: '正在刷新实例和账户信息…', timeout: 2200 });
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
function accountTypeLabel(account: any) { if (!account) return '请先添加启动账户'; const type = String(account.type || '').toLowerCase(); if (type.includes('msa') || type.includes('microsoft')) return 'Microsoft 账户'; if (type.includes('ygg')) { const server = account.yggdrasil?.displayName || account.yggdrasil?.sourceName || account.authServerName || '第三方认证'; return `第三方认证 · ${server}`; } if (type.includes('unified')) { const server = account.unifiedPass?.displayName || account.unifiedPass?.serverName || account.unifiedPass?.serverId || account.authServerName || 'UnifiedPass'; return `UnifiedPass · ${server}`; } return '离线账户'; }
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
  display: flex;
  flex-direction: column;
  gap: 15px;
  min-width: 0;
  height: 100%;
  padding: 25px;
  overflow-y: auto;
}

.pcl-launch-right :deep(.pcl-card) {
  margin-bottom: 0;
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
  font-size: calc(20px * var(--pcl-font-scale, 1));
  font-weight: 700;
  text-align: center;
  transform: skewX(-3deg);
}

.pcl-launching-name {
  color: var(--pcl-c3);
  font-size: calc(13.5px * var(--pcl-font-scale, 1));
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
  font-size: calc(12.5px * var(--pcl-font-scale, 1));
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
  font-size: calc(14px * var(--pcl-font-scale, 1));
  font-weight: 400;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-login-slot__copy small {
  margin-top: 2px;
  color: var(--pcl-muted);
  font-size: calc(12px * var(--pcl-font-scale, 1));
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
  font-size: calc(12px * var(--pcl-font-scale, 1));
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
  font-size: calc(16px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-launch-button small {
  font-size: calc(10.5px * var(--pcl-font-scale, 1));
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
  font-size: calc(12px * var(--pcl-font-scale, 1));
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
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-launch-card-copy {
  margin: 0 0 7px;
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  line-height: 1.8;
}

.pcl-launch-card-copy:last-child {
  margin-bottom: 0;
}

.pcl-home-card__content {
  margin: 0;
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
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
  font: calc(12px * var(--pcl-font-scale, 1))/1.55 ui-monospace, Consolas, monospace;
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

.pcl-home-instances,
.pcl-home-servers {
  margin-top: 14px;
}

.pcl-home-instance-groups {
  max-height: none;
  overflow-y: auto;
  padding-right: 2px;
}

.pcl-home-instance-list {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 5px;
  max-height: none;
  overflow: visible;
}

.pcl-home-instance {
  display: flex;
  align-items: center;
  gap: 7px;
  min-width: 0;
  padding: 5px 7px;
  border: 1px solid transparent;
  border-radius: 3px;
  background: transparent;
  color: var(--pcl-text);
  text-align: left;
  cursor: pointer;
}

.pcl-home-instance:hover { background: var(--pcl-c8); }
.pcl-home-instance.active { border-color: var(--pcl-c3); background: var(--pcl-c8); }
.pcl-home-instance img,
.pcl-home-instance__fallback { width: 28px; height: 28px; flex: 0 0 28px; border-radius: 3px; object-fit: cover; }
.pcl-home-instance__fallback { display: flex; align-items: center; justify-content: center; color: var(--pcl-muted); background: var(--pcl-c7); }
.pcl-home-instance__copy { display: flex; flex-direction: column; min-width: 0; flex: 1; }
.pcl-home-instance__copy strong { overflow: hidden; font-size: var(--pcl-home-font-size, 13px); font-weight: 500; text-overflow: ellipsis; white-space: nowrap; }
.pcl-home-instance__copy small { overflow: hidden; color: var(--pcl-muted); font-size: var(--pcl-home-small-size, 10px); text-overflow: ellipsis; white-space: nowrap; }
.pcl-home-instance__hint { flex: 0 0 auto; color: var(--pcl-muted); font-size: var(--pcl-home-hint-size, 9px); opacity: 0; }
.pcl-home-instance:hover .pcl-home-instance__hint { opacity: 1; }
.pcl-home-instance__running { flex: 0 0 auto; color: var(--pcl-c3); font-size: var(--pcl-home-hint-size, 9px); }

.pcl-home-server-list { display: flex; flex-direction: column; gap: 3px; max-height: 170px; overflow-y: auto; }
.pcl-home-server { display: flex; align-items: center; gap: 8px; width: 100%; padding: 6px 8px; border: 1px solid transparent; border-radius: 3px; background: transparent; color: var(--pcl-text); text-align: left; cursor: pointer; }
.pcl-home-server:hover { border-color: var(--pcl-border); background: var(--pcl-c8); }
.pcl-home-server__copy { display: flex; flex-direction: column; min-width: 0; flex: 1; }
.pcl-home-server__copy strong { overflow: hidden; font-size: calc(10px * var(--pcl-font-scale, 1)); font-weight: 500; text-overflow: ellipsis; white-space: nowrap; }
.pcl-home-server__copy small { overflow: hidden; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); text-overflow: ellipsis; white-space: nowrap; }
.pcl-home-server__play { color: var(--pcl-muted); }
.pcl-home-servers__hint { margin: 6px 0 0; color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-home-server-loading { display: flex; align-items: center; gap: 6px; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); }
.pcl-home-server-error { color: #fb7185; font-size: calc(10px * var(--pcl-font-scale, 1)); }

.pcl-home-instance-list.is-list {
  grid-template-columns: minmax(0, 1fr) !important;
}

.pcl-icon-btn.active {
  color: var(--pcl-c3);
  background: var(--pcl-c6);
}

.pcl-home-instance-groups { display: flex; flex-direction: column; gap: 7px; }
.pcl-home-instance-group__head { display: flex; align-items: center; gap: 5px; min-height: 18px; color: var(--pcl-muted); font-size: var(--pcl-home-small-size, 10px); }
.pcl-home-instance-group__head strong { color: var(--pcl-text); font-weight: 500; }
.pcl-home-instance-group__head small { margin-left: auto; }
.pcl-home-instance-group__head:hover { color: var(--pcl-c3); }
.pcl-home-instance.is-drag-over { border-color: var(--pcl-c3); background: var(--pcl-c6); }

.pcl-launch-grid.is-classic {
  grid-template-columns: minmax(0, 1fr) 264px;
}

.pcl-launch-grid.is-classic.classic-left {
  grid-template-columns: 264px minmax(0, 1fr);
}

.pcl-launch-grid.is-classic .pcl-launch-right { order: 1; }
.pcl-launch-grid.is-classic .pcl-classic-nav { order: 2; }
.pcl-launch-grid.is-classic.classic-left .pcl-launch-right { order: 2; }
.pcl-launch-grid.is-classic.classic-left .pcl-classic-nav { order: 1; border-left: 0; border-right: 1px solid var(--pcl-border); }

.pcl-home-instance.text-below {
  flex-direction: column;
  align-items: center;
  justify-content: flex-start;
  gap: 4px;
  min-width: 0;
  overflow: hidden;
  padding: 7px 5px;
  text-align: center;
}

.pcl-home-instance.text-below img,
.pcl-home-instance.text-below .pcl-home-instance__fallback {
  flex: 0 0 32px;
  width: 32px;
  height: 32px;
}

.pcl-home-instance.text-below .pcl-home-instance__copy {
  flex: 0 0 auto;
  width: 100%;
  align-items: center;
}

.pcl-home-instance.text-below .pcl-home-instance__copy strong {
  display: -webkit-box;
  width: 100%;
  max-height: 2.4em;
  overflow: hidden;
  line-height: 1.2;
  white-space: normal;
  overflow-wrap: anywhere;
  text-overflow: ellipsis;
  -webkit-box-orient: vertical;
  -webkit-line-clamp: 2;
}

.pcl-home-instance.text-below .pcl-home-instance__copy small {
  margin-top: 1px;
}

.pcl-home-instance.text-below .pcl-home-instance__hint {
  display: none;
}

.pcl-home-instance.text-below .pcl-home-instance__running {
  margin-top: 1px;
}

.pcl-context-backdrop {
  position: fixed;
  inset: 0;
  z-index: 90;
}

.pcl-instance-context {
  position: fixed;
  z-index: 91;
  width: 200px;
  padding: 5px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-card);
  box-shadow: 0 10px 28px rgba(0, 0, 0, .38);
}

.pcl-instance-context__head {
  overflow: hidden;
  padding: 5px 7px 7px;
  border-bottom: 1px solid var(--pcl-border);
  color: var(--pcl-muted);
  font-size: calc(9px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-instance-context button {
  display: flex;
  align-items: center;
  gap: 8px;
  width: 100%;
  min-height: 27px;
  padding: 4px 7px;
  border: 0;
  border-radius: 3px;
  background: transparent;
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  text-align: left;
  cursor: pointer;
}

.pcl-instance-context button:hover:not(:disabled) { background: var(--pcl-c8); }
.pcl-instance-context button:disabled { color: var(--pcl-c5); cursor: not-allowed; }
.pcl-instance-context button.danger { color: #e06c75; }
</style>
