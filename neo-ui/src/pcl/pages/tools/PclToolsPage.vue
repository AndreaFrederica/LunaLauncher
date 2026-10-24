<template>
  <section class="pcl-page pcl-tools-page">
    <aside class="pcl-page-nav pcl-tools-nav">
      <div class="pcl-tools-title">
        <PclIcon name="tool-case" :size="19" />
        <span>工具</span>
      </div>
      <template v-for="group in itemGroups" :key="group.label">
        <div class="pcl-tools-group">{{ group.label }}</div>
        <button
          v-for="item in group.items"
          :key="item.id"
          type="button"
          class="pcl-page-nav__item"
          :class="{ active: section === item.id }"
          @click="section = item.id"
        >
          <PclIcon :name="item.icon" :size="18" />
          <span>{{ item.label }}</span>
        </button>
      </template>
      <div class="pcl-tools-hint">
        <PclIcon name="info" :size="15" />
        <span>任务、下载和实例状态实时读取启动器；服务器查询使用 mcsrvstat.us。</span>
      </div>
    </aside>

    <main class="pcl-page-main">
      <div class="pcl-page-view">
          <div class="pcl-tools-toolbar">
            <div>
              <h1>{{ current.label }}</h1>
              <p>{{ current.description }}</p>
            </div>
            <PclButton size="sm" icon="refresh-cw" :disabled="busy" @click="reload">刷新</PclButton>
          </div>

          <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

          <div v-show="isMultiplayerSection" class="pcl-tools-stack">
            <PclCard title="P2P 联机">
              <template #actions>
                <PclButton size="sm" icon="refresh-cw" :disabled="backendBusy" @click="refreshBackend">刷新</PclButton>
              </template>

              <div v-if="section === 'multiplayer'" class="pcl-backend-tabs">
                <button
                  v-for="item in backends"
                  :key="item.id"
                  type="button"
                  :class="{ active: backend === item.id }"
                  @click="selectBackend(item.id)"
                >
                  <strong>{{ item.name }}</strong>
                  <span>{{ item.installed ? (item.version || '已安装') : '未安装' }} · {{ item.processRunning ? '进程运行中' : '进程未启动' }}</span>
                </button>
              </div>
              <div v-if="!backends.length" class="pcl-tools-muted">正在读取联机后端…</div>

              <PclLoading v-if="backendBusy" text="正在处理联机请求" inline />
              <PclHint v-if="backendError" theme="red" closable @close="backendError = ''">{{ backendError }}</PclHint>

              <template v-if="backendStatus">
                <div class="pcl-backend-head">
                  <div>
                    <strong>{{ selectedBackend?.name || backendStatus.name || backend }}</strong>
                    <span>{{ backendStatus.version || '未知版本' }} · {{ backendStatus.available ? '后端可用' : '后端未响应' }}</span>
                  </div>
                  <div class="pcl-actions">
                    <PclButton size="sm" icon="download" :disabled="backendBusy" @click="installBackend">
                      {{ backendStatus.installed ? '检查更新' : '安装后端' }}
                    </PclButton>
                    <PclButton size="sm" variant="ghost" icon="refresh-cw" :disabled="backendBusy" @click="refreshBackend">刷新状态</PclButton>
                  </div>
                </div>

                <div v-if="backendState" class="pcl-backend-state">
                  <span class="pcl-backend-pill" :class="`pcl-backend-pill--${backendState.state}`">{{ stateLabel(backendState.state) }}</span>
                  <span v-if="backendState.room">房间码 <b>{{ backendState.room }}</b></span>
                  <span v-if="backendState.url">连接地址 <b>{{ backendState.url }}</b></span>
                  <span v-if="backendState.difficulty && backendState.difficulty !== 'unknown'">难度 {{ backendState.difficulty }}</span>
                </div>

                <div v-if="backendProfiles.length" class="pcl-backend-profiles">
                  <PclListItem
                    v-for="profile in backendProfiles"
                    :key="profile.machineId || profile.name"
                    :title="profile.name"
                    :info="`${profile.kind} · ${profile.vendor || 'Minecraft'}`"
                  >
                    <template #icon><PclIcon name="user" :size="16" /></template>
                  </PclListItem>
                </div>

                <div class="pcl-backend-mode">
                  <button type="button" :class="{ active: backendMode === 'host' }" @click="backendMode = 'host'">创建房间</button>
                  <button type="button" :class="{ active: backendMode === 'guest' }" @click="backendMode = 'guest'">加入房间</button>
                </div>
                <div class="pcl-tools-server-form">
                  <PclInput v-model="playerName" placeholder="玩家名" @enter="startBackend" />
                  <PclInput v-if="backendMode === 'guest'" v-model="roomCode" placeholder="邀请码" @enter="startBackend" />
                  <PclButton variant="highlight" icon="play" :disabled="backendBusy || !playerName.trim() || (backendMode === 'guest' && !roomCode.trim())" @click="startBackend">
                    {{ backendMode === 'host' ? '创建房间' : '加入房间' }}
                  </PclButton>
                </div>
                <div class="pcl-actions">
                  <PclButton variant="red" icon="power" :disabled="backendBusy || !backendState" @click="stopBackend">停止联机</PclButton>
                  <PclButton v-if="backend === 'yukari'" variant="ghost" icon="rotate-ccw" :disabled="backendBusy" @click="retryBackend">重试异常</PclButton>
                  <PclButton variant="ghost" icon="scroll-text" :disabled="backendBusy" @click="loadBackendLog">读取日志</PclButton>
                </div>
              </template>

              <PclHint theme="blue">
                Terracotta 与 YukariConnect 是两套独立的 P2P 联机后端。创建房间前请先启动本地 Minecraft 服务端或局域网世界；加入房间需要对方提供的邀请码。
              </PclHint>
            </PclCard>

            <PclCard title="联机日志">
              <template #actions>
                <PclButton size="sm" variant="ghost" icon="refresh-cw" :disabled="backendBusy" @click="loadBackendLog">刷新</PclButton>
              </template>
              <pre v-if="backendLog" class="pcl-tools-pre">{{ backendLog }}</pre>
              <p v-else class="pcl-tools-muted">读取联机后端自己的日志缓冲区。</p>
            </PclCard>
          </div>

          <div v-show="section === 'tasks'" class="pcl-tools-grid">
            <PclCard title="任务管理">
              <template #actions>
                <PclIconButton icon="refresh-cw" label="刷新任务" :size="24" :disabled="busy" @click="loadTasks" />
              </template>
              <PclLoading v-if="loading" text="正在读取任务" inline />
              <div class="pcl-tools-list">
                <PclListItem
                  v-for="task in tasks"
                  :key="task.id || task.gid"
                  :title="task.name || task.title || task.id || '后台任务'"
                  :info="`${task.status || task.state || '运行中'} · ${task.progress ?? task.completedLength ?? 0}${task.total ? ` / ${task.total}` : ''}`"
                >
                  <template #icon><PclIcon :name="task.running === false ? 'circle-check' : 'download'" :size="16" /></template>
                  <template #actions>
                    <PclButton size="sm" variant="red" :disabled="busy || task.running === false" @click.stop="cancelTask(task)">取消</PclButton>
                  </template>
                </PclListItem>
                <div v-if="!tasks.length" class="pcl-empty pcl-empty--compact">当前没有任务。</div>
              </div>
            </PclCard>

            <PclCard title="下载队列">
              <template #actions>
                <PclButton size="sm" variant="ghost" icon="trash-2" :disabled="busy" @click="clearDownloads">清理已完成</PclButton>
              </template>
              <div class="pcl-tools-list">
                <PclListItem
                  v-for="download in downloads"
                  :key="download.gid"
                  :title="download.name || download.gid"
                  :info="`${download.status || download.state} · ${download.completedLength || 0} / ${download.totalLength || 0}`"
                >
                  <template #icon><PclIcon name="download" :size="16" /></template>
                  <template #actions>
                    <PclButton size="sm" variant="red" :disabled="busy" @click.stop="cancelDownload(download)">取消</PclButton>
                  </template>
                </PclListItem>
                <div v-if="!downloads.length" class="pcl-empty pcl-empty--compact">没有下载项。</div>
              </div>
            </PclCard>
          </div>

          <PclCard v-show="section === 'logs'" title="运行日志">
            <template #actions>
              <span class="pcl-tools-muted">选择实例后进入控制台</span>
            </template>
            <div class="pcl-tools-list">
              <PclListItem
                v-for="instance in instances"
                :key="instance.id"
                :title="instance.name"
                :info="instance.running ? '客户端运行中' : '未运行'"
                @click="emit('open-instance', instance)"
              >
                <template #icon><PclIcon name="scroll-text" :size="16" /></template>
                <template #actions><PclIcon name="chevron-right" :size="14" /></template>
              </PclListItem>
              <div v-if="!instances.length" class="pcl-empty pcl-empty--compact">没有可显示的实例。</div>
            </div>
          </PclCard>

          <PclCard v-show="section === 'scheduler'" title="API 调度分析">
            <template #actions>
              <PclButton size="sm" icon="refresh-cw" @click="loadStats">刷新</PclButton>
            </template>
            <div v-if="backendScheduler" class="pcl-scheduler-summary">
              <span>模式 <b>{{ backendScheduler.mode }}</b></span>
              <span>读并发 <b>{{ backendScheduler.activeReads }} / {{ backendScheduler.readLanes }}</b></span>
              <span>写活跃 <b>{{ backendScheduler.writeActive ? '是' : '否' }}</b></span>
              <span>排队 <b>{{ backendScheduler.queued }}</b></span>
              <span>已完成 <b>{{ backendScheduler.completedReads }} 读 / {{ backendScheduler.completedWrites }} 写</b></span>
            </div>
            <div v-if="stats.length" class="pcl-stats-table">
              <div class="pcl-stats-row pcl-stats-row--head">
                <span>操作</span><span>调用</span><span>缓存</span><span>平均</span><span>最大</span><span>慢请求</span>
              </div>
              <div v-for="row in stats" :key="row.operation" class="pcl-stats-row">
                <span class="pcl-stats-op">{{ row.operation }}</span>
                <span>{{ row.calls }}</span>
                <span>{{ row.cacheHits }}</span>
                <span>{{ row.averageMs }} ms</span>
                <span>{{ row.maxMs }} ms</span>
                <span :class="{ 'pcl-stats-slow': row.slow }">{{ row.slow }}</span>
              </div>
            </div>
            <div v-else class="pcl-empty pcl-empty--compact">还没有 API 调用统计。</div>
            <PclHint theme="blue">
              当前是安全优先的优先级串行队列：短读优先、写操作独占。读线程因 Qt 线程亲和性已停用；超过 900 ms 的请求会在开发者控制台输出 `[launcher-api] slow`。
            </PclHint>
          </PclCard>

          <div v-show="section === 'authlib' || section === 'nide8auth'" class="pcl-tools-stack">
            <PclCard :title="section === 'authlib' ? 'authlib-injector' : 'Nide8Auth'">
              <template #actions>
                <PclButton size="sm" icon="refresh-cw" :disabled="helperBusy" @click="loadHelper(helperKey(section))">刷新状态</PclButton>
              </template>
              <PclLoading v-if="helperBusy" text="正在处理认证组件" inline />
              <div v-if="helperStatus[helperKey(section)]" class="pcl-helper-status">
                <span class="pcl-local-status__dot" :class="{ ok: helperStatus[helperKey(section)].installed }" />
                <div>
                  <strong>{{ helperStatus[helperKey(section)].installed ? '已安装' : '未安装' }} <template v-if="helperStatus[helperKey(section)].version">· {{ helperStatus[helperKey(section)].version }}</template></strong>
                  <small>{{ helperStatus[helperKey(section)].path || '尚未读取路径' }}</small>
                  <small v-if="helperStatus[helperKey(section)].error" class="pcl-helper-error">{{ helperStatus[helperKey(section)].error }}</small>
                </div>
              </div>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="download" :disabled="helperBusy" @click="installHelper(helperKey(section))">{{ helperStatus[helperKey(section)]?.installed ? '重新安装' : '安装组件' }}</PclButton>
                <PclButton variant="red" icon="trash-2" :disabled="helperBusy || !helperStatus[helperKey(section)]?.installed" @click="removeHelper(helperKey(section))">删除组件</PclButton>
              </div>
              <PclHint theme="blue">认证组件用于第三方 Yggdrasil 账户登录。安装后由启动器在启动对应账户时自动注入。</PclHint>
            </PclCard>
          </div>

          <div v-show="section === 'test'" class="pcl-tools-grid">
            <PclCard title="API 测试">
              <template #actions>
                <PclButton size="sm" variant="highlight" :disabled="busy" @click="loadApi">检查能力</PclButton>
              </template>
              <pre v-if="apiInfo" class="pcl-tools-pre">{{ JSON.stringify(apiInfo, null, 2) }}</pre>
              <p v-else class="pcl-tools-muted">检查后端接口是否可用。</p>
            </PclCard>

            <PclCard title="皮肤与头像预览">
              <div class="pcl-tools-skin-form">
                <PclInput v-model="skinUrl" placeholder="皮肤图片 URL" />
                <PclSelect v-model="avatarSize" :options="[64, 128, 256, 512].map(value => ({ label: `${value}px`, value }))" />
                <PclSelect v-model="skinVariant" :options="[{ label: 'Classic', value: 'classic' }, { label: 'Slim', value: 'slim' }]" />
              </div>
              <div class="pcl-tools-skin-preview" :style="skinUrl ? { backgroundImage: `url(${skinUrl})` } : undefined">
                <PclIcon v-if="!skinUrl" name="user" :size="28" />
              </div>
              <div class="pcl-actions">
                <PclButton size="sm" :disabled="!skinUrl" @click="skinPreview = skinUrl">预览头像</PclButton>
                <PclButton size="sm" variant="ghost" :disabled="!skinPreview" @click="saveSkinPreview">保存预览</PclButton>
              </div>
              <img v-if="skinPreview" :src="skinPreview" class="pcl-tools-skin-image" alt="皮肤预览" />
              <p class="pcl-tools-muted">当前预览：{{ avatarSize }}px · {{ skinVariant }}</p>
            </PclCard>

            <PclCard title="服务器查询">
              <div class="pcl-tools-server-form">
                <PclInput v-model="testServer" placeholder="服务器地址" />
                <PclButton size="sm" icon="search" :disabled="!testServer" @click="queryServer(testServer)">查询</PclButton>
              </div>
              <p v-if="serverStatus" class="pcl-tools-muted">{{ serverStatus.name || testServer }} · {{ serverStatus.players ?? 0 }} 人在线{{ serverStatus.latency ? ` · ${serverStatus.latency} ms` : '' }}</p>
            </PclCard>

            <PclCard title="URL / 网络请求">
              <template #actions>
                <PclButton size="sm" icon="external-link" :disabled="urlTesting || !testUrl" @click="testUrlRequest">测试</PclButton>
              </template>
              <PclInput v-model="testUrl" placeholder="https://example.com" />
              <p v-if="urlResult" class="pcl-tools-muted">{{ urlResult }}</p>
            </PclCard>

            <PclCard title="文件哈希">
              <template #actions>
                <PclButton size="sm" icon="shield-check" :disabled="!hashText" @click="hashTextValue">计算 SHA-256</PclButton>
              </template>
              <PclInput v-model="hashText" multiline :rows="4" placeholder="粘贴文本内容进行哈希测试" />
              <p v-if="hashResult" class="pcl-tools-pre pcl-tools-hash">{{ hashResult }}</p>
            </PclCard>

            <PclCard title="成就文本预览">
              <template #actions>
                <PclButton size="sm" variant="ghost" icon="save" @click="saveAchievement">保存</PclButton>
              </template>
              <PclInput v-model="achievementTitle" placeholder="成就标题" />
              <PclInput v-model="achievementLine" placeholder="第一行文本" />
              <div class="pcl-tools-achievement">
                <strong>{{ achievementTitle || '成就标题' }}</strong>
                <span>{{ achievementLine || '成就描述' }}</span>
              </div>
            </PclCard>
          </div>
      </div>
    </main>
  </section>
</template>

<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclSelect from '../../components/PclSelect.vue';
import { launcher } from '../../../boot/launcher-api';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ busy: boolean; execute: Execute; selected?: any; instances: any[] }>();
const emit = defineEmits<{ refresh: []; 'open-instance': [instance: any] }>();

const itemGroups = [
  { label: '实用工具', items: [
    { id: 'multiplayer', label: '联机', icon: 'earth', description: '打开联机工具和服务器连接功能。' },
    { id: 'tasks', label: '任务管理', icon: 'list', description: '查看后台任务和下载队列。' },
    { id: 'logs', label: '日志查看', icon: 'scroll-text', description: '快速进入实例控制台和历史日志。' },
    { id: 'scheduler', label: '调度分析', icon: 'gauge', description: '查看 API 排队、缓存命中与耗时。' },
    { id: 'test', label: '工具测试', icon: 'flask-conical', description: '检查工具能力和 API 连接状态。' },
  ] },
  { label: '外挂组件', items: [
    { id: 'yukari', label: 'YukariConnect', icon: 'globe', description: '管理 YukariConnect P2P 联机后端、房间与日志。' },
    { id: 'terracotta', label: 'Terracotta', icon: 'network', description: '管理 Terracotta P2P 联机后端、房间与日志。' },
    { id: 'authlib', label: 'authlib-injector', icon: 'shield-check', description: '管理 Yggdrasil 认证注入组件。' },
    { id: 'nide8auth', label: 'Nide8Auth', icon: 'key-round', description: '管理 Nide8Auth 认证组件。' },
  ] },
];
const items = itemGroups.flatMap(group => group.items);
const section = ref('multiplayer'), tasks = ref<any[]>([]), downloads = ref<any[]>([]), loading = ref(false);
const apiInfo = ref<any>(null), servers = ref<any[]>([]), serverAddress = ref(''), serverStatus = ref<any>(null);
const skinUrl = ref(''), skinPreview = ref(''), testServer = ref(''), achievementTitle = ref(''), achievementLine = ref('');
const avatarSize = ref(128), skinVariant = ref('classic'), testUrl = ref(''), urlResult = ref(''), urlTesting = ref(false);
const hashText = ref(''), hashResult = ref(''), error = ref('');
const stats = ref<any[]>([]);
const backendScheduler = ref<any>(null);
async function loadStats() {
  stats.value = launcher.getStats();
  try { backendScheduler.value = await props.execute('scheduler.status'); }
  catch { backendScheduler.value = null; }
}
const backends = ref<any[]>([]);
const backend = ref('terracotta');
const backendStatus = ref<any>(null);
const backendBusy = ref(false);
const backendError = ref('');
const backendMode = ref<'host' | 'guest'>('host');
const playerName = ref('Player');
const roomCode = ref('');
const backendLog = ref('');
const helperStatus = ref<Record<string, any>>({});
const helperBusy = ref(false);
let refreshTimer: ReturnType<typeof setInterval> | undefined;
let refreshInFlight = false;
const current = computed(() => items.find(item => item.id === section.value) || items[0]);
const isMultiplayerSection = computed(() => ['multiplayer', 'yukari', 'terracotta'].includes(section.value));
const selectedBackend = computed(() => backends.value.find(item => item.id === backend.value) || null);
const backendState = computed(() => backendStatus.value?.state || null);
const backendProfiles = computed(() => Array.isArray(backendState.value?.profiles) ? backendState.value.profiles : []);

function message(value: unknown) {
  return value instanceof Error ? value.message : String(value);
}

function stateLabel(state: string) {
  const labels: Record<string, string> = {
    waiting: '等待中', 'host-scanning': '正在扫描房间', 'host-starting': '正在创建房间',
    'host-ok': '房间已创建', 'guest-connecting': '正在连接房主', 'guest-starting': '正在加入房间',
    'guest-ok': '已加入房间', exception: '发生异常',
  };
  return labels[state] || state || '未知状态';
}

function readPreference(key: string, fallback: string) {
  try { return localStorage.getItem(key) || fallback; } catch { return fallback; }
}

function savePreference(key: string, value: string) {
  try { localStorage.setItem(key, value); } catch { /* storage unavailable */ }
}

function selectBackend(id: string) {
  backend.value = id;
  savePreference('luna-multiplayer-backend', id);
  backendStatus.value = null;
  backendError.value = '';
  void refreshBackend();
}

async function loadBackends() {
  try {
    const result = await props.execute('multiplayer.backends');
    backends.value = Array.isArray(result?.backends) ? result.backends : [];
    if (backends.value.length && !backends.value.some(item => item.id === backend.value)) backend.value = backends.value[0].id;
    await refreshBackend();
  } catch (exception) {
    backendError.value = message(exception);
  }
}

async function refreshBackend() {
  if (!backend.value) return;
  backendBusy.value = true;
  backendError.value = '';
  try {
    backendStatus.value = await props.execute('multiplayer.status', { backend: backend.value, fetchState: true });
    const currentStatus = backendStatus.value;
    backends.value = backends.value.map(item => item.id === backend.value
      ? { ...item, installed: Boolean(currentStatus?.installed), version: currentStatus?.version || item.version, processRunning: Boolean(currentStatus?.processRunning) }
      : item);
    if (backendStatus.value?.state?.room) roomCode.value = backendStatus.value.state.room;
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function installBackend() {
  backendBusy.value = true;
  backendError.value = '';
  try {
    await props.execute('multiplayer.install', { backend: backend.value });
    await loadBackends();
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function startBackend() {
  if (!playerName.value.trim()) return;
  backendBusy.value = true;
  backendError.value = '';
  try {
    const parameters: Record<string, unknown> = { backend: backend.value, mode: backendMode.value, player: playerName.value.trim() };
    if (backendMode.value === 'guest') parameters.room = roomCode.value.trim();
    await props.execute('multiplayer.start', parameters);
    savePreference('luna-multiplayer-player', playerName.value.trim());
    await refreshBackend();
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function stopBackend() {
  backendBusy.value = true;
  backendError.value = '';
  try {
    await props.execute('multiplayer.stop', { backend: backend.value });
    await refreshBackend();
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function retryBackend() {
  backendBusy.value = true;
  backendError.value = '';
  try {
    await props.execute('multiplayer.retry', { backend: backend.value });
    await refreshBackend();
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}

async function loadBackendLog() {
  backendBusy.value = true;
  backendError.value = '';
  try {
    const result = await props.execute('multiplayer.log', { backend: backend.value });
    backendLog.value = String(result?.log || '');
  } catch (exception) {
    backendError.value = message(exception);
  } finally {
    backendBusy.value = false;
  }
}
async function refreshVisibleData() {
  if (refreshInFlight || props.busy || backendBusy.value || helperBusy.value) return;
  refreshInFlight = true;
  try {
    if (isMultiplayerSection.value) {
      await refreshBackend();
      if (backendLog.value || backendState.value) await loadBackendLog();
    } else if (section.value === 'tasks') {
      await loadTasks();
    } else if (section.value === 'scheduler') {
      await loadStats();
    }
  } finally {
    refreshInFlight = false;
  }
}
async function loadTasks() {
  loading.value = true;
  error.value = '';
  try {
    const result = await props.execute('task.list');
    tasks.value = Array.isArray(result) ? result : (result?.tasks || []);
  } catch (exception) {
    error.value = message(exception);
  }
  try {
    downloads.value = await props.execute('aria2.downloads');
  } catch {
    downloads.value = [];
  } finally {
    loading.value = false;
  }
}
async function reload() {
  if (isMultiplayerSection.value) await refreshVisibleData();
  else if (section.value === 'tasks') await loadTasks();
  else if (section.value === 'scheduler') await loadStats();
  else emit('refresh');
}
async function cancelTask(task: any) {
  try { await props.execute('task.cancel', task.id ? { taskId: task.id } : {}); await loadTasks(); }
  catch (exception) { error.value = message(exception); }
}
async function cancelDownload(download: any) {
  try { await props.execute('aria2.cancel', { gid: download.gid }); await loadTasks(); }
  catch (exception) { error.value = message(exception); }
}
async function clearDownloads() {
  try { await props.execute('aria2.clear-finished'); await loadTasks(); }
  catch (exception) { error.value = message(exception); }
}
async function loadApi() {
  try { apiInfo.value = await props.execute('api.describe'); }
  catch (exception) { apiInfo.value = { error: message(exception) }; }
}
async function loadServers() {
  try {
    const result = JSON.parse(localStorage.getItem('luna-server-list') || '[]');
    servers.value = Array.isArray(result) ? result : [];
  } catch { servers.value = []; }
}
async function queryServer(address = serverAddress.value) {
  if (!address) return;
  serverAddress.value = address;
  error.value = '';
  try {
    const response = await fetch(`https://api.mcsrvstat.us/2/${encodeURIComponent(address)}`);
    const data = await response.json();
    serverStatus.value = {
      name: data.hostname || address, address,
      players: data.players?.online || 0, maxPlayers: data.players?.max || 0,
      latency: data.debug?.ping || undefined, online: Boolean(data.online),
    };
    const next = [...servers.value.filter(item => item.address !== address), { address, name: data.hostname || address, players: data.players?.online || 0 }];
    servers.value = next;
    localStorage.setItem('luna-server-list', JSON.stringify(next.slice(-50)));
  } catch (exception) {
    serverStatus.value = { name: address, error: message(exception) };
  }
}
async function testUrlRequest() {
  urlTesting.value = true;
  urlResult.value = '';
  const start = performance.now();
  try {
    const response = await fetch(testUrl.value, { method: 'HEAD' });
    urlResult.value = `HTTP ${response.status} ${response.statusText} · ${Math.round(performance.now() - start)} ms`;
  } catch (exception) {
    urlResult.value = `请求失败：${message(exception)}`;
  } finally {
    urlTesting.value = false;
  }
}
async function hashTextValue() {
  if (!hashText.value) return;
  const bytes = new TextEncoder().encode(hashText.value);
  const digest = await crypto.subtle.digest('SHA-256', bytes);
  hashResult.value = Array.from(new Uint8Array(digest)).map(value => value.toString(16).padStart(2, '0')).join('');
}
function saveSkinPreview() {
  try { localStorage.setItem('luna-tool-skin-preview', JSON.stringify({ url: skinPreview.value, size: avatarSize.value, variant: skinVariant.value })); }
  catch { /* storage unavailable */ }
}
function helperKey(kind: string) {
  return kind === 'authlib' || kind === 'nide8auth' ? (kind === 'authlib' ? 'authlib-injector' : 'nide8auth') : '';
}
async function loadHelper(helper: string) {
  if (!helper) return;
  helperBusy.value = true;
  try { helperStatus.value[helper] = await props.execute('authentication.helper.status', { helper }); }
  catch (exception) { helperStatus.value[helper] = { error: message(exception) }; }
  finally { helperBusy.value = false; }
}
async function installHelper(helper: string) {
  helperBusy.value = true;
  try { helperStatus.value[helper] = await props.execute('authentication.helper.install', { helper }); }
  catch (exception) { helperStatus.value[helper] = { error: message(exception) }; }
  finally { helperBusy.value = false; }
}
async function removeHelper(helper: string) {
  if (!window.confirm(`确定删除 ${helper} 吗？`)) return;
  helperBusy.value = true;
  try { helperStatus.value[helper] = await props.execute('authentication.helper.remove', { helper, confirm: true }); }
  catch (exception) { helperStatus.value[helper] = { error: message(exception) }; }
  finally { helperBusy.value = false; }
}

function saveAchievement() {
  try { localStorage.setItem('luna-tool-achievement-preview', JSON.stringify({ title: achievementTitle.value, line: achievementLine.value })); }
  catch { /* storage unavailable */ }
}
watch(section, value => {
  if (value === 'scheduler') loadStats();
  if (value === 'yukari' || value === 'terracotta') {
    backend.value = value;
    backendStatus.value = null;
    void refreshBackend();
  }
  if (value === 'authlib' || value === 'nide8auth') void loadHelper(helperKey(value));
});

onMounted(() => {
  backend.value = readPreference('luna-multiplayer-backend', 'terracotta');
  playerName.value = readPreference('luna-multiplayer-player', 'Player') || 'Player';
  void loadTasks();
  void loadBackends();
  void refreshBackend();
  refreshTimer = setInterval(() => { void refreshVisibleData(); }, 3000);
});
onUnmounted(() => { if (refreshTimer) clearInterval(refreshTimer); });
</script>

<style scoped>
.pcl-tools-page {
  width: 100%;
  max-width: none;
  margin: 0;
}

.pcl-tools-nav {
  width: 170px;
  min-width: 170px;
  padding: 14px 0 16px;
}

.pcl-tools-title {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 0 13px 12px;
  border-bottom: 1px solid var(--pcl-border);
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-tools-group {
  padding: 9px 13px 5px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  font-weight: 700;
  letter-spacing: .08em;
}

.pcl-tools-hint {
  display: flex;
  gap: 6px;
  margin: 16px 10px 0;
  padding: 8px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  line-height: 1.6;
}

.pcl-tools-toolbar {
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  gap: 12px;
  padding-bottom: 14px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-tools-toolbar h1 {
  margin: 0;
  color: var(--pcl-text);
  font-size: calc(19px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-tools-toolbar p {
  margin: 3px 0 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-tools-stack {
  display: grid;
  grid-template-columns: minmax(0, 1.25fr) minmax(0, .75fr);
  gap: 12px;
  align-items: start;
}

.pcl-tools-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 12px;
}

.pcl-tools-grid :deep(.pcl-card) {
  margin-bottom: 0;
}

.pcl-tools-server-form {
  display: flex;
  gap: 7px;
  margin-bottom: 10px;
}

.pcl-tools-server-form :deep(.pcl-input) {
  flex: 1;
}

.pcl-tools-server-list,
.pcl-tools-list {
  max-height: 360px;
  overflow-y: auto;
}

.pcl-tools-muted {
  margin: 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  line-height: 1.7;
}

.pcl-tools-pre {
  max-height: 240px;
  margin: 0;
  overflow: auto;
  padding: 9px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c8);
  color: var(--pcl-text);
  white-space: pre-wrap;
  font: calc(11px * var(--pcl-font-scale, 1))/1.55 ui-monospace, Consolas, monospace;
}

.pcl-tools-stack > :deep(.pcl-card) { min-width: 0; }
.pcl-tools-stack > :deep(.pcl-card) .pcl-tools-pre { max-height: calc(100vh - 300px); min-height: 220px; }

.pcl-tools-hash {
  margin-top: 8px;
  overflow-wrap: anywhere;
}

.pcl-tools-skin-form {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 110px;
  gap: 7px;
  margin-bottom: 8px;
}

.pcl-tools-skin-form :deep(.pcl-input) {
  grid-column: 1 / -1;
}

.pcl-tools-skin-preview {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 72px;
  height: 72px;
  margin: 8px 0;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c6) center/cover no-repeat;
  color: var(--pcl-muted);
  image-rendering: pixelated;
}

.pcl-tools-skin-image {
  display: block;
  max-width: 180px;
  max-height: 120px;
  margin-top: 8px;
  object-fit: contain;
  image-rendering: pixelated;
}

.pcl-tools-achievement {
  display: flex;
  flex-direction: column;
  gap: 4px;
  margin-top: 9px;
  padding: 12px;
  border-radius: 4px;
  background: linear-gradient(135deg, var(--pcl-c6), var(--pcl-c8));
  color: var(--pcl-text);
}

.pcl-tools-achievement span {
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-backend-tabs {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 7px;
  margin-bottom: 10px;
}

.pcl-backend-tabs button {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 2px;
  padding: 8px 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-half-white);
  color: var(--pcl-text);
  text-align: left;
  cursor: pointer;
}

.pcl-backend-tabs button.active {
  border-color: var(--pcl-c2);
  color: var(--pcl-c2);
}

.pcl-backend-tabs strong {
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-backend-tabs span {
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-backend-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  margin-bottom: 9px;
}

.pcl-backend-head strong {
  display: block;
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
}

.pcl-backend-head span {
  display: block;
  margin-top: 2px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-backend-state {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px;
  margin-bottom: 9px;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-backend-state b {
  color: var(--pcl-text);
  font-weight: 600;
}

.pcl-backend-pill {
  padding: 2px 8px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 10px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-backend-pill--host-ok,
.pcl-backend-pill--guest-ok {
  border-color: rgba(52, 211, 153, .5);
  color: #34d399;
}

.pcl-backend-pill--exception {
  border-color: rgba(255, 107, 107, .5);
  color: #ff6b6b;
}

.pcl-backend-profiles {
  max-height: 150px;
  margin-bottom: 8px;
  overflow-y: auto;
}

.pcl-backend-mode {
  display: inline-flex;
  gap: 2px;
  margin-bottom: 8px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c8);
  padding: 2px;
}

.pcl-backend-mode button {
  height: 26px;
  padding: 0 11px;
  border: 0;
  border-radius: 3px;
  background: transparent;
  color: var(--pcl-muted);
  font: 400 calc(11px * var(--pcl-font-scale, 1))/1 var(--pcl-font);
  cursor: pointer;
}

.pcl-backend-mode button.active {
  background: var(--pcl-c3);
  color: #fff;
}

.pcl-scheduler-summary {
  display: flex;
  flex-wrap: wrap;
  gap: 8px 14px;
  margin-bottom: 10px;
  padding: 8px 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-half-white);
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-scheduler-summary b {
  color: var(--pcl-c2);
  font-weight: 600;
}

.pcl-stats-table {
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  overflow: hidden;
}

.pcl-stats-row {
  display: grid;
  grid-template-columns: minmax(0, 1.7fr) 52px 52px 72px 72px 58px;
  gap: 6px;
  padding: 7px 9px;
  border-bottom: 1px solid var(--pcl-border);
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-stats-row:last-child {
  border-bottom: 0;
}

.pcl-stats-row--head {
  background: var(--pcl-half-white);
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-stats-op {
  overflow: hidden;
  color: var(--pcl-c2);
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-stats-slow {
  color: #ff6b6b;
  font-weight: 700;
}

@media (max-width: 900px) {
  .pcl-tools-nav {
    width: 152px;
    min-width: 152px;
  }

  .pcl-tools-stack,
  .pcl-tools-grid {
    grid-template-columns: 1fr;
  }
}

.pcl-helper-status { display: flex; align-items: flex-start; gap: 8px; padding: 10px; margin: 8px 0; border: 1px solid var(--pcl-border); border-radius: 4px; background: var(--pcl-c8); }
.pcl-helper-status > div { display: flex; flex-direction: column; gap: 2px; min-width: 0; }
.pcl-helper-status strong { color: var(--pcl-text); font-size: calc(11px * var(--pcl-font-scale, 1)); }
.pcl-helper-status small { color: var(--pcl-muted); font-size: calc(9px * var(--pcl-font-scale, 1)); overflow-wrap: anywhere; }
.pcl-helper-error { color: #fb7185 !important; }
</style>
