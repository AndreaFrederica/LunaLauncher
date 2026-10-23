<template>
  <section class="pcl-page pcl-account-page">
    <aside class="pcl-page-nav">
      <div class="pcl-page-nav__title"><span class="material-icons">person</span><span>账户</span></div>
      <div class="pcl-page-nav__group">账户管理</div>
      <button v-for="item in items" :key="item.id" class="pcl-page-nav__item" :class="{ active: section === item.id }" @click="openSection(item.id)">
        <PclIcon :name="item.icon" :size="18" /><span>{{ item.label }}</span>
      </button>
      <div class="pcl-page-nav__hint"><span class="material-icons">info_outline</span><span>账户、皮肤和认证服务都由 Luna 后端缓存并持久化。</span></div>
    </aside>
    <main class="pcl-page-main">
      <template v-if="section === 'profile'">
        <PclCard title="账户">
          <template #actions>
            <PclButton size="sm" variant="highlight" icon="user-plus" :disabled="busy" @click="openLogin('offline')">添加账户</PclButton>
            <PclIconButton icon="refresh-cw" label="刷新账户" :size="24" :disabled="busy || refreshing" @click="refreshAll" />
          </template>
          <PclAccountPanel v-if="accounts.length" :accounts="accounts" :busy="busy" :execute="execute" @changed="emit('changed')" />
          <div v-else class="pcl-empty pcl-account-empty">
            <PclIcon name="user" :size="40" />
            <strong>还没有账户</strong>
            <span>添加 Microsoft、离线或第三方认证账户后即可启动游戏。</span>
            <div class="pcl-actions">
              <PclButton variant="highlight" icon="user-plus" :disabled="busy" @click="openLogin('offline')">添加离线账户</PclButton>
              <PclButton icon="cloud" :disabled="busy" @click="openLogin('microsoft')">Microsoft 登录</PclButton>
            </div>
          </div>
        </PclCard>
      </template>

      <PclCard v-else-if="section === 'services'" title="认证服务">
        <template #actions>
          <PclButton size="sm" variant="highlight" icon="circle-plus" @click="openPreset()">添加服务</PclButton>
        </template>
        <div class="pcl-account-service-list">
          <PclListItem
            v-for="preset in presets"
            :key="preset.name"
            :title="preset.name"
            :info="presetInfo(preset)"
          >
            <template #icon><PclIcon name="shield-check" :size="18" /></template>
            <template #actions>
              <PclButton v-if="preset.official" size="sm" variant="highlight" icon="account" @click.stop="openLogin('microsoft')">官方登录</PclButton>
              <PclButton size="sm" variant="ghost" icon="refresh-cw" :disabled="busy || testingPreset === preset.name" @click.stop="testPreset(preset, 'ping')">Ping</PclButton>
              <PclButton size="sm" variant="ghost" icon="fact_check" :disabled="busy || testingPreset === preset.name" @click.stop="testPreset(preset, 'function')">功能测试</PclButton>
              <PclButton v-if="!preset.builtin" size="sm" variant="ghost" icon="pencil" @click.stop="openPreset(preset)">编辑</PclButton>
              <PclIconButton v-if="!preset.builtin" icon="trash-2" label="删除服务" :size="24" danger :disabled="busy" @click.stop="removePreset(preset)" />
            </template>
          </PclListItem>
          <div v-if="!presets.length" class="pcl-empty">没有认证服务。</div>
        </div>
        <PclHint v-if="presetTest" :theme="presetTest.passed ? 'blue' : 'yellow'" closable @close="presetTest = null">
          {{ presetTest.name }} · {{ presetTest.message }}
          <span v-if="presetTest.elapsedMs !== undefined"> · {{ Math.round(Number(presetTest.elapsedMs) || 0) }} ms</span>
          <span v-if="presetTest.statusCode"> · HTTP {{ presetTest.statusCode }}</span>
        </PclHint>
      </PclCard>

      <PclCard v-if="section === 'services'" title="本地验证服务">
        <div class="pcl-local-status">
          <span class="pcl-local-status__dot" :class="{ ok: helper?.installed }" />
          <strong>{{ helper ? (helper.installed ? '认证组件已安装' : '认证组件未安装') : helperChecked ? '认证组件状态未知' : '点击“检查认证组件”读取状态' }}</strong>
          <span>由启动器按需启动</span>
        </div>
        <div class="pcl-form-row">
          <span class="pcl-form-row__label">数据目录</span>
          <div class="pcl-form-row__control">
            <code class="pcl-local-path">{{ localDataRoot || '—' }}</code>
          </div>
          <PclButton size="sm" icon="folder-open" :disabled="!localDataRoot" @click="openLocalDir">打开</PclButton>
        </div>
        <div class="pcl-local-network">
          <div class="pcl-local-network__head">
            <strong>对外服务</strong>
            <PclButton size="sm" variant="highlight" icon="save" :disabled="busy" @click="applyLocalService">应用并重启</PclButton>
          </div>
          <PclCheckbox v-model="localExternal" label="允许局域网 / 外网连接" />
          <div class="pcl-local-network__grid">
            <label class="pcl-login-field"><span>监听端口</span><PclInput v-model="localPort" placeholder="25585" /></label>
            <label class="pcl-login-field"><span>对外访问 URL</span><PclInput v-model="localPublicUrl" placeholder="http://192.168.1.10:25585" /></label>
          </div>
          <PclHint v-if="localExternal && !localPublicUrl" theme="yellow">开启对外服务前请填写客户端可访问的 HTTP URL。</PclHint>
        </div>
        <div class="pcl-check-grid">
          <PclCheckbox :model-value="localBool('LocalServiceCache')" label="缓存游戏资源" @update:model-value="setLocalSetting('LocalServiceCache', $event)" />
          <PclCheckbox :model-value="localBool('YggdrasilAccountCache')" label="缓存账户与皮肤" @update:model-value="setLocalSetting('YggdrasilAccountCache', $event)" />
        </div>
        <div class="pcl-local-users">
          <div class="pcl-local-users__head">
            <strong>本地用户数据库</strong>
            <div class="pcl-actions">
              <PclButton size="sm" icon="refresh-cw" :disabled="busy" @click="loadLocalServiceStatus">刷新</PclButton>
              <PclButton size="sm" variant="highlight" icon="user-plus" :disabled="busy" @click="openLocalUser">添加用户</PclButton>
            </div>
          </div>
          <div v-if="localUsers.length" class="pcl-local-users__list">
            <div v-for="user in localUsers" :key="user.id" class="pcl-local-users__row">
              <PclIcon name="user" :size="16" />
              <span>{{ user.username }}</span>
              <small>{{ user.enabled ? '启用' : '禁用' }}</small>
              <PclIconButton icon="trash-2" label="删除用户" :size="22" danger :disabled="busy" @click="removeLocalUser(user)" />
            </div>
          </div>
          <div v-else class="pcl-empty pcl-empty--compact">还没有本地用户。</div>
        </div>
        <PclHint theme="blue">
          服务进程由 Luna 后端在登录或启动时按需拉起，退出启动器时自动停止。这里管理缓存策略、数据目录和认证组件状态。
        </PclHint>
        <div class="pcl-actions">
          <PclButton variant="highlight" icon="user-plus" :disabled="busy" @click="openLogin('yggdrasil', true)">添加本地账户</PclButton>
          <PclButton icon="shield-check" :disabled="busy" @click="checkAuthlib">检查认证组件</PclButton>
        </div>
      </PclCard>

      <PclCard v-else-if="section === 'tools'" title="账户工具">
        <div class="pcl-actions pcl-account-actions">
          <PclButton variant="highlight" icon="refresh-cw" :disabled="busy" @click="refreshAll">刷新所有账户</PclButton>
          <PclButton icon="shield-check" :disabled="busy" @click="checkAuthlib">检查认证组件</PclButton>
          <PclButton icon="download" :disabled="busy" @click="installAuthlib">安装认证组件</PclButton>
        </div>
        <p class="pcl-muted">{{ helper ? `认证组件：${helper.installed ? `已安装 ${helper.version || ''}` : '未安装'}` : '正在读取认证组件状态…' }}</p>
      </PclCard>
    </main>
    <q-dialog v-model="loginShown">
      <q-card class="pcl-dialog pcl-login-dialog">
        <q-card-section>
          <div class="pcl-kicker">账户登录</div>
          <h3>添加账户</h3>
        </q-card-section>
        <q-card-section class="pcl-login-body">
          <label class="pcl-login-field">
            <span>登录方式</span>
            <PclSelect v-model="loginType" :options="loginTypes" />
          </label>
          <label class="pcl-login-field">
            <span>{{ loginType === 'microsoft' ? '已有玩家名（可选）' : '玩家名 / 用户名' }}</span>
            <PclInput v-model="loginForm.username" :placeholder="loginType === 'microsoft' ? '可选' : '请输入玩家名'" />
          </label>
          <template v-if="loginType === 'offline'">
            <label class="pcl-login-field">
              <span>离线 UUID 模式</span>
              <PclSelect v-model="loginForm.offlineUuidMode" :options="offlineUuidModes" />
            </label>
            <label v-if="loginForm.offlineUuidMode === 'custom'" class="pcl-login-field">
              <span>自定义 UUID</span>
              <PclInput v-model="loginForm.offlineUuid" placeholder="32 位十六进制 UUID" />
            </label>
          </template>
          <label v-if="loginType === 'yggdrasil' || loginType === 'unified-pass'" class="pcl-login-field">
            <span>密码</span>
            <PclInput v-model="loginForm.password" type="password" placeholder="密码" />
          </label>
          <template v-if="loginType === 'yggdrasil'">
            <label class="pcl-login-field">
              <span>验证服务器</span>
              <PclSelect v-model="loginPreset" :options="loginPresetOptions" @update:model-value="applyLoginPreset" />
            </label>
            <PclCheckbox v-model="loginForm.managedLocal" label="使用 Luna 本地验证服务器" @update:model-value="toggleManagedLocal" />
            <label v-if="!loginForm.managedLocal" class="pcl-login-field">
              <span>认证 URL</span>
              <PclInput v-model="loginForm.authUrl" placeholder="https://..." />
            </label>
            <label v-if="!loginForm.managedLocal" class="pcl-login-field">
              <span>会话 URL</span>
              <PclInput v-model="loginForm.sessionUrl" placeholder="https://..." />
            </label>
            <label class="pcl-login-field">
              <span>服务名称（可选）</span>
              <PclInput v-model="loginForm.sourceName" placeholder="显示名称" />
            </label>
          </template>
          <template v-if="loginType === 'unified-pass'">
            <label class="pcl-login-field"><span>Server ID</span><PclInput v-model="loginForm.serverId" placeholder="Server ID" /></label>
            <label class="pcl-login-field"><span>API Base URL（可选）</span><PclInput v-model="loginForm.baseUrl" placeholder="https://..." /></label>
          </template>
          <template v-if="loginType === 'microsoft'">
            <label class="pcl-login-field">
              <span>Microsoft 登录方式</span>
              <PclSelect v-model="loginForm.microsoftMode" :options="microsoftModes" />
            </label>
            <PclHint theme="blue">
              {{ loginForm.microsoftMode === 'oauth' ? '会打开浏览器并在本地回调，无需手动输入代码。' : '会显示设备代码，需要在浏览器中输入。' }}
            </PclHint>
            <label class="pcl-login-field">
              <span>没有 Java 档案时创建的名称（可选）</span>
              <PclInput v-model="loginForm.minecraftProfileName" placeholder="可选" />
            </label>
          </template>
        </q-card-section>
        <div v-if="loginBusy" class="pcl-login-progress">
          <div class="pcl-login-progress__head"><PclLoading inline /><span>{{ loginStatus || '正在检查登录状态…' }}</span></div>
          <div v-for="step in loginSteps.slice(0, 8)" :key="step.id || step.label" class="pcl-login-progress__step">
            <span class="pcl-login-progress__dot" :class="step.state" />
            <span>{{ step.label || step.status || '检查中' }}</span>
            <small>{{ step.status || step.details || '' }}</small>
          </div>
        </div>
        <div v-if="loginDeviceUrl" class="pcl-login-device">
          <div class="pcl-login-device__label">{{ loginDeviceCode ? '在浏览器中打开并输入代码' : '浏览器授权页面已打开' }}</div>
          <code v-if="loginDeviceCode" class="pcl-login-device__code">{{ loginDeviceCode }}</code>
          <code class="pcl-login-device__url">{{ loginDeviceUrl }}</code>
          <div class="pcl-actions">
            <PclButton size="sm" icon="external-link" @click="openDeviceUrl">打开浏览器</PclButton>
            <PclButton v-if="loginDeviceCode" size="sm" icon="copy" @click="copyDeviceCode">复制代码</PclButton>
          </div>
        </div>
        <PclHint v-if="loginError" theme="red" closable @close="loginError = ''">{{ loginError }}</PclHint>
        <q-card-actions align="right">
          <PclButton variant="ghost" @click="cancelLogin">取消</PclButton>
          <PclButton variant="highlight" :disabled="busy || loginBusy || (!loginForm.username && loginType !== 'microsoft')" @click="submitLogin">
            {{ loginBusy ? '等待登录' : '继续' }}
          </PclButton>
        </q-card-actions>
      </q-card>
    </q-dialog>
    <q-dialog v-model="presetShown">
      <q-card class="pcl-dialog">
        <q-card-section>
          <div class="pcl-kicker">认证服务</div>
          <h3>{{ editingPreset ? '编辑认证服务' : '添加认证服务' }}</h3>
        </q-card-section>
        <q-card-section class="pcl-login-body">
          <label class="pcl-login-field"><span>服务名称</span><PclInput v-model="presetForm.name" placeholder="例如 LittleSkin" /></label>
          <label class="pcl-login-field"><span>认证 URL</span><PclInput v-model="presetForm.authUrl" placeholder="https://..." /></label>
          <label class="pcl-login-field"><span>会话 URL</span><PclInput v-model="presetForm.sessionUrl" placeholder="https://..." /></label>
        </q-card-section>
        <q-card-actions align="right">
          <PclButton variant="ghost" @click="presetShown = false">取消</PclButton>
          <PclButton variant="highlight" :disabled="busy || !presetForm.name || !presetForm.authUrl" @click="savePreset">保存</PclButton>
        </q-card-actions>
      </q-card>
    </q-dialog>
    <q-dialog v-model="localUserShown">
      <q-card class="pcl-dialog">
        <q-card-section>
          <div class="pcl-kicker">本地用户</div>
          <h3>添加本地验证用户</h3>
        </q-card-section>
        <q-card-section class="pcl-login-body">
          <label class="pcl-login-field"><span>用户名</span><PclInput v-model="localUserForm.username" placeholder="本地用户名" /></label>
          <label class="pcl-login-field"><span>密码</span><PclInput v-model="localUserForm.password" type="password" placeholder="密码" /></label>
        </q-card-section>
        <q-card-actions align="right">
          <PclButton variant="ghost" @click="localUserShown = false">取消</PclButton>
          <PclButton variant="highlight" :disabled="busy || !localUserForm.username || !localUserForm.password" @click="createLocalUser">创建</PclButton>
        </q-card-actions>
      </q-card>
    </q-dialog>
  </section>
</template>
<script setup lang="ts">
import { computed, onMounted, reactive, ref } from 'vue';
import PclAccountPanel from './PclAccountPanel.vue';
import PclCard from '../../components/PclCard.vue';
import PclButton from '../../components/PclButton.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclSelect from '../../components/PclSelect.vue';
type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ accounts: any[]; busy: boolean; execute: Execute }>();
const emit = defineEmits<{ changed: []; 'cancel-login': [] }>();
const items = [{ id: 'profile', label: '账户与皮肤', icon: 'user', description: '管理账户缓存、头像、皮肤和披风。' }, { id: 'services', label: '认证服务', icon: 'shield-check', description: '管理 Microsoft、Luna 本地验证和第三方认证服务。' }, { id: 'tools', label: '账户工具', icon: 'tool-case', description: '刷新账户状态并检查认证组件。' }];
const section = ref('profile'), presets = ref<any[]>([]), helper = ref<any>(null), loginShown = ref(false), presetShown = ref(false), editingPreset = ref<any>(null), loginType = ref('offline');
const presetTest = ref<any>(null), testingPreset = ref('');
const loginBusy = ref(false), loginError = ref('');
const loginTaskId = ref(''), loginDeviceUrl = ref(''), loginDeviceCode = ref('');
const loginStatus = ref(''), loginSteps = ref<any[]>([]);
const localSettings = ref<Record<string, any>>({}), localDataRoot = ref(''), helperChecked = ref(false);
const localExternal = ref(false), localPort = ref('25585'), localPublicUrl = ref(''), localUsers = ref<any[]>([]);
const localUserShown = ref(false), localUserForm = reactive({ username: '', password: '' });
const refreshing = ref(false);
const microsoftModes = [{ label: '浏览器授权（无需输入代码）', value: 'oauth' }, { label: '设备代码', value: 'device' }];
const loginTypes = [
  { label: '离线账户', value: 'offline' },
  { label: 'Microsoft 登录', value: 'microsoft' },
  { label: 'Yggdrasil / Authlib Injector', value: 'yggdrasil' },
  { label: '统一通行证', value: 'unified-pass' },
];
const offlineUuidModes = [{label:'按用户名生成（默认）',value:'default'},{label:'每次随机 UUID',value:'random'},{label:'自定义 UUID',value:'custom'}];
const loginPreset = ref('custom');
const loginForm = reactive<any>({ username: '', password: '', authUrl: '', sessionUrl: '', sourceName: '', authenticateEndpoint: '', refreshEndpoint: '', validateEndpoint: '', profileEndpoint: '', oauthTokenEndpoint: '', tokenType: 'Standard', managedLocal: false, serverId: '', baseUrl: '', minecraftProfileName: '', microsoftMode: 'oauth', offlineUuidMode: 'default', offlineUuid: '' });
const loginPresetOptions = computed(() => [
  { label: '自定义服务器', value: 'custom' },
  ...presets.value.filter((preset: any) => !preset.official && !preset.managedLocal).map((preset: any) => ({ label: preset.name, value: preset.name })),
  { label: 'Luna 本地验证服务器', value: '__local__' },
]);
const presetForm = reactive<any>({ name: '', authUrl: '', sessionUrl: '', tokenType: 'Standard' });
async function loadPresets() { try { const result = await props.execute('account.preset.list'); presets.value = Array.isArray(result) ? result : []; } catch { presets.value = []; } }
onMounted(() => { void loadPresets(); });
async function openLogin(type: string, managedLocal = false) {
  loginType.value = type;
  loginError.value = '';
  loginPreset.value = managedLocal ? '__local__' : 'custom';
  Object.assign(loginForm, { username: '', password: '', authUrl: '', sessionUrl: '', sourceName: '',
    authenticateEndpoint: '', refreshEndpoint: '', validateEndpoint: '', profileEndpoint: '', oauthTokenEndpoint: '',
    tokenType: 'Standard', managedLocal, serverId: '', baseUrl: '', minecraftProfileName: '', microsoftMode: 'oauth',
    offlineUuidMode: 'default', offlineUuid: '' });
  loginShown.value = true;
}
function applyLoginPreset(name: string) {
  if (name === '__local__') {
    loginForm.managedLocal = true;
    Object.assign(loginForm, { authUrl: '', sessionUrl: '', sourceName: 'Luna 本地验证服务器',
      authenticateEndpoint: '', refreshEndpoint: '', validateEndpoint: '', profileEndpoint: '', oauthTokenEndpoint: '' });
    return;
  }
  loginForm.managedLocal = false;
  if (name === 'custom') return;
  const preset = presets.value.find((item: any) => item.name === name);
  if (!preset) return;
  Object.assign(loginForm, {
    authUrl: preset.authUrl || '', sessionUrl: preset.sessionUrl || '',
    sourceName: preset.sourceName || preset.name || '',
    authenticateEndpoint: preset.authenticateEndpoint || '', refreshEndpoint: preset.refreshEndpoint || '',
    validateEndpoint: preset.validateEndpoint || '', profileEndpoint: preset.profileEndpoint || '',
    oauthTokenEndpoint: preset.oauthTokenEndpoint || '', tokenType: preset.tokenType || 'Standard',
  });
}
function toggleManagedLocal(value: boolean) {
  if (value) { loginPreset.value = '__local__'; applyLoginPreset('__local__'); }
  else if (loginPreset.value === '__local__') loginPreset.value = 'custom';
}
async function loadLocalServiceStatus() {
  try {
    const status = await props.execute('local.yggdrasil.status');
    localExternal.value = Boolean(status?.external);
    localPort.value = String(status?.port || 25585);
    localPublicUrl.value = String(status?.publicUrl || '');
    localUsers.value = (Array.isArray(status?.users) ? status.users : []).filter((user: any) => !String(user.username || '').startsWith('snapshot-'));
    if (status?.dataDirectory) localDataRoot.value = String(status.dataDirectory);
  } catch {
    localUsers.value = [];
  }
}
async function applyLocalService() {
  try {
    await props.execute('local.yggdrasil.configure', {
      external: localExternal.value,
      port: Number(localPort.value) || 25585,
      publicUrl: localPublicUrl.value.trim(),
    });
    await loadLocalServiceStatus();
  } catch (exception) {
    alert(exception instanceof Error ? exception.message : String(exception));
  }
}
function openLocalUser() {
  localUserForm.username = '';
  localUserForm.password = '';
  localUserShown.value = true;
}
async function createLocalUser() {
  try {
    await props.execute('local.yggdrasil.users.create', { username: localUserForm.username, password: localUserForm.password });
    localUserShown.value = false;
    await loadLocalServiceStatus();
  } catch (exception) {
    alert(exception instanceof Error ? exception.message : String(exception));
  }
}
async function removeLocalUser(user: any) {
  if (!window.confirm(`删除本地用户 ${user.username}？`)) return;
  try {
    await props.execute('local.yggdrasil.users.remove', { id: user.id, confirm: true });
    await loadLocalServiceStatus();
  } catch (exception) {
    alert(exception instanceof Error ? exception.message : String(exception));
  }
}

function localBool(key: string): boolean {
  const value = localSettings.value[key];
  if (typeof value === 'string') return ['true', '1', 'yes', 'on'].includes(value.trim().toLowerCase());
  return Boolean(value);
}
async function loadLocalSettings() {
  try {
    const list = await props.execute('settings.list', { scope: 'launcher' });
    const entries = Array.isArray(list) ? list : (list?.entries || []);
    localSettings.value = Object.fromEntries(entries.map((entry: any) => [entry.key, entry.value]));
  } catch {
    localSettings.value = {};
  }
  try {
    const info = await props.execute('runtime.info', {});
    const root = String(info?.dataRoot || '');
    localDataRoot.value = root ? `${root.replace(/[\\/]+$/, '')}/yggdrasil` : '';
  } catch {
    localDataRoot.value = '';
  }
}
async function setLocalSetting(key: string, value: boolean) {
  const current = localSettings.value[key];
  const normalized = typeof current === 'string' ? String(value) : value;
  await props.execute('settings.set', { scope: 'launcher', key, value: normalized });
  await loadLocalSettings();
}
async function openLocalDir() {
  if (localDataRoot.value) await props.execute('desktop.open-path', { path: localDataRoot.value });
}
function cancelLogin() {
  loginShown.value = false;
  loginBusy.value = false;
  loginDeviceUrl.value = '';
  loginDeviceCode.value = '';
  if (loginTaskId.value) void props.execute('neo.task.cancel', { taskId: loginTaskId.value }).catch(() => {});
  loginTaskId.value = '';
  emit('cancel-login');
}

async function openDeviceUrl() {
  if (!loginDeviceUrl.value) return;
  try {
    await props.execute('desktop.open-url', { url: loginDeviceUrl.value });
  } catch (exception) {
    loginError.value = exception instanceof Error ? exception.message : String(exception);
    try { window.open(loginDeviceUrl.value, '_blank', 'noopener,noreferrer'); } catch { /* WebView may block popups. */ }
  }
}

async function copyDeviceCode() {
  if (!loginDeviceCode.value) return;
  try { await navigator.clipboard.writeText(loginDeviceCode.value); } catch { /* ignore */ }
}

async function submitLogin() {
  const payload: any = { type: loginType.value, ...loginForm };
  if (loginType.value === 'microsoft') payload.useDeviceCode = loginForm.microsoftMode !== 'oauth';
  if (loginType.value !== 'offline') { delete payload.offlineUuidMode; delete payload.offlineUuid; }
  if (!payload.username && loginType.value !== 'microsoft') return;
  loginBusy.value = true;
  loginError.value = '';
  loginDeviceUrl.value = '';
  loginDeviceCode.value = '';
  loginStatus.value = '正在准备登录…';
  loginSteps.value = [];
  try {
    {
      let task: any = await props.execute('neo.task.start', { operation: 'account.login', priority: 'high', parameters: payload });
      loginTaskId.value = task?.taskId || '';
      while (task && !task.finished) {
        loginDeviceUrl.value = task.deviceUrl || '';
        loginDeviceCode.value = task.deviceCode || '';
        loginStatus.value = task.status || task.details || loginStatus.value;
        loginSteps.value = Array.isArray(task.children) ? task.children : [];
        await new Promise(resolve => setTimeout(resolve, 400));
        task = await props.execute('neo.task.status', { taskId: task.taskId });
      }
      if (!task || task.state !== 'succeeded') throw new Error(task?.error || '账户登录失败');
    }
    loginShown.value = false;
    loginDeviceUrl.value = '';
    loginDeviceCode.value = '';
    loginStatus.value = '';
    loginSteps.value = [];
    loginTaskId.value = '';
    emit('changed');
  } catch (exception) {
    loginError.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    loginBusy.value = false;
    loginStatus.value = '';
    loginSteps.value = [];
  }
}
function presetInfo(preset: any) {
  if (preset.official) return 'Microsoft 官方账户 · 官方服务器 · 内置';
  if (preset.managedLocal) return 'Luna 本地验证服务 · 内置';
  return `${preset.authUrl || ''} · ${preset.sessionUrl || ''}${preset.builtin ? ' · 内置' : ''}`;
}
async function testPreset(preset: any, mode: 'ping' | 'function') {
  if (testingPreset.value) return;
  const url = preset.testUrl || preset.authUrl || preset.sessionUrl;
  if (!url) { presetTest.value = { name: preset.name, passed: false, message: '没有可测试的地址' }; return; }
  testingPreset.value = preset.name;
  presetTest.value = null;
  try {
    const result = await props.execute('account.preset.test', { url, mode });
    presetTest.value = { name: preset.name, ...result };
  } catch (exception) {
    presetTest.value = { name: preset.name, passed: false, message: exception instanceof Error ? exception.message : String(exception) };
  } finally {
    testingPreset.value = '';
  }
}
function openPreset(preset?: any) { editingPreset.value = preset || null; Object.assign(presetForm, preset ? { name: preset.name, authUrl: preset.authUrl || '', sessionUrl: preset.sessionUrl || '' } : { name: '', authUrl: '', sessionUrl: '', tokenType: 'Standard' }); presetShown.value = true; }
async function savePreset() { const payload: any = { ...presetForm }; if (editingPreset.value) await props.execute('account.preset.edit', { originalName: editingPreset.value.name, ...payload }); else await props.execute('account.preset.add', payload); presetShown.value = false; await loadPresets(); }
async function removePreset(preset: any) { await props.execute('account.preset.remove', { name: preset.name, confirm: true }); await loadPresets(); }
async function refreshAll() {
  if (refreshing.value) return;
  refreshing.value = true;
  try {
    let tasks: any[] = [];
    for (const account of props.accounts) {
      const key = String(account.internalId || account.id || account.profileName);
      const task = await props.execute('neo.task.start', { operation: 'account.refresh', priority: 'high', parameters: { account: key } });
      if (task?.taskId) tasks.push(task);
    }
    while (tasks.some(task => task && !task.finished)) {
      await new Promise(resolve => setTimeout(resolve, 300));
      tasks = await Promise.all(tasks.map(task => task.finished ? task : props.execute('neo.task.status', { taskId: task.taskId })));
    }
    const failed = tasks.find(task => task?.state !== 'succeeded');
    if (failed) throw new Error(failed.error || '账户刷新失败');
    emit('changed');
  } catch (exception) {
    loginError.value = exception instanceof Error ? exception.message : String(exception);
  } finally {
    refreshing.value = false;
  }
}
async function checkAuthlib() {
  try {
    helper.value = await props.execute('authentication.helper.status', { helper: 'authlib-injector' });
  } catch {
    helper.value = null;
  } finally {
    helperChecked.value = true;
  }
}
async function installAuthlib() { await props.execute('authentication.helper.install', { helper: 'authlib-injector' }); await checkAuthlib(); }
function openSection(id: string) {
  section.value = id;
  if (id === 'services') void loadServiceData();
}
async function loadServiceData() {
  await loadPresets();
  await loadLocalSettings();
  await checkAuthlib();
  await loadLocalServiceStatus();
}

</script>
<style scoped>
.pcl-page{display:flex;min-height:calc(100vh - 48px);width:100%;color:#31485d}.pcl-page-nav{width:236px;min-width:236px;padding:25px 14px 30px 0;border-right:1px solid #d8e2eb}.pcl-page-nav__title{display:flex;align-items:center;gap:10px;padding:0 15px 20px;color:#315d80;font-size:18px;font-weight:700}.pcl-page-nav__title .material-icons{color:#3b83b7}.pcl-page-nav__group{padding:18px 15px 7px;color:#8a99a7;font-size:10px;font-weight:800;letter-spacing:.12em}.pcl-page-nav__item{display:flex;align-items:center;gap:10px;width:100%;height:39px;padding:0 14px;border:0;border-radius:5px;background:transparent;color:#607488;text-align:left;font:inherit;font-size:13px;cursor:pointer}.pcl-page-nav__item:hover,.pcl-page-nav__item.active{background:#dcecf8;color:#236a9f}.pcl-page-nav__item.active{font-weight:700;box-shadow:inset 3px 0 #3c8bc9}.pcl-page-nav__hint{display:flex;gap:8px;margin:28px 12px 0;padding:11px;border:1px solid #d9e4ec;border-radius:5px;color:#8a99a7;font-size:11px;line-height:1.5}.pcl-page-main{flex:1;min-width:0;padding:30px 34px 54px}.pcl-page-header{display:flex;justify-content:space-between;align-items:flex-end;gap:24px;padding-bottom:20px;border-bottom:1px solid #d8e2eb}.pcl-page-header h1{margin:4px 0 5px;color:#2f5979;font-size:28px}.pcl-page-header p{margin:0;color:#81909d;font-size:13px}.pcl-kicker{color:#4284b2;font-size:10px;letter-spacing:.16em;font-weight:800}.pcl-panel{margin-top:22px;padding:22px;border:1px solid #d5e0e8;background:#fff!important;border-radius:7px;box-shadow:0 4px 13px #22445f12}.pcl-card-heading{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px;color:#355d79;font-weight:700;font-size:16px}.pcl-account-actions{display:flex;gap:9px;flex-wrap:wrap}.pcl-muted{color:#81909d;font-size:12px}.pcl-dialog{width:520px;max-width:94vw;background:#fff;color:#31485d}.pcl-dialog h3{margin:4px 0 0;color:#2f5979}
.pcl-account-empty {
  min-height: 300px;
}

.pcl-account-service-list {
  max-height: 460px;
  overflow-y: auto;
}

.pcl-account-actions {
  margin-top: 0;
}

.pcl-local-status {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 12px;
  color: var(--pcl-text);
  font-size: 13px;
}

.pcl-local-status strong {
  font-weight: 700;
}

.pcl-local-status > span:last-child {
  color: var(--pcl-muted);
}

.pcl-local-status__dot {
  width: 9px;
  height: 9px;
  border-radius: 50%;
  background: var(--pcl-danger);
}

.pcl-local-status__dot.ok {
  background: #34d399;
}

.pcl-local-path {
  overflow: hidden;
  color: var(--pcl-text);
  font: 12px/1.5 ui-monospace, Consolas, monospace;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-login-dialog {
  overflow: visible;
  background: var(--pcl-c6) !important;
}

.pcl-login-body {
  display: flex;
  flex-direction: column;
  gap: 10px;
  overflow: visible;
}

.pcl-login-field {
  display: flex;
  flex-direction: column;
  gap: 4px;
  color: var(--pcl-muted);
  font-size: 12px;
}

.pcl-login-field .pcl-input,
.pcl-login-field .pcl-select {
  width: 100%;
}

.pcl-login-device {
  display: flex;
  flex-direction: column;
  gap: 7px;
  padding: 12px;
  border: 1px solid color-mix(in srgb, var(--pcl-c1) 45%, transparent);
  border-radius: 6px;
  background: color-mix(in srgb, var(--pcl-c1) 10%, transparent);
}

.pcl-login-device__label {
  color: var(--pcl-muted);
  font-size: 12px;
}

.pcl-login-device__code {
  color: var(--pcl-text);
  font: 700 24px/1.2 ui-monospace, Consolas, monospace;
  letter-spacing: 4px;
}

.pcl-login-device__url {
  overflow-wrap: anywhere;
  color: var(--pcl-muted);
  font: 12px/1.5 ui-monospace, Consolas, monospace;
}

.pcl-local-network {
  display: flex;
  flex-direction: column;
  gap: 9px;
  padding: 12px;
  border: 1px solid var(--pcl-border);
  border-radius: 5px;
  background: var(--pcl-c8);
}

.pcl-local-network__head,
.pcl-local-users__head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
}

.pcl-local-network__head strong,
.pcl-local-users__head strong {
  color: var(--pcl-text);
  font-size: 12px;
}

.pcl-local-network__grid {
  display: grid;
  grid-template-columns: 130px 1fr;
  gap: 10px;
}

.pcl-local-users {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-top: 10px;
  padding: 12px;
  border: 1px solid var(--pcl-border);
  border-radius: 5px;
  background: var(--pcl-c8);
}

.pcl-local-users__list {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.pcl-local-users__row {
  display: flex;
  align-items: center;
  gap: 7px;
  min-height: 30px;
  padding: 0 5px;
  border-bottom: 1px solid var(--pcl-border);
  color: var(--pcl-text);
  font-size: 11px;
}

.pcl-local-users__row span {
  flex: 1;
}

.pcl-local-users__row small {
  color: var(--pcl-muted);
  font-size: 9px;
}

@media (max-width: 700px) {
  .pcl-local-network__grid { grid-template-columns: 1fr; }
}

.pcl-login-progress {
  display: flex;
  flex-direction: column;
  gap: 5px;
  margin: 2px 0 4px;
  padding: 8px 10px;
  border: 1px solid var(--pcl-border);
  border-radius: 5px;
  background: var(--pcl-c8);
}

.pcl-login-progress__head {
  display: flex;
  align-items: center;
  gap: 7px;
  color: var(--pcl-text);
  font-size: 11px;
}

.pcl-login-progress__step {
  display: flex;
  align-items: center;
  gap: 6px;
  min-width: 0;
  color: var(--pcl-muted);
  font-size: 10px;
}

.pcl-login-progress__step > span:nth-child(2) {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-login-progress__step small {
  margin-left: auto;
  color: var(--pcl-c5);
  font-size: 9px;
}

.pcl-login-progress__dot {
  flex: 0 0 5px;
  width: 5px;
  height: 5px;
  border-radius: 50%;
  background: var(--pcl-c5);
}

.pcl-login-progress__dot.running { background: var(--pcl-c3); }
.pcl-login-progress__dot.succeeded { background: #34d399; }
.pcl-login-progress__dot.failed { background: #fb7185; }
</style>
