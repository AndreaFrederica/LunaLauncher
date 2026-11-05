<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef-settings-page">
    <div class="ef-settings-page__scope">
      <button type="button" :class="{ current: scope === 'launcher' }" @click="scope = 'launcher'">
        <small>// 01</small>启动器设置
      </button>
      <button type="button" :class="{ current: scope === 'instance' }" :disabled="!selected" @click="scope = 'instance'">
        <small>// 02</small>实例设置
      </button>
      <span v-if="scope === 'instance' && selected" class="ef-settings-page__target">{{ selected.name }}</span>
    </div>

    <div class="ef-settings-page__layout">
      <nav class="ef-settings-nav" aria-label="设置分类">
        <button
          v-for="category in categories"
          :key="category.id"
          type="button"
          :class="{ current: active === category.id }"
          @click="active = category.id"
        >
          <span class="material-icons">{{ category.icon }}</span>
          <span>{{ category.label }}</span>
          <small v-if="countOf(category.id)">{{ countOf(category.id) }}</small>
        </button>
      </nav>

      <section class="ef-settings-content">
        <header class="ef-settings-content__head">
          <p class="ef-eyebrow">// {{ active.toUpperCase() }}</p>
          <h2>{{ activeInfo.label }}</h2>
          <p>{{ activeInfo.description }}</p>
        </header>

        <template v-if="active === 'appearance'">
          <EfCard title="启动器主题">
            <div class="ef-shell-cards">
              <button
                v-for="option in shellOptions"
                :key="option.id"
                type="button"
                class="ef-shell-card"
                :class="{ current: isCurrentShell(option) }"
                @click="setShell(option)"
              >
                <span class="ef-shell-card__preview" :class="`is-${option.preview}`" aria-hidden="true">
                  <i class="ef-shell-card__bar" />
                  <i class="ef-shell-card__side" />
                  <i class="ef-shell-card__block" />
                  <i class="ef-shell-card__line" />
                  <i class="ef-shell-card__line short" />
                </span>
                <span class="ef-shell-card__copy">
                  <strong>{{ option.label }}</strong>
                  <small>{{ option.description }}</small>
                </span>
                <span v-if="isCurrentShell(option)" class="ef-shell-card__mark">当前</span>
              </button>
            </div>
            <p class="ef-hint">切换后会立即重建整个启动器界面；每个主题的颜色、布局和部件都不同。</p>
          </EfCard>

          <EfCard title="明暗">
            <div class="ef-choice-row">
              <EfRadio v-for="option in themeOptions" :key="option.value" name="ef-theme" :value="option.value" :checked="theme === option.value" @change="setTheme">{{ option.label }}</EfRadio>
            </div>
            <p class="ef-hint">Endfield 固定使用信号黄作为唯一强调色；浅色与深色各是一套完整主题。</p>
          </EfCard>
          <EfCard title="界面">
            <EfCheckbox v-model="animations">启用界面动效</EfCheckbox>
            <p class="ef-hint">关闭后所有入场、擦入和进度动效立即结束；系统开启“减少动态效果”时同样生效。</p>
          </EfCard>
          <EfCard title="外观说明">
            <div class="ef-appearance-note">
              <span class="ef-appearance-swatch" aria-hidden="true" />
              <div>
                <strong>纸面、墨线、信号黄</strong>
                <p>正文与容器直角，可点击元素切角；黄色只表示当前位置、可点击内容和需要留意的事。</p>
              </div>
            </div>
          </EfCard>
        </template>

        <template v-else-if="active === 'memory'">
          <div class="ef-settings-stats">
            <EfStat label="总内存" :value="memory.totalMiB" unit="MiB" />
            <EfStat label="已用" :value="memory.usedMiB" unit="MiB" />
            <EfStat label="占用" :value="memoryRatio" unit="%" :alert="memoryRatio > 85" />
          </div>
          <EfCard title="内存分配">
            <EfSlider v-model="memoryMax" :min="512" :max="Math.max(4096, memory.totalMiB)" :step="256" suffix=" MiB" />
            <div class="ef-settings-actions">
              <EfButton variant="accent" :disabled="busy" @click="saveMemory">保存到{{ scope === 'instance' ? '实例' : '启动器' }}</EfButton>
              <EfButton icon="refresh" :disabled="busy" @click="loadMemory">刷新</EfButton>
            </div>
          </EfCard>
          <EfSettingRow v-for="setting in rows" :key="setting.key" :setting="setting" :busy="busy" @save="saveSetting" @reset="resetSetting" />
        </template>

        <template v-else-if="active === 'java'">
          <div class="ef-settings-actions">
            <EfButton icon="refresh" :disabled="busy" @click="loadJava">扫描 Java</EfButton>
            <EfButton icon="fact_check" :disabled="busy" @click="diagnoseJava">检测</EfButton>
            <EfButton icon="build" :disabled="busy" @click="repairJava">修复</EfButton>
          </div>
          <div class="ef-settings-list">
            <article class="ef-settings-item">
              <div>
                <strong>自动选择</strong>
                <small>由启动器根据 Minecraft 版本选择 Java</small>
              </div>
              <EfButton size="sm" :disabled="busy" @click="selectJava('')">使用</EfButton>
            </article>
            <article v-for="runtime in javaRuntimes" :key="runtime.path" class="ef-settings-item">
              <div>
                <strong>{{ runtime.version || runtime.name || runtime.path }}</strong>
                <small>{{ runtime.path }}</small>
              </div>
              <EfButton size="sm" variant="ghost" :disabled="busy" @click="selectJava(runtime.path)">选择</EfButton>
            </article>
          </div>
          <pre v-if="javaDiagnostic" class="ef-settings-log">{{ javaDiagnostic }}</pre>
          <EfSettingRow v-for="setting in rows" :key="setting.key" :setting="setting" :busy="busy" @save="saveSetting" @reset="resetSetting" />
        </template>

        <template v-else-if="active === 'network'">
          <EfCard title="代理">
            <div class="ef-settings-form">
              <label>类型<EfSelect v-model="proxy.type" :options="proxyTypes" /></label>
              <label v-if="proxy.type !== 'None'">主机<EfInput v-model="proxy.host" placeholder="127.0.0.1" /></label>
              <label v-if="proxy.type !== 'None'">端口<EfInput v-model="proxy.port" type="number" /></label>
            </div>
            <div class="ef-settings-actions">
              <EfButton variant="accent" :disabled="busy" @click="saveProxy">保存代理</EfButton>
              <EfButton icon="network_check" :disabled="busy" @click="testProxy">测试连接</EfButton>
            </div>
            <p v-if="proxyResult" class="ef-hint">{{ proxyResult }}</p>
          </EfCard>
          <EfSettingRow v-for="setting in rows" :key="setting.key" :setting="setting" :busy="busy" @save="saveSetting" @reset="resetSetting" />
        </template>

        <template v-else-if="active === 'paths'">
          <EfCard title="目录">
            <div class="ef-settings-path">
              <span>数据目录</span><code>{{ runtimeInfo.dataRoot || '—' }}</code>
              <EfButton size="sm" icon="folder_open" :disabled="busy" @click="openPath(runtimeInfo.dataRoot)">打开</EfButton>
            </div>
            <div class="ef-settings-path">
              <span>程序目录</span><code>{{ runtimeInfo.root || runtimeInfo.programRoot || '—' }}</code>
              <EfButton size="sm" icon="folder_open" :disabled="busy" @click="openPath(runtimeInfo.root || runtimeInfo.programRoot)">打开</EfButton>
            </div>
          </EfCard>
          <EfSettingRow v-for="setting in rows" :key="setting.key" :setting="setting" :busy="busy" @save="saveSetting" @reset="resetSetting" />
        </template>

        <template v-else-if="active === 'instance-stores'">
          <InstanceStoreWorkspace :execute="props.execute" variant="endfield" />
        </template>

        <template v-else-if="active === 'cache'">
          <div class="ef-settings-stats">
            <EfStat label="HTTP 缓存" :value="cache.httpSize ?? '—'" :unit="cache.httpSize !== undefined ? 'MB' : ''" />
            <EfStat label="认证缓存" :value="cache.authCount ?? '—'" note="账户条目" />
          </div>
          <div class="ef-settings-actions">
            <EfButton icon="refresh" :disabled="busy" @click="loadCache">刷新</EfButton>
            <EfButton variant="red" icon="delete_sweep" :disabled="busy" @click="clearCache">清理缓存</EfButton>
          </div>
          <EfSettingRow v-for="setting in rows" :key="setting.key" :setting="setting" :busy="busy" @save="saveSetting" @reset="resetSetting" />
        </template>

        <template v-else-if="active === 'language'">
          <EfCard title="界面语言">
            <EfSelect v-model="language" :options="languageOptions" placeholder="选择语言" />
            <div class="ef-settings-actions">
              <EfButton variant="accent" :disabled="busy || !language" @click="saveLanguage">应用语言</EfButton>
            </div>
          </EfCard>
          <EfSettingRow v-for="setting in rows" :key="setting.key" :setting="setting" :busy="busy" @save="saveSetting" @reset="resetSetting" />
        </template>

        <template v-else-if="active === 'update'">
          <EfCard title="启动器更新">
            <div class="ef-settings-list">
              <div class="ef-settings-path"><span>当前版本</span><code>{{ updateStatus.currentVersion || updateStatus.version || '—' }}</code></div>
              <div class="ef-settings-path"><span>通道</span><code>{{ updateStatus.channel || '—' }}</code></div>
              <div class="ef-settings-path"><span>状态</span><code>{{ updateStatus.status || updateStatus.state || '—' }}</code></div>
            </div>
            <div class="ef-settings-actions">
              <EfButton icon="refresh" :disabled="busy" @click="checkUpdate">检查更新</EfButton>
              <EfButton variant="accent" icon="system_update" :disabled="busy" @click="applyUpdate">安装更新</EfButton>
            </div>
          </EfCard>
          <EfSettingRow v-for="setting in rows" :key="setting.key" :setting="setting" :busy="busy" @save="saveSetting" @reset="resetSetting" />
        </template>

        <template v-else-if="active === 'about'">
          <EfCard title="LunaLauncher">
            <div class="ef-settings-list">
              <div class="ef-settings-path"><span>版本</span><code>{{ runtimeInfo.version || runtimeInfo.appVersion || '—' }}</code></div>
              <div class="ef-settings-path"><span>数据目录</span><code>{{ runtimeInfo.dataRoot || '—' }}</code></div>
              <div class="ef-settings-path"><span>程序目录</span><code>{{ runtimeInfo.root || runtimeInfo.programRoot || '—' }}</code></div>
              <div class="ef-settings-path"><span>实例</span><code>{{ instances.length }} 个</code></div>
            </div>
            <div class="ef-settings-actions">
              <EfButton icon="folder_open" :disabled="busy" @click="openPath(runtimeInfo.dataRoot)">打开数据目录</EfButton>
              <EfButton icon="article" :disabled="busy" @click="openPath(runtimeInfo.logs || runtimeInfo.dataRoot)">打开日志目录</EfButton>
            </div>
          </EfCard>
        </template>

        <template v-else-if="active === 'accounts'">
          <EfCard title="账户">
            <p class="ef-hint">账户的登录、默认账户与皮肤在账户页管理。</p>
            <div class="ef-settings-list">
              <div class="ef-settings-path"><span>账户数</span><code>{{ (accounts || []).length }}</code></div>
              <div class="ef-settings-path"><span>默认账户</span><code>{{ defaultAccountName }}</code></div>
            </div>
            <div class="ef-settings-actions">
              <EfButton variant="accent" icon="group" :disabled="busy" @click="emit('accounts')">打开账户页</EfButton>
            </div>
          </EfCard>
          <EfSettingRow v-for="setting in rows" :key="setting.key" :setting="setting" :busy="busy" @save="saveSetting" @reset="resetSetting" />
        </template>

        <template v-else-if="active === 'logs'">
          <EfCard title="日志">
            <div class="ef-settings-actions">
              <EfButton icon="folder_open" :disabled="busy" @click="openPath(runtimeInfo.logs || runtimeInfo.dataRoot)">打开日志目录</EfButton>
              <EfButton icon="description" :disabled="busy" @click="openPath(runtimeInfo.updateLog || runtimeInfo.logs || runtimeInfo.dataRoot)">打开更新日志</EfButton>
            </div>
            <p class="ef-hint">日志目录：{{ runtimeInfo.logs || runtimeInfo.dataRoot || '—' }}</p>
          </EfCard>
          <EfSettingRow v-for="setting in rows" :key="setting.key" :setting="setting" :busy="busy" @save="saveSetting" @reset="resetSetting" />
        </template>

        <template v-else-if="active === 'feedback'">
          <EfCard title="反馈与诊断">
            <p class="ef-hint">反馈问题时附上启动器日志和运行环境，能更快定位。</p>
            <div class="ef-settings-actions">
              <EfButton icon="content_copy" :disabled="busy" @click="copyDiagnostics">复制诊断信息</EfButton>
              <EfButton icon="folder_open" :disabled="busy" @click="openPath(runtimeInfo.logs || runtimeInfo.dataRoot)">打开日志目录</EfButton>
            </div>
            <pre v-if="diagnosticsCopied" class="ef-settings-log">{{ diagnosticsCopied }}</pre>
          </EfCard>
        </template>

        <template v-else>
          <div v-if="rows.length" class="ef-settings-list">
            <EfSettingRow v-for="setting in rows" :key="setting.key" :setting="setting" :busy="busy" @save="saveSetting" @reset="resetSetting" />
          </div>
          <EfEmpty v-else title="这一分类没有可调设置" description="后端还没有返回这个分类的设置项。" icon="tune" />
        </template>
      </section>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue';
import { applyPclStylePreset, neoUiPreferences, persistNeoUiPreferences } from '../../../neo-ui-preferences';
import { settingMetadata } from '../../../settings-schema';
import EfButton from '../components/EfButton.vue';
import EfCard from '../components/EfCard.vue';
import EfCheckbox from '../components/EfCheckbox.vue';
import EfEmpty from '../components/feedback/EfEmpty.vue';
import EfInput from '../components/EfInput.vue';
import EfRadio from '../components/controls/EfRadio.vue';
import EfSelect from '../components/EfSelect.vue';
import EfSlider from '../components/controls/EfSlider.vue';
import EfStat from '../components/data/EfStat.vue';
import EfSettingRow from '../components/data/EfSettingRow.vue';
import InstanceStoreWorkspace from '../../../components/InstanceStoreWorkspace.vue';

const props = defineProps<{
  busy: boolean;
  execute: (op: string, p?: Record<string, unknown>) => Promise<any>;
  settings: any[];
  selected?: any;
  instances?: any[];
  accounts?: any[];
}>();
const emit = defineEmits<{
  changed: [];
  accounts: [];
  toast: [type: 'success' | 'error' | 'info', title: string, message?: string];
}>();

type Category = { id: string; label: string; icon: string; description: string };
const launcherCategories: Category[] = [
  { id: 'appearance', label: '界面', icon: 'palette', description: '主题与动效。' },
  { id: 'accounts', label: '账户', icon: 'group', description: '账户登录与默认账户。' },
  { id: 'launch', label: '启动', icon: 'play_arrow', description: '游戏窗口、启动后行为与实例选择。' },
  { id: 'game', label: '游戏', icon: 'sports_esports', description: '模组依赖、全局数据包与兼容提示。' },
  { id: 'java', label: 'Java', icon: 'coffee', description: 'Java 运行时、参数与自动选择。' },
  { id: 'memory', label: '内存', icon: 'memory', description: '可用内存与分配上限。' },
  { id: 'paths', label: '路径', icon: 'folder', description: '数据目录、程序目录与外部工具路径。' },
  { id: 'instance-stores', label: '实例目录', icon: 'layers', description: '管理 Prism、PCL、HMCL 和原版实例目录。' },
  { id: 'network', label: '网络', icon: 'wifi', description: '代理、超时与下载连接。' },
  { id: 'downloads', label: '下载', icon: 'download', description: '下载来源、并发与缓存策略。' },
  { id: 'misc', label: '其他', icon: 'more_horiz', description: '下载回退、目录监听与杂项。' },
  { id: 'compatibility', label: '兼容性', icon: 'speed', description: 'GLFW、OpenAL、显卡与启动兼容。' },
  { id: 'console', label: '控制台', icon: 'terminal', description: '控制台显示与日志行为。' },
  { id: 'playtime', label: '游戏时间', icon: 'schedule', description: '游戏时间统计与显示。' },
  { id: 'language', label: '语言', icon: 'translate', description: '启动器界面语言。' },
  { id: 'update', label: '更新', icon: 'system_update', description: '检查与安装启动器更新。' },
  { id: 'cache', label: '缓存', icon: 'storage', description: '下载缓存与认证缓存。' },
  { id: 'qt-ui', label: 'Qt 界面', icon: 'desktop_windows', description: '旧版 Qt 界面设置。' },
  { id: 'advanced', label: '高级', icon: 'build', description: 'API、外部工具与实验性选项。' },
  { id: 'logs', label: '日志', icon: 'article', description: '启动器日志与更新日志。' },
  { id: 'about', label: '关于', icon: 'info', description: '版本、目录与运行环境。' },
  { id: 'feedback', label: '反馈', icon: 'bug_report', description: '诊断信息与日志位置。' },
];
const instanceCategories: Category[] = [
  { id: 'launch', label: '启动', icon: 'play_arrow', description: '这个实例的启动行为。' },
  { id: 'game', label: '游戏', icon: 'sports_esports', description: '实例的模组依赖与数据包。' },
  { id: 'java', label: 'Java', icon: 'coffee', description: '实例使用的 Java 运行时与参数。' },
  { id: 'memory', label: '内存', icon: 'memory', description: '实例内存分配。' },
  { id: 'paths', label: '路径', icon: 'folder', description: '实例与游戏目录。' },
  { id: 'compatibility', label: '兼容性', icon: 'speed', description: '渲染与启动兼容选项。' },
  { id: 'misc', label: '其他', icon: 'more_horiz', description: '下载回退与杂项。' },
  { id: 'console', label: '控制台', icon: 'terminal', description: '实例控制台显示。' },
  { id: 'playtime', label: '游戏时间', icon: 'schedule', description: '实例游戏时间统计。' },
  { id: 'instance', label: '实例', icon: 'inventory_2', description: '实例自身的设置。' },
  { id: 'instance-overrides', label: '覆盖项', icon: 'tune', description: '覆盖启动器默认值的设置。' },
  { id: 'advanced', label: '高级', icon: 'build', description: '实验性与高级选项。' },
  { id: 'logs', label: '日志', icon: 'article', description: '实例日志位置。' },
];

const scope = ref<'launcher' | 'instance'>('launcher');
const active = ref('appearance');
const instanceSettings = ref<any[]>([]);
const memory = ref({ totalMiB: 0, usedMiB: 0 });
const memoryMax = ref(4096);
const javaRuntimes = ref<any[]>([]);
const javaDiagnostic = ref('');
const proxy = ref({ type: 'None', host: '', port: 0 });
const proxyResult = ref('');
const cache = ref<any>({});
const runtimeInfo = ref<any>({});
const updateStatus = ref<any>({});
const language = ref('');
const languages = ref<any[]>([]);
const diagnosticsCopied = ref('');

const categories = computed(() => scope.value === 'launcher' ? launcherCategories : instanceCategories);
const activeInfo = computed(() => categories.value.find(category => category.id === active.value) || categories.value[0]);
const shellOptions = [
  { id: 'luna-neo', label: 'Luna Neo（默认）', shell: 'pcl', preset: 'luna-neo', preview: 'luna-neo', description: 'PCL Style 的 Luna Neo 预设，默认界面' },
  { id: 'pcl', label: 'PCL Style', shell: 'pcl', preset: 'pcl', preview: 'pcl', description: 'PCL 经典蓝白配色与启动页' },
  { id: 'endfield', label: '终末地', shell: 'endfield', preset: '', preview: 'endfield', description: '纸面、墨线与信号黄的工业界面' },
  { id: 'melody', label: '忘却的旋律', shell: 'melody', preset: '', preview: 'melody', description: '独立窗口与深色旋律界面' },
  { id: 'demo', label: 'Demo', shell: 'neo', preset: '', preview: 'demo', description: '已废弃的 XMCL 移植界面' },
];
const theme = computed(() => neoUiPreferences.theme);
const animations = computed({
  get: () => neoUiPreferences.animations,
  set: (value: boolean) => { neoUiPreferences.animations = value; persistNeoUiPreferences(); },
});
const themeOptions = [
  { label: '跟随系统', value: 'system' },
  { label: '浅色', value: 'light' },
  { label: '深色', value: 'dark' },
];
const proxyTypes = [
  { label: '不使用', value: 'None' },
  { label: 'HTTP', value: 'Http' },
  { label: 'SOCKS5', value: 'Socks5' },
];
const languageOptions = computed(() => languages.value.map((item: any) => ({
  label: item.name || item.displayName || item.locale || item.id,
  value: String(item.locale || item.id || item.code || item.name || ''),
})));
const memoryRatio = computed(() => memory.value.totalMiB > 0 ? Math.round(memory.value.usedMiB / memory.value.totalMiB * 100) : 0);
const currentSettings = computed(() => scope.value === 'launcher' ? props.settings : instanceSettings.value);
const instanceCount = computed(() => props.instances?.length || 0);

const legacyGroups: Record<string, string> = { Proxy: 'network', Launcher: 'qt-ui', ExternalTools: 'advanced', API: 'advanced' };
const keyGroups: Record<string, string> = {
  ModDependenciesDisabled: 'game',
  GlobalDataPacksEnabled: 'game',
  GlobalDataPacksPath: 'game',
  ModMetadataDisabled: 'game',
  SkipModpackUpdatePrompt: 'game',
  ShowModIncompat: 'game',
  FallbackMRBlockedMods: 'game',
  PclDownloadFallbackToQt: 'misc',
  Aria2FallbackToQt: 'misc',
  DownloadsDirWatchRecursive: 'misc',
};
function categoryOf(setting: any) {
  const fallback = settingMetadata[setting.key];
  return keyGroups[setting.key] || setting.category || legacyGroups[fallback?.group || ''] || fallback?.group || 'advanced';
}
const defaultAccountName = computed(() => {
  const account = (props.accounts || []).find(item => item.default);
  return account ? (account.profileName || account.displayName || '默认账户') : '未设置';
});
const rows = computed(() => currentSettings.value.filter(setting => categoryOf(setting) === active.value));
function countOf(category: string) {
  return currentSettings.value.filter(setting => categoryOf(setting) === category).length;
}

type ShellOption = typeof shellOptions[number];
function isCurrentShell(option: ShellOption) {
  if (neoUiPreferences.shell !== option.shell) return false;
  return option.shell !== 'pcl' || neoUiPreferences.pclStylePreset === option.preset;
}
function setShell(option: ShellOption) {
  if (isCurrentShell(option)) return;
  neoUiPreferences.shell = option.shell as any;
  if (option.preset) applyPclStylePreset(option.preset as any);
  else persistNeoUiPreferences();
}
function setTheme(value: string) {
  neoUiPreferences.theme = value as any;
  persistNeoUiPreferences();
}
async function loadInstanceSettings() {
  if (!props.selected?.id) { instanceSettings.value = []; return; }
  try {
    const result: any = await props.execute('settings.list', { scope: 'instance', instance: props.selected.id });
    instanceSettings.value = Array.isArray(result) ? result : (result?.entries || []);
  } catch { instanceSettings.value = []; }
}

async function saveSetting(key: string, value: unknown) {
  try {
    if (scope.value === 'instance') {
      if (!props.selected?.id) return;
      await props.execute('settings.set', { scope: 'instance', instance: props.selected.id, key, value });
      await loadInstanceSettings();
    } else {
      await props.execute('settings.set', { scope: 'launcher', key, value });
    }
    emit('toast', 'success', '设置已保存', key);
    emit('changed');
  } catch { /* shell toast */ }
}
async function resetSetting(key: string) {
  try {
    if (scope.value === 'instance') {
      if (!props.selected?.id) return;
      await props.execute('settings.set', { scope: 'instance', instance: props.selected.id, key, value: null });
      await loadInstanceSettings();
    } else {
      await props.execute('settings.set', { scope: 'launcher', key, value: null });
    }
    emit('toast', 'success', '设置已重置', key);
    emit('changed');
  } catch { /* shell toast */ }
}

async function loadMemory() {
  try {
    const result: any = await props.execute('runtime.memory', {});
    const data = result?.data || result || {};
    memory.value = { totalMiB: Number(data.totalMiB || 0), usedMiB: Number(data.usedMiB || 0) };
    const alloc = currentSettings.value.find(setting => setting.key === 'MaxMemAlloc');
    if (alloc && Number(alloc.value) > 0) memoryMax.value = Number(alloc.value);
  } catch { /* ignore */ }
}
async function saveMemory() {
  if (scope.value === 'instance' && !props.selected?.id) return;
  await saveSetting('MaxMemAlloc', memoryMax.value);
}
async function loadJava() {
  try {
    const result: any = await props.execute('java.refresh', {});
    javaRuntimes.value = Array.isArray(result) ? result : (result?.runtimes || []);
  } catch { javaRuntimes.value = []; }
}
async function selectJava(path: string) {
  try {
    await props.execute('java.select', scope.value === 'instance' && props.selected?.id ? { scope: 'instance', instance: props.selected.id, path } : { scope: 'launcher', path });
    emit('toast', 'success', path ? '已选择 Java' : '已切换为自动选择');
  } catch { /* shell toast */ }
}
async function diagnoseJava() {
  try {
    const result = await props.execute('java.diagnose', scope.value === 'instance' && props.selected?.id ? { scope: 'instance', instance: props.selected.id } : { scope: 'launcher' });
    javaDiagnostic.value = typeof result === 'string' ? result : JSON.stringify(result, null, 2);
  } catch { /* shell toast */ }
}
async function repairJava() {
  try {
    const result = await props.execute('java.repair', scope.value === 'instance' && props.selected?.id ? { scope: 'instance', instance: props.selected.id } : { scope: 'launcher' });
    javaDiagnostic.value = typeof result === 'string' ? result : JSON.stringify(result, null, 2);
    await loadJava();
    emit('toast', 'success', '修复完成');
  } catch { /* shell toast */ }
}
async function loadProxy() {
  try {
    const result: any = await props.execute('network.proxy.get');
    proxy.value = { type: result.type || 'None', host: result.host || '', port: Number(result.port || 0) };
  } catch { /* ignore */ }
}
async function saveProxy() {
  try {
    await props.execute('network.proxy.set', { type: proxy.value.type, host: proxy.value.host, port: proxy.value.port });
    emit('toast', 'success', '代理已保存');
  } catch { /* shell toast */ }
}
async function testProxy() {
  proxyResult.value = '测试中…';
  try {
    const result: any = await props.execute('network.proxy.test', { url: 'https://www.minecraft.net', timeoutMs: 10000 });
    proxyResult.value = result?.status ? `HTTP ${result.status} | ${result.timeMs ?? '—'} ms` : JSON.stringify(result);
  } catch (error: any) {
    proxyResult.value = error?.message || String(error);
  }
}
async function loadCache() {
  try {
    const [http, auth] = await Promise.allSettled([props.execute('launcher.cache.status'), props.execute('yggdrasil.cache.status')]);
    const httpData: any = http.status === 'fulfilled' ? http.value : {};
    const authData: any = auth.status === 'fulfilled' ? auth.value : {};
    cache.value = { httpSize: httpData.sizeMiB ?? httpData.totalMiB, authCount: authData.entries ?? authData.count };
  } catch { /* ignore */ }
}
async function clearCache() {
  if (!window.confirm('清理下载与认证缓存？')) return;
  try {
    await props.execute('launcher.cache.clear', { confirm: true });
    await props.execute('yggdrasil.cache.purge', { confirm: true }).catch(() => {});
    await loadCache();
    emit('toast', 'success', '缓存已清理');
  } catch { /* shell toast */ }
}
async function loadLanguages() {
  try {
    const result: any = await props.execute('language.list');
    languages.value = Array.isArray(result) ? result : (result?.languages || []);
    const current = languages.value.find((item: any) => item.current || item.selected);
    if (current) language.value = String(current.locale || current.id || current.code || '');
  } catch { /* ignore */ }
}
async function saveLanguage() {
  if (!language.value) return;
  try {
    await props.execute('language.select', { language: language.value });
    emit('toast', 'success', '语言已切换');
  } catch { /* shell toast */ }
}
async function loadRuntime() {
  try { runtimeInfo.value = await props.execute('runtime.info'); } catch { /* ignore */ }
}
async function loadUpdate() {
  try { updateStatus.value = await props.execute('launcher.update.status'); } catch { /* ignore */ }
}
async function checkUpdate() {
  try {
    updateStatus.value = await props.execute('launcher.update.check');
    emit('toast', 'success', '已检查更新');
  } catch { /* shell toast */ }
}
async function applyUpdate() {
  try {
    await props.execute('launcher.update.apply');
    emit('toast', 'success', '更新已开始');
  } catch { /* shell toast */ }
}
async function openPath(path: string) {
  if (!path) return;
  try { await props.execute('desktop.open-path', { path }); } catch { /* shell toast */ }
}
async function copyDiagnostics() {
  const info = { runtime: runtimeInfo.value, memory: memory.value, cache: cache.value, update: updateStatus.value };
  diagnosticsCopied.value = JSON.stringify(info, null, 2);
  try { await navigator.clipboard.writeText(diagnosticsCopied.value); emit('toast', 'success', '诊断信息已复制'); } catch { /* clipboard may be unavailable */ }
}

function ensureCategory() {
  if (!categories.value.some(category => category.id === active.value)) active.value = categories.value[0].id;
}

watch(scope, () => {
  ensureCategory();
  if (scope.value === 'instance') void loadInstanceSettings();
});
watch(() => props.selected?.id, () => { if (scope.value === 'instance') void loadInstanceSettings(); });
watch(active, value => {
  if (value === 'memory') void loadMemory();
  else if (value === 'java') void loadJava();
  else if (value === 'network') void loadProxy();
  else if (value === 'cache') void loadCache();
  else if (value === 'language') void loadLanguages();
  else if (value === 'update') void loadUpdate();
  else if (value === 'about' || value === 'paths' || value === 'feedback') void loadRuntime();
}, { immediate: true });
onMounted(() => {
  void loadMemory();
  void loadRuntime();
});
</script>

<style scoped>
.ef-settings-page { width: 100%; }
.ef-settings-page__scope {
  display: flex;
  align-items: center;
  gap: 0;
  margin-bottom: 12px;
  border-bottom: 1px solid var(--ef-ink);
}
.ef-settings-page__scope button {
  display: flex;
  align-items: baseline;
  gap: 5px;
  padding: 6px 14px;
  border: 0;
  border-right: 1px solid var(--ef-rule);
  background: transparent;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 11px;
  font-weight: 600;
  cursor: pointer;
}
.ef-settings-page__scope button small { font-size: 8px; letter-spacing: .1em; color: var(--ef-ink-3, #8a8a86); }
.ef-settings-page__scope button.current { background: var(--ef-signal); color: var(--ef-on-signal); }
.ef-settings-page__scope button.current small { color: #4a4700; }
.ef-settings-page__scope button:disabled { opacity: .4; cursor: default; }
.ef-settings-page__target {
  margin-left: auto;
  color: var(--ef-ink-2);
  font-size: 10px;
}
.ef-settings-page__layout {
  display: grid;
  grid-template-columns: 168px minmax(0, 1fr);
  gap: 16px;
  align-items: start;
}
.ef-settings-nav {
  position: sticky;
  top: 0;
  display: flex;
  flex-direction: column;
  border-left: 1px solid var(--ef-rule);
}
.ef-settings-nav button {
  display: flex;
  align-items: center;
  gap: 7px;
  padding: 6px 8px;
  border: 0;
  border-left: 2px solid transparent;
  background: transparent;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 11px;
  font-weight: 600;
  text-align: left;
  cursor: pointer;
}
.ef-settings-nav button .material-icons { font-size: 14px; opacity: .8; }
.ef-settings-nav button small {
  margin-left: auto;
  color: var(--ef-ink-3, #8a8a86);
  font-size: 8px;
  font-variant-numeric: tabular-nums;
}
.ef-settings-nav button:hover { border-left-color: var(--ef-signal); background: var(--ef-paper-3); color: var(--ef-ink); }
.ef-settings-nav button.current { border-left-color: var(--ef-ink); background: var(--ef-signal); color: var(--ef-on-signal); }
.ef-settings-nav button.current small { color: #4a4700; }
.ef-settings-content { min-width: 0; }
.ef-settings-content__head {
  padding-bottom: 10px;
  margin-bottom: 12px;
  border-bottom: 1px solid var(--ef-rule);
}
.ef-eyebrow {
  margin: 0 0 3px;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 9px;
  letter-spacing: .16em;
  text-transform: uppercase;
}
.ef-settings-content__head h2 { margin: 0; font-family: var(--ef-font-ui); font-size: 18px; font-weight: 800; }
.ef-settings-content__head p:last-child { margin: 4px 0 0; color: var(--ef-ink-2); font-size: 11px; }
.ef-settings-list { display: flex; flex-direction: column; }
.ef-settings-item {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 8px 0;
  border-bottom: 1px solid var(--ef-rule);
}
.ef-settings-item > div { flex: 1; min-width: 0; }
.ef-settings-item strong { display: block; font-family: var(--ef-font-ui); font-size: 12px; font-weight: 700; }
.ef-settings-item small { display: block; overflow: hidden; margin-top: 1px; color: var(--ef-ink-2); font-size: 9px; text-overflow: ellipsis; white-space: nowrap; }
.ef-settings-stats { display: flex; gap: 8px; margin-bottom: 10px; overflow: auto; }
.ef-settings-actions { display: flex; gap: 6px; flex-wrap: wrap; margin: 10px 0; }
.ef-settings-form { display: flex; flex-direction: column; gap: 10px; max-width: 420px; }
.ef-settings-form label { display: flex; flex-direction: column; gap: 4px; color: var(--ef-ink-2); font-family: var(--ef-font-ui); font-size: 9px; letter-spacing: .1em; text-transform: uppercase; }
.ef-settings-path { display: flex; align-items: center; gap: 8px; padding: 5px 0; border-bottom: 1px solid var(--ef-rule); }
.ef-settings-path span { flex: 0 0 72px; color: var(--ef-ink-2); font-family: var(--ef-font-ui); font-size: 9px; letter-spacing: .1em; text-transform: uppercase; }
.ef-settings-path code { flex: 1; min-width: 0; overflow: hidden; color: var(--ef-ink); font-family: var(--ef-font-mono); font-size: 10px; text-overflow: ellipsis; white-space: nowrap; }
.ef-settings-log { margin: 10px 0; max-height: 34vh; overflow: auto; font-family: var(--ef-font-mono); font-size: 10px; line-height: 1.55; color: var(--ef-ink-2); white-space: pre-wrap; }
.ef-hint { margin: 8px 0 0; color: var(--ef-ink-2); font-size: 10px; line-height: 1.5; }
.ef-shell-cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
  gap: 8px;
}
.ef-shell-card {
  position: relative;
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding: 8px;
  border: 1px solid var(--ef-rule);
  border-radius: 0;
  background: var(--ef-panel);
  color: var(--ef-ink);
  text-align: left;
  cursor: pointer;
  transition: border-color .12s var(--ef-mech);
}
.ef-shell-card:hover { border-color: var(--ef-ink); }
.ef-shell-card.current {
  border-color: var(--ef-ink);
  box-shadow: inset 0 -3px 0 var(--ef-signal);
}
.ef-shell-card__preview {
  position: relative;
  display: block;
  height: 88px;
  overflow: hidden;
  border: 1px solid var(--ef-rule);
  background: #f4f4f2;
}
.ef-shell-card__preview i { position: absolute; display: block; }
.ef-shell-card__bar { top: 0; left: 0; right: 0; height: 15px; background: #161616; }
.ef-shell-card__side { top: 15px; left: 0; bottom: 0; width: 26%; background: #e7e7e3; border-right: 1px solid #d4d4d0; }
.ef-shell-card__block { top: 23px; left: 33%; width: 32%; height: 8px; background: var(--ef-signal); }
.ef-shell-card__line { top: 39px; left: 33%; width: 54%; height: 2px; background: #c9c9c5; }
.ef-shell-card__line.short { top: 47px; width: 38%; }
.ef-shell-card__preview.is-melody { background: #1b1a1f; border-color: #302c3a; }
.ef-shell-card__preview.is-melody .ef-shell-card__bar { background: #2a2633; }
.ef-shell-card__preview.is-melody .ef-shell-card__side { background: #24212b; border-right-color: #37323f; }
.ef-shell-card__preview.is-melody .ef-shell-card__block { background: #b9a7ff; }
.ef-shell-card__preview.is-melody .ef-shell-card__line { background: #4a4358; }
.ef-shell-card__preview.is-luna-neo { background: #f7f4f6; }
.ef-shell-card__preview.is-luna-neo .ef-shell-card__bar { background: #241f24; }
.ef-shell-card__preview.is-luna-neo .ef-shell-card__side { background: #eee8ed; border-right-color: #ddd4dc; }
.ef-shell-card__preview.is-luna-neo .ef-shell-card__block { background: #d65c9a; }
.ef-shell-card__preview.is-luna-neo .ef-shell-card__line { background: #d3c6cf; }
.ef-shell-card__preview.is-pcl { background: #f2f3f5; }
.ef-shell-card__preview.is-pcl .ef-shell-card__bar { background: #2f3b4c; }
.ef-shell-card__preview.is-pcl .ef-shell-card__side { background: #e3e6ea; border-right-color: #cdd2d9; }
.ef-shell-card__preview.is-pcl .ef-shell-card__block { background: #5aa9e6; }
.ef-shell-card__preview.is-pcl .ef-shell-card__line { background: #c2c8d0; }
.ef-shell-card__preview.is-demo { background: #111827; border-color: #243047; }
.ef-shell-card__preview.is-demo .ef-shell-card__bar { background: #0b1220; }
.ef-shell-card__preview.is-demo .ef-shell-card__side { background: #1b2435; border-right-color: #2a364d; }
.ef-shell-card__preview.is-demo .ef-shell-card__block { background: #6366f1; }
.ef-shell-card__preview.is-demo .ef-shell-card__line { background: #334155; }
.ef-shell-card__copy strong {
  display: block;
  font-family: var(--ef-font-ui);
  font-size: 13px;
  font-weight: 800;
}
.ef-shell-card__copy small {
  display: block;
  margin-top: 2px;
  color: var(--ef-ink-2);
  font-size: 9px;
  line-height: 1.4;
}
.ef-shell-card__mark {
  position: absolute;
  top: 8px;
  right: 8px;
  padding: 1px 6px;
  background: var(--ef-signal);
  color: var(--ef-on-signal);
  font-family: var(--ef-font-ui);
  font-size: 8px;
  font-weight: 700;
  letter-spacing: .08em;
}
.ef-choice-row { display: flex; flex-wrap: wrap; gap: 14px; }
.ef-choice-row--spaced { margin-top: 14px; }
.ef-appearance-note { display: flex; align-items: flex-start; gap: 10px; }
.ef-appearance-swatch { flex: 0 0 28px; width: 28px; height: 28px; background: var(--ef-signal); }
.ef-appearance-note strong { display: block; font-family: var(--ef-font-ui); font-size: 12px; font-weight: 700; }
.ef-appearance-note p { margin: 3px 0 0; color: var(--ef-ink-2); font-size: 10px; line-height: 1.5; }
@media (max-width: 860px) {
  .ef-settings-page__layout { grid-template-columns: 1fr; }
  .ef-settings-nav { position: static; flex-direction: row; overflow: auto; border-left: 0; border-bottom: 1px solid var(--ef-rule); }
  .ef-settings-nav button { flex: 0 0 auto; border-left: 0; border-bottom: 2px solid transparent; }
  .ef-settings-nav button.current { border-bottom-color: var(--ef-ink); }
}
</style>
