<template>
  <q-dialog :model-value="true" persistent>
    <q-card class="neo-setup-card" :class="{ 'pcl-dialog': isPcl }" :data-setup-stage="stage">
      <q-card-section class="neo-setup-header">
        <div class="neo-setup-brand">
          <img :src="launcherLogo" alt="Luna Launcher" width="44" height="44" />
          <div>
            <div class="neo-setup-kicker">LUNA LAUNCHER</div>
            <h2>首次启动设置</h2>
          </div>
        </div>
        <p>先完成数据迁移和常用设置，之后可以在设置页继续调整。</p>
        <div class="neo-setup-progress" role="progressbar" aria-label="设置进度" :aria-valuenow="progress" aria-valuemin="0" aria-valuemax="100"><span :style="{ width: `${progress}%` }" /></div>
      </q-card-section>

      <q-card-section ref="body" class="neo-setup-body">
        <div v-if="working && working !== 'login'" class="setup-working">
          <PclLoading v-if="isPcl" inline :text="workingText" />
          <template v-else><q-spinner color="primary" size="22px" /><span>{{ workingText }}</span></template>
        </div>
        <div v-if="stage === 'welcome'">
          <h3>欢迎使用 NeoUI</h3>
          <p class="muted">NeoUI 和 Classic UI 共用同一份启动器数据。检测到旧版数据时，迁移会使用启动器原生的复制任务，并保留原目录。</p>
          <component :is="controls.hint" v-if="status?.dataRoot" v-bind="hintProps()" class="setup-banner">
            当前数据目录：<code>{{ status.dataRoot }}</code>
          </component>
          <component :is="controls.hint" v-if="status?.portable" v-bind="hintProps()" class="setup-banner">当前为 portable 模式，数据会保存在便携目录中。</component>
        </div>

        <div v-else-if="stage === 'migration'">
          <h3>迁移旧数据</h3>
          <p class="muted">可以把 Prism Launcher、PolyMC 或 MultiMC 的实例、账户、资源和设置迁移到当前数据目录。</p>
          <div class="migration-list">
            <div v-for="item in migrations" :key="item.name" class="migration-item">
              <div class="migration-details">
                <strong>{{ item.name }}</strong>
                <div class="muted migration-path">{{ item.source }}</div>
                <div v-if="item.currentDataExists" class="setup-warning">当前目录已有配置，无法自动覆盖；请手动合并。</div>
              </div>
              <div class="migration-actions">
                <component :is="controls.button" v-bind="actionProps(true, Boolean(working) || item.currentDataExists)" @click="migrate(item)">迁移</component>
                <component :is="controls.button" v-bind="actionProps()" @click="dismiss(item)">跳过</component>
              </div>
            </div>
          </div>
          <component :is="controls.hint" v-if="!migrations.length" v-bind="hintProps()" class="setup-banner">没有待处理的旧数据。</component>
        </div>

        <div v-else-if="stage === 'preferences'">
          <h3>常用设置</h3>
          <p class="muted">这些选项对应 Classic UI 首次启动向导中的语言、Java 和外观设置，之后都可以在设置页修改。</p>
          <fieldset class="setup-group">
            <legend>语言与外观</legend>
            <div class="setup-grid">
              <div class="setup-field">
                <span>界面语言</span>
                <component :is="controls.select" v-model="language" v-bind="selectProps" :options="languageOptions" aria-label="界面语言" />
              </div>
              <component :is="controls.checkbox" v-model="useSystemLocale" v-bind="checkboxProps" label="跟随系统语言" />
              <div class="setup-field" data-setup-field="preset">
                <span>NeoUI 外观预设</span>
                <component :is="controls.select" :model-value="preferences.pclStylePreset" v-bind="selectProps" :options="presetOptions" aria-label="NeoUI 外观预设" @update:model-value="selectPreset" />
              </div>
              <div class="setup-field" data-setup-field="color">
                <span>NeoUI 颜色模式</span>
                <component :is="controls.select" v-model="preferences.theme" v-bind="selectProps" :options="colorOptions" aria-label="NeoUI 颜色模式" />
              </div>
              <div class="setup-field">
                <span>Classic UI 应用主题</span>
                <component :is="controls.select" v-model="theme" v-bind="selectProps" :options="themeOptions" aria-label="Classic UI 应用主题" />
              </div>
              <div class="setup-field">
                <span>Classic UI 图标主题</span>
                <component :is="controls.select" v-model="icons" v-bind="selectProps" :options="iconOptions" aria-label="Classic UI 图标主题" />
              </div>
            </div>
          </fieldset>
          <fieldset class="setup-group">
            <legend>Java 与内存</legend>
            <div class="setup-grid">
              <component :is="controls.checkbox" v-model="automaticJavaSwitch" v-bind="checkboxProps" label="按实例自动选择 Java" />
              <component :is="controls.checkbox" v-model="automaticJavaDownload" v-bind="checkboxProps" :disabled="Boolean(working) || status?.javaDownloaderEnabled === false" :disable="Boolean(working) || status?.javaDownloaderEnabled === false" label="需要时自动下载 Java" />
              <div v-if="!automaticJavaDownload" class="setup-wide setup-java">
                <div class="setup-field">
                  <label for="setup-java-path">Java 可执行文件</label>
                  <div class="setup-java-path">
                    <component :is="controls.input" v-model="javaPath" v-bind="inputFieldProps('setup-java-path')" placeholder="javaw.exe 或 PATH 中的 java" />
                    <component :is="controls.button" v-bind="actionProps()" @click="browseJava">浏览</component>
                    <component :is="controls.button" v-bind="actionProps()" @click="scanJava">扫描本机</component>
                  </div>
                </div>
                <div v-if="javaOptions.length" class="setup-field">
                  <span>已检测的 Java</span>
                  <component :is="controls.select" v-model="javaPath" v-bind="selectProps" :options="javaOptions" aria-label="已检测的 Java" />
                </div>
                <component :is="controls.checkbox" v-model="ignoreJavaWizard" v-bind="checkboxProps" label="稍后配置 Java" />
              </div>
              <div class="setup-field">
                <label for="setup-min-memory">最小内存（MiB）</label>
                <component :is="controls.input" v-model.number="minMemory" v-bind="inputFieldProps('setup-min-memory')" type="number" min="128" step="128" />
              </div>
              <div class="setup-field">
                <label for="setup-max-memory">最大内存（MiB）</label>
                <component :is="controls.input" v-model.number="maxMemory" v-bind="inputFieldProps('setup-max-memory')" type="number" min="128" step="128" />
              </div>
            </div>
          </fieldset>
          <fieldset class="setup-group">
            <legend>日志上传</legend>
            <div class="setup-grid">
              <div class="setup-field">
                <span>日志上传服务</span>
                <component :is="controls.select" v-model="pasteType" v-bind="selectProps" :options="pasteOptions" aria-label="日志上传服务" />
              </div>
              <div v-if="pasteType === 0" class="setup-field">
                <label for="setup-paste-base">自定义日志服务地址</label>
                <component :is="controls.input" v-model="pasteBase" v-bind="inputFieldProps('setup-paste-base')" placeholder="留空使用默认地址" />
              </div>
            </div>
          </fieldset>
          <component :is="controls.hint" v-bind="hintProps()" class="setup-banner">账户登录、下载核心、镜像源和完整性校验可以在设置页中继续配置。</component>
        </div>

        <div v-else-if="stage === 'account'">
          <h3>添加账户</h3>
          <p v-if="hasAccounts" class="muted">已有账户已就绪，可以继续完成设置。你也可以再添加一个 Microsoft 账户。</p>
          <p v-else class="muted">登录拥有 Minecraft 的 Microsoft 账户后即可开始游戏。也可以稍后在账户页添加账户。</p>
          <component :is="controls.button" v-if="loginAvailable" class="setup-login-button" v-bind="actionProps(true)" @click="login">Microsoft 登录</component>
          <component :is="controls.hint" v-else v-bind="hintProps()" class="setup-banner">当前构建未配置 Microsoft 登录。其他账户类型可以在账户页管理。</component>
          <component :is="controls.hint" v-if="loginTask" v-bind="hintProps()" class="setup-banner">
            <div>{{ loginTask.status || '正在等待登录…' }}</div>
            <div v-if="loginTask.deviceCode" class="setup-device-code">{{ loginTask.deviceCode }}</div>
            <div v-if="loginTask.deviceUrl" class="setup-login-actions">
              <component :is="controls.button" v-bind="actionProps(false, false)" @click="openLoginUrl">打开登录网页</component>
              <component :is="controls.button" v-if="loginTask.deviceCode" v-bind="actionProps(false, false)" @click="copyLoginCode">复制代码</component>
            </div>
            <component :is="controls.button" v-bind="actionProps(false, false)" @click="cancelLogin">取消登录</component>
          </component>
        </div>

        <div v-else>
          <h3>设置完成</h3>
          <p class="muted">NeoUI 已准备好。你可以从首页导入整合包、管理实例，或随时回到设置页修改这些选项。</p>
          <div class="finish-icon"><PclIcon v-if="isPcl" name="circle-check" :size="56" /><q-icon v-else name="check_circle" size="56px" /></div>
        </div>

        <component :is="controls.hint" v-if="error" v-bind="hintProps('red')" class="setup-banner setup-error" role="alert">
          {{ error }}
          <component :is="controls.button" v-if="!preferencesLoaded" v-bind="actionProps()" @click="reloadPreferences">重试读取设置</component>
        </component>
      </q-card-section>

      <q-card-actions align="between" class="neo-setup-actions">
        <component :is="controls.button" v-if="stage !== 'welcome'" v-bind="actionProps()" @click="back">上一步</component>
        <span v-else />
        <component :is="controls.button" v-if="stage !== 'finish'" data-setup-next v-bind="actionProps(true, Boolean(working) || !preferencesLoaded || (stage === 'migration' && migrations.length > 0))" @click="next">{{ nextLabel }}</component>
        <component :is="controls.button" v-else data-setup-next v-bind="actionProps(true)" @click="complete">开始使用</component>
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>
<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { QBanner, QBtn, QInput, QSelect, QToggle } from 'quasar';
import { open } from '@tauri-apps/plugin-dialog';
import { applyPclStylePreset, neoUiIsDark, neoUiPreferences } from '../neo-ui-preferences';
import launcherLogo from '../assets/luna-launcher.png';
import PclButton from '../pcl/components/PclButton.vue';
import PclCheckbox from '../pcl/components/PclCheckbox.vue';
import PclHint from '../pcl/components/PclHint.vue';
import PclIcon from '../pcl/components/PclIcon.vue';
import PclInput from '../pcl/components/PclInput.vue';
import PclLoading from '../pcl/components/PclLoading.vue';
import PclSelect from '../pcl/components/PclSelect.vue';
import { setNeoLocale } from '../i18n';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ status: any; execute: Execute }>();
const emit = defineEmits<{ complete: []; changed: [] }>();

const stage = ref<'welcome' | 'migration' | 'preferences' | 'account' | 'finish'>('welcome');
const migrations = ref<any[]>([]);
const working = ref('loading');
const error = ref('');
const preferencesLoaded = ref(false);
const language = ref('en_US');
const useSystemLocale = ref(false);
const automaticJavaSwitch = ref(true);
const automaticJavaDownload = ref(true);
const javaPath = ref('');
const ignoreJavaWizard = ref(false);
const javaOptions = ref<any[]>([]);
const minMemory = ref(512);
const maxMemory = ref(4096);
const theme = ref('');
const icons = ref('');
const pasteType = ref(3);
const pasteBase = ref('');
const hasAccounts = ref(Boolean(props.status?.hasAccounts));
const loginAvailable = ref(Boolean(props.status?.loginAvailable));
const loginTask = ref<any>(null);
let disposed = false;
const preferences = neoUiPreferences;
const body = ref<{ $el: HTMLElement } | null>(null);
const isPcl = computed(() => preferences.shell === 'pcl');
const controls = computed(() => isPcl.value
  ? { button: PclButton, input: PclInput, select: PclSelect, checkbox: PclCheckbox, hint: PclHint }
  : { button: QBtn, input: QInput, select: QSelect, checkbox: QToggle, hint: QBanner });
const inputProps = computed(() => isPcl.value
  ? { disabled: Boolean(working.value) }
  : { disable: Boolean(working.value), dark: neoUiIsDark.value, outlined: true, dense: true });
const selectProps = computed(() => isPcl.value ? inputProps.value : { ...inputProps.value, emitValue: true, mapOptions: true });
const checkboxProps = computed(() => isPcl.value
  ? { disabled: Boolean(working.value) }
  : { disable: Boolean(working.value), dark: neoUiIsDark.value });
const workingText = computed(() => ({ loading: '正在读取设置…', saving: '正在保存设置…', java: '正在扫描 Java…', complete: '正在完成设置…' }[working.value] || '正在处理旧数据…'));
function actionProps(primary = false, disabled = Boolean(working.value)) {
  return isPcl.value ? { variant: primary ? 'highlight' : 'ghost', disabled }
    : { color: primary ? 'primary' : undefined, unelevated: primary, flat: !primary, disable: disabled, noCaps: true };
}
function inputFieldProps(id: string) {
  return { ...inputProps.value, ...(isPcl.value ? { id } : { for: id }) };
}
function hintProps(theme: 'blue' | 'red' = 'blue') {
  return isPcl.value ? { theme } : { rounded: true };
}
function selectPreset(value: string | number) {
  if (value === 'luna-neo' || value === 'pcl') applyPclStylePreset(value);
}
watch(stage, async () => {
  await nextTick();
  body.value?.$el.scrollTo({ top: 0 });
});
watch(error, async value => {
  if (!value) return;
  await nextTick();
  body.value?.$el.querySelector('.setup-error')?.scrollIntoView({ block: 'nearest' });
});
const presetOptions = [{ label: 'Luna Neo（默认）', value: 'luna-neo' }, { label: 'PCL', value: 'pcl' }];
const colorOptions = [{ label: '深色', value: 'dark' }, { label: '浅色', value: 'light' }, { label: '跟随系统', value: 'system' }];
const pasteOptions = [
  { label: '0x0.st', value: 0 },
  { label: 'Hastebin', value: 1 },
  { label: 'paste.gg', value: 2 },
  { label: 'mclo.gs', value: 3 },
];
const languageOptions = ref<any[]>([]);
const themeOptions = ref<any[]>([]);
const iconOptions = ref<any[]>([]);

const progress = computed(() => ({ welcome: 20, migration: 40, preferences: 60, account: 80, finish: 100 }[stage.value]));
const nextLabel = computed(() => stage.value === 'welcome' ? (migrations.value.length ? '检查迁移' : '开始设置') : '继续');

async function loadPreferences() {
    preferencesLoaded.value = false;
    const [settings, languageResult, appearance] = await Promise.all([
      props.execute('settings.list', { scope: 'launcher' }),
      props.execute('language.list'),
      props.execute('appearance.catalog'),
    ]);
    const setting = (key: string) => settings.find((item: any) => item.key === key)?.value;
    languageOptions.value = (languageResult?.languages || []).map((item: any) => ({ label: item.name || item.id, value: item.id }));
    language.value = languageResult?.selected || String(setting('Language') || 'en_US');
    useSystemLocale.value = Boolean(languageResult?.useSystemLocale ?? setting('UseSystemLocale'));
    themeOptions.value = (appearance?.themes || []).map((item: any) => ({ label: item.name || item.id, value: item.id }));
    iconOptions.value = (appearance?.icons || []).map((item: any) => ({ label: item.name || item.id, value: item.id }));
    const validSelection = (options: any[], value: string, fallback: string) =>
      options.find(item => item.value === value)?.value || options.find(item => item.value === fallback)?.value || options[0]?.value || '';
    theme.value = validSelection(themeOptions.value, appearance?.selected?.theme || String(setting('ApplicationTheme') || ''), 'dark');
    icons.value = validSelection(iconOptions.value, appearance?.selected?.icons || String(setting('IconTheme') || ''), 'pe_colored');
  automaticJavaSwitch.value = Boolean(setting('AutomaticJavaSwitch') ?? true);
  automaticJavaDownload.value = props.status?.javaDownloaderEnabled !== false && Boolean(setting('AutomaticJavaDownload') ?? true);
  javaPath.value = String(setting('JavaPath') || '');
  ignoreJavaWizard.value = Boolean(setting('IgnoreJavaWizard'));
  minMemory.value = Number(setting('MinMemAlloc') || 512);
  maxMemory.value = Number(setting('MaxMemAlloc') || 4096);
  pasteType.value = Number(setting('PastebinType') ?? 3);
  pasteBase.value = String(setting('PastebinCustomAPIBase') || '');
  // Classic's paste-service page upgrades the old URL setting too.
  const oldPasteUrl = String(setting('PastebinURL') || '');
  if (oldPasteUrl && oldPasteUrl !== 'https://0x0.st') { pasteType.value = 0; pasteBase.value = oldPasteUrl; }
  preferencesLoaded.value = true;
}

async function load() {
  migrations.value = Array.isArray(props.status?.migrations) ? [...props.status.migrations] : [];
  await reloadPreferences();
}

async function reloadPreferences() {
  working.value = 'loading'; error.value = '';
  try { await loadPreferences(); }
  catch (exception) { error.value = String(exception); }
  finally { working.value = ''; }
}

async function migrate(item: any) {
  working.value = item.name;
  error.value = '';
  try {
    const result = await props.execute('startup.migrate', { name: item.name });
    migrations.value = Array.isArray(result?.status?.migrations)
      ? result.status.migrations
      : migrations.value.filter(candidate => candidate.name !== item.name);
    hasAccounts.value = Boolean(result?.status?.hasAccounts);
    loginAvailable.value = Boolean(result?.status?.loginAvailable);
    await loadPreferences();
    emit('changed');
  } catch (exception) {
    error.value = String(exception);
  } finally {
    working.value = '';
  }
}

async function dismiss(item: any) {
  working.value = item.name;
  error.value = '';
  try {
    const result = await props.execute('startup.dismiss-migration', { name: item.name });
    migrations.value = Array.isArray(result?.status?.migrations)
      ? result.status.migrations
      : migrations.value.filter(candidate => candidate.name !== item.name);
    emit('changed');
  } catch (exception) {
    error.value = String(exception);
  } finally {
    working.value = '';
  }
}

async function saveSetting(key: string, value: unknown) {
  await props.execute('settings.set', { scope: 'launcher', key, value });
}

async function savePreferences() {
    const minimum = Number(minMemory.value), maximum = Number(maxMemory.value);
    if (!Number.isInteger(minimum) || !Number.isInteger(maximum) || minimum < 128 || maximum < minimum)
      throw new Error('内存必须为整数，最小值至少为 128 MiB，最大内存不能小于最小内存。');
    if (!automaticJavaDownload.value && !ignoreJavaWizard.value) {
      if (!javaPath.value.trim()) throw new Error('请选择 Java，启用自动下载，或选择稍后配置。');
      const diagnostic = await props.execute('java.diagnose', { scope: 'launcher', path: javaPath.value.trim() });
      if (!diagnostic?.usable) throw new Error('所选 Java 无法运行，请重新选择，或启用自动下载。');
      await props.execute('java.select', { scope: 'launcher', path: javaPath.value.trim() });
    }
    await saveSetting('AutomaticJavaSwitch', automaticJavaSwitch.value);
    await saveSetting('AutomaticJavaDownload', automaticJavaDownload.value);
    await saveSetting('IgnoreJavaWizard', ignoreJavaWizard.value);
    await saveSetting('UserAskedAboutAutomaticJavaDownload', true);
    await saveSetting('MinMemAlloc', minimum);
    await saveSetting('MaxMemAlloc', maximum);
    await saveSetting('PastebinType', Number(pasteType.value));
    await saveSetting('PastebinCustomAPIBase', pasteBase.value.trim());
    await props.execute('settings.reset', { scope: 'launcher', key: 'PastebinURL' });
    await props.execute('language.select', { language: language.value, useSystemLocale: useSystemLocale.value });
    setNeoLocale(useSystemLocale.value ? navigator.language : language.value);
    if (theme.value || icons.value) await props.execute('appearance.select', { ...(theme.value ? { theme: theme.value } : {}), ...(icons.value ? { icons: icons.value } : {}) });
    emit('changed');
}

async function next() {
  error.value = '';
  if (stage.value === 'welcome') {
    stage.value = migrations.value.length ? 'migration' : 'preferences';
  } else if (stage.value === 'migration') {
    if (migrations.value.length) return;
    stage.value = 'preferences';
  } else if (stage.value === 'preferences') {
    working.value = 'saving';
    try { await savePreferences(); stage.value = 'account'; }
    catch (exception) { error.value = String(exception); }
    finally { working.value = ''; }
  } else if (stage.value === 'account') {
    stage.value = 'finish';
  }
}

function back() {
  if (stage.value === 'migration') stage.value = 'welcome';
  else if (stage.value === 'preferences') stage.value = migrations.value.length ? 'migration' : 'welcome';
  else if (stage.value === 'account') stage.value = 'preferences';
  else if (stage.value === 'finish') stage.value = 'account';
}

async function browseJava() {
  try {
    const path = await open({ title: '选择 Java 可执行文件', multiple: false, directory: false });
    if (typeof path === 'string') javaPath.value = path;
  } catch (exception) { error.value = String(exception); }
}

async function scanJava() {
  working.value = 'java'; error.value = '';
  try {
    const installed = await props.execute('java.refresh');
    javaOptions.value = installed.map((item: any) => ({ label: `${item.version || 'Java'} · ${item.path}`, value: item.path }));
    if (!javaPath.value && javaOptions.value.length) javaPath.value = javaOptions.value[0].value;
    if (!javaOptions.value.length) error.value = '未检测到 Java，可手动选择或启用自动下载。';
  } catch (exception) { error.value = String(exception); }
  finally { working.value = ''; }
}

async function login() {
  working.value = 'login'; error.value = '';
  try {
    loginTask.value = await props.execute('neo.task.start', { operation: 'account.login', priority: 'high', parameters: { type: 'microsoft', useDeviceCode: true } });
    if (disposed) { await cancelLogin(); return; }
    while (!disposed && loginTask.value && !loginTask.value.finished) {
      await new Promise(resolve => setTimeout(resolve, 400));
      if (!disposed) loginTask.value = await props.execute('neo.task.status', { taskId: loginTask.value.taskId });
    }
    if (disposed) return;
    if (loginTask.value?.state !== 'succeeded') throw new Error(loginTask.value?.error || '登录已取消或失败，可以重试或稍后添加账户。');
    hasAccounts.value = true;
    emit('changed');
  } catch (exception) { error.value = String(exception); }
  finally { loginTask.value = null; working.value = ''; }
}

async function cancelLogin() {
  try { if (loginTask.value?.taskId) await props.execute('neo.task.cancel', { taskId: loginTask.value.taskId }); }
  catch (exception) { error.value = String(exception); }
}
async function openLoginUrl() {
  try { if (loginTask.value?.deviceUrl) await props.execute('desktop.open-url', { url: loginTask.value.deviceUrl }); }
  catch (exception) { error.value = String(exception); }
}
async function copyLoginCode() {
  try { await navigator.clipboard.writeText(loginTask.value?.deviceCode || ''); }
  catch (exception) { error.value = String(exception); }
}

async function complete() {
  working.value = 'complete';
  error.value = '';
  try {
    await props.execute('startup.complete');
    emit('complete');
  } catch (exception) {
    error.value = String(exception);
  } finally {
    working.value = '';
  }
}

onMounted(load);
onBeforeUnmount(() => { disposed = true; void cancelLogin(); });
</script>

<style scoped>
.neo-setup-card {
  --setup-bg: var(--neo-panel);
  --setup-text: var(--neo-text);
  --setup-muted: var(--neo-muted);
  --setup-border: var(--neo-border);
  --setup-accent: var(--neo-primary);
  display: flex;
  flex-direction: column;
  width: 760px;
  max-width: 94vw;
  max-height: 94vh;
  overflow: hidden;
  color: var(--setup-text);
  background: var(--setup-bg);
  border: 1px solid var(--setup-border);
  border-radius: 18px;
}
.neo-setup-card.pcl-dialog {
  --setup-bg: var(--pcl-white);
  --setup-text: var(--pcl-text);
  --setup-muted: var(--pcl-muted);
  --setup-border: var(--pcl-border);
  --setup-accent: var(--pcl-c3);
  font-family: var(--pcl-font);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  border-radius: var(--pcl-radius);
  box-shadow: var(--pcl-shadow);
  overflow: hidden;
}
.neo-setup-header { padding: 24px 28px 20px; background: linear-gradient(135deg, color-mix(in srgb, var(--setup-accent) 12%, transparent), transparent); border-bottom: 1px solid var(--setup-border); }
.neo-setup-brand { display: flex; align-items: center; gap: 14px; margin-bottom: 12px; }
.neo-setup-brand img { flex-shrink: 0; object-fit: contain; }
.neo-setup-kicker { color: var(--setup-accent); font-size: .8em; letter-spacing: .14em; font-weight: 700; }
.neo-setup-header h2 { margin: 4px 0 0; color: var(--setup-text); font-size: 24px; font-weight: 600; line-height: 1.4; }
.neo-setup-header p, .muted { margin: 0; color: var(--setup-muted); line-height: 1.7; }
.neo-setup-progress { height: 3px; margin-top: 18px; overflow: hidden; border-radius: 3px; background: color-mix(in srgb, var(--setup-accent) 16%, transparent); }
.neo-setup-progress span { display: block; height: 100%; background: var(--setup-accent); transition: width .25s ease; }
.neo-setup-body { min-height: 0; overflow-y: auto; overflow-x: hidden; flex: 1 1 auto; padding: 24px 28px; }
.neo-setup-header, .neo-setup-actions { flex-shrink: 0; }
.neo-setup-body h3 { margin: 0 0 10px; font-size: 21px; line-height: 1.4; }
.setup-working { display: flex; align-items: center; gap: 8px; margin-bottom: 16px; color: var(--setup-accent); }
.setup-banner { margin-top: 18px; overflow-wrap: anywhere; }
.setup-banner:not(.pcl-hint) { background: color-mix(in srgb, var(--setup-accent) 10%, var(--setup-bg)); color: var(--setup-text); border-left: 3px solid var(--setup-accent); }
.setup-error:not(.pcl-hint) { color: var(--q-negative); border-left-color: var(--q-negative); background: color-mix(in srgb, var(--q-negative) 10%, var(--setup-bg)); }
.setup-banner code { overflow-wrap: anywhere; }
.migration-list { margin-top: 20px; border: 1px solid var(--setup-border); border-radius: var(--pcl-radius, 10px); }
.migration-item { display: flex; align-items: center; gap: 16px; padding: 16px; }
.migration-item + .migration-item { border-top: 1px solid var(--setup-border); }
.migration-details { flex: 1; min-width: 0; }
.migration-path { overflow-wrap: anywhere; font-size: .9em; }
.migration-actions { display: flex; align-items: center; flex-shrink: 0; gap: 8px; }
.setup-warning { color: var(--pcl-warning, var(--q-warning)); line-height: 1.7; }
.setup-group { min-width: 0; margin: 24px 0 0; padding: 0; border: 0; }
.setup-group legend { width: 100%; padding: 0 0 8px; border-bottom: 1px solid var(--setup-border); color: var(--setup-accent); font-size: 1em; font-weight: 600; }
.setup-grid { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); align-items: center; gap: 16px; margin-top: 16px; }
.setup-field { display: flex; flex-direction: column; min-width: 0; gap: 8px; }
.setup-wide { grid-column: 1 / -1; }
.setup-java { display: flex; flex-direction: column; gap: 16px; }
.setup-java-path { display: flex; align-items: center; flex-wrap: wrap; gap: 8px; }
.setup-java-path > :first-child { flex: 1 1 200px; min-width: 0; }
.setup-device-code { margin: 18px 0; font: 600 26px monospace; letter-spacing: .12em; }
.setup-login-button { margin-top: 24px; }
.setup-login-actions { display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 8px; }
.finish-icon { display: flex; justify-content: center; margin: 40px auto 20px; color: var(--setup-accent); }
.neo-setup-actions { gap: 12px; padding: 16px 28px; border-top: 1px solid var(--setup-border); background: var(--setup-bg); }
@media (max-width: 640px) {
  .setup-grid { grid-template-columns: minmax(0, 1fr); }
  .neo-setup-body { padding: 20px; }
  .neo-setup-header { padding: 20px 20px 16px; }
  .neo-setup-actions { padding: 16px 20px; }
  .migration-item { align-items: flex-start; flex-direction: column; }
}
</style>
