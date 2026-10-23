<template>
  <div class="pcl-java-panel">
    <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>

    <PclCard title="Java 运行时">
      <template #actions>
        <PclButton size="sm" icon="refresh-cw" :disabled="busy || localBusy" @click="scan">扫描</PclButton>
      </template>

      <PclLoading v-if="localBusy && !installed.length" text="正在扫描 Java 运行时" inline />
      <div v-else class="pcl-java-list">
        <PclListItem
          title="自动选择"
          info="根据当前实例自动选择兼容的 Java 运行时"
          :selected="automatic"
          @click="toggleAutomatic(true)"
        >
          <template #icon><PclIcon name="sparkles" :size="19" /></template>
          <template #actions>
            <PclIcon v-if="automatic" name="circle-check" :size="16" />
          </template>
        </PclListItem>

        <PclListItem
          v-for="runtime in installed"
          :key="runtime.path"
          :title="runtimeTitle(runtime)"
          :info="runtimeInfo(runtime)"
          :selected="!automatic && selectedPath === runtime.path"
          @click="select(runtime.path)"
        >
          <template #icon><PclIcon name="coffee" :size="19" /></template>
          <template #actions>
            <PclIcon v-if="!automatic && selectedPath === runtime.path" name="circle-check" :size="16" />
            <PclIconButton v-if="isManaged(runtime)" icon="trash-2" label="删除托管 Java" :size="24" danger @click.stop="remove(runtime)" />
          </template>
        </PclListItem>

        <div v-if="!installed.length" class="pcl-empty">
          <PclIcon name="coffee" :size="34" />
          <strong>尚未发现 Java</strong>
          <span>点击“扫描”读取本机 Java 运行时。</span>
        </div>
      </div>
    </PclCard>

    <PclCard title="手动选择 Java">
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">Java 目录</span>
        <div class="pcl-form-row__control">
          <PclInput v-model="manualPath" placeholder="D:\Program Files\Java\jdk-21 或 ...\bin\java.exe" />
        </div>
        <PclButton variant="highlight" icon="check" :disabled="busy || localBusy || !manualPath.trim()" @click="applyManual">应用</PclButton>
      </div>
      <PclHint theme="blue">可以填 Java 主目录，会自动补全 <code>bin\java.exe</code>；也可以直接填 java.exe。应用后会关闭自动选择并写入当前作用域的 JavaPath。</PclHint>
    </PclCard>

    <PclCard title="诊断与修复">
      <div class="pcl-actions pcl-java-actions">
        <PclButton icon="fact_check" :disabled="busy || localBusy" @click="diagnose">测试当前设置</PclButton>
        <PclButton icon="tool-case" :disabled="busy || localBusy" @click="repair">自动修复</PclButton>
        <PclButton variant="ghost" icon="folder-open" :disabled="busy || localBusy" @click="openJavaRoot">打开 Java 目录</PclButton>
      </div>

      <template v-if="diagnostic">
        <div class="pcl-java-summary">
          <span class="pcl-java-summary__dot" :class="{ ok: diagnostic.usable && diagnostic.compatible }" />
          <strong>{{ diagnostic.usable ? 'Java 可以运行' : 'Java 不可用' }}</strong>
          <span>{{ diagnostic.compatible ? '与当前实例兼容' : '请检查兼容性' }}</span>
        </div>
        <div class="pcl-java-meta">
          <span>版本</span><code>{{ diagnostic.version || '—' }}</code>
          <span>架构</span><code>{{ diagnostic.architecture || '—' }}{{ diagnostic.is64Bit ? ' · 64 位' : '' }}</code>
          <span>路径</span><code>{{ diagnostic.executable || diagnostic.resolved || '—' }}</code>
        </div>
        <PclHint v-if="diagnostic.recommendations?.length" theme="yellow">
          <span v-for="recommendation in diagnostic.recommendations" :key="recommendation">{{ recommendation }}</span>
        </PclHint>
        <pre v-if="diagnostic.probeOutput || diagnostic.probeError" class="pcl-java-probe">{{ diagnostic.probeOutput }}{{ diagnostic.probeError }}</pre>
      </template>
      <p v-else class="pcl-muted pcl-java-placeholder">点击“测试当前设置”检查 Java 是否可执行、是否与实例版本兼容。</p>
    </PclCard>

    <PclCard title="下载 Java" collapsible>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">发行商</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="provider" :options="providers" @update:model-value="resetDownload" />
        </div>
      </div>
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">Java 版本</span>
        <div class="pcl-form-row__control">
          <PclSelect v-model="version" :options="versions" placeholder="先加载版本" />
        </div>
        <PclButton size="sm" icon="refresh-cw" :disabled="busy || localBusy" @click="loadVersions">加载版本</PclButton>
      </div>
      <div class="pcl-actions">
        <PclButton icon="package" :disabled="busy || localBusy || !version" @click="loadPackages">查找安装包</PclButton>
      </div>
      <div class="pcl-java-packages">
        <div v-for="item in packages" :key="item.url" class="pcl-java-package">
          <div>
            <strong>{{ item.name }} {{ item.version }}</strong>
            <small>{{ item.vendor }} · {{ item.runtimeOS }}</small>
          </div>
          <PclButton size="sm" variant="highlight" icon="download" :disabled="busy || localBusy" @click="install(item)">安装</PclButton>
        </div>
        <PclLoading v-if="localBusy && packagesLoading" text="正在读取安装包" inline />
      </div>
    </PclCard>
    <PclInstallProgressDialog v-model="installOpen" :title="installTitle" :task="installTask" :cancelling="installCancelling" @cancel="cancelInstall" />
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue';
import PclCard from '../../components/PclCard.vue';
import PclButton from '../../components/PclButton.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclInstallProgressDialog from '../download/PclInstallProgressDialog.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclSelect from '../../components/PclSelect.vue';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ scope: Record<string, unknown>; settings: any[]; busy: boolean; execute: Execute }>();
const emit = defineEmits<{ changed: [] }>();

const installed = ref<any[]>([]);
const diagnostic = ref<any>(null);
const error = ref('');
const localBusy = ref(false);
const installOpen = ref(false), installTask = ref<any>(null), installCancelling = ref(false), installTitle = ref('正在安装 Java');
const packagesLoading = ref(false);
const provider = ref('net.minecraft.java');
const version = ref('');
const versions = ref<string[]>([]);
const packages = ref<any[]>([]);
const managedRoot = ref('');
const providers = [
  { label: 'Mojang', value: 'net.minecraft.java' },
  { label: 'Adoptium', value: 'net.adoptium.java' },
  { label: 'Azul Zulu', value: 'com.azul.java' },
  { label: 'IBM Semeru', value: 'com.ibm.java' },
];
const automatic = ref(false);
const selectedPath = computed(() => String(props.settings.find(setting => setting.key === 'JavaPath')?.value || ''));
const manualPath = ref(selectedPath.value);

watch(() => props.settings, items => {
  const setting = items.find((item: any) => item.key === 'AutomaticJava');
  if (setting) {
    const value = setting.value;
    automatic.value = typeof value === 'string'
      ? ['true', '1', 'yes', 'on'].includes(value.trim().toLowerCase())
      : Boolean(value);
  }
}, { immediate: true });

watch(() => props.scope.instance, () => {
  diagnostic.value = null;
  error.value = '';
  manualPath.value = selectedPath.value;
  void scan();
});
watch(selectedPath, value => { manualPath.value = value; });

function message(value: unknown): string {
  return value instanceof Error ? value.message : String(value);
}
async function perform(action: () => Promise<void>) {
  if (props.busy || localBusy.value) return;
  error.value = '';
  localBusy.value = true;
  try {
    await action();
  } catch (exception) {
    error.value = message(exception);
  } finally {
    localBusy.value = false;
  }
}
async function fetchInstalled() {
  const result = await props.execute('java.refresh', {});
  installed.value = Array.isArray(result) ? result : (result?.runtimes || []);
}
function scan() {
  return perform(fetchInstalled);
}
function diagnose() {
  return perform(async () => {
    diagnostic.value = await props.execute('java.diagnose', {
      ...props.scope,
      arguments: props.settings.find(setting => setting.key === 'JvmArgs')?.value || '',
    });
  });
}
function repair() {
  return perform(async () => {
    diagnostic.value = await props.execute('java.repair', props.scope);
    await fetchInstalled();
    emit('changed');
  });
}
function toggleAutomatic(value: boolean) {
  automatic.value = value;
  return perform(async () => {
    await props.execute('settings.set', { ...props.scope, key: 'AutomaticJava', value });
    emit('changed');
  });
}
function select(path: string) {
  return perform(async () => {
    await props.execute('java.select', { ...props.scope, path });
    manualPath.value = path;
    emit('changed');
  });
}
function applyManual() {
  let path = manualPath.value.trim().replace(/^"(.*)"$/, '$1');
  if (!path) return;
  if (!/\.exe$/i.test(path) && !/[\\/]java$/i.test(path) && !/[\\/]bin[\\/]?$/i.test(path)) {
    path = `${path.replace(/[\\/]+$/, '')}\\bin\\java.exe`;
  } else if (/[\\/]bin[\\/]?$/i.test(path)) {
    path = `${path.replace(/[\\/]+$/, '')}\\java.exe`;
  }
  return perform(async () => {
    await props.execute('java.select', { ...props.scope, path });
    manualPath.value = path;
    automatic.value = false;
    await fetchInstalled();
    emit('changed');
  });
}
async function remove(runtime: any) {
  if (!window.confirm(`确定删除托管 Java ${runtime.version || runtime.path} 吗？`)) return;
  await perform(async () => {
    await props.execute('java.remove', { name: runtime.path, path: runtime.path, confirm: true });
    await fetchInstalled();
    emit('changed');
  });
}
function resetDownload() {
  versions.value = [];
  version.value = '';
  packages.value = [];
}
function loadVersions() {
  return perform(async () => {
    const result = await props.execute('component.versions', { uid: provider.value });
    versions.value = (result?.versions || []).map((item: any) => String(item.version || '')).filter(Boolean);
  });
}
function loadPackages() {
  if (!version.value) return;
  return perform(async () => {
    packagesLoading.value = true;
    try {
      packages.value = await props.execute('java.versions', { uid: provider.value, version: version.value });
    } finally {
      packagesLoading.value = false;
    }
  });
}
async function cancelInstall() {
  installCancelling.value = true;
  try { if (installTask.value?.taskId) await props.execute('neo.task.cancel', { taskId: installTask.value.taskId }); }
  catch { /* already terminal */ }
}

function install(metadata: any) {
  return perform(async () => {
    installTitle.value = `正在安装 Java：${metadata?.name || metadata?.version || ''}`;
    installTask.value = null;
    installCancelling.value = false;
    installOpen.value = true;
    try {
      let task: any = await props.execute('neo.task.start', { operation: 'java.install', priority: 'high', parameters: { metadata } });
      while (task && !task.finished) {
        installTask.value = task;
        await new Promise(resolve => setTimeout(resolve, 300));
        task = await props.execute('neo.task.status', { taskId: task.taskId });
      }
      installTask.value = task;
      if (!task || task.state !== 'succeeded') throw new Error(task?.error || 'Java 安装失败');
      await fetchInstalled();
      emit('changed');
    } finally {
      installOpen.value = false;
      installCancelling.value = false;
      installTask.value = null;
    }
  });
}
async function loadManagedRoot() {
  try {
    const info = await props.execute('runtime.info', {});
    let root = String(info.javaRoot || 'java');
    if (!/^[A-Za-z]:[\\/]/.test(root) && !root.startsWith('/') && info.dataRoot) {
      root = `${String(info.dataRoot).replace(/[\\/]+$/, '')}/${root.replace(/^[\\/]+/, '')}`;
    }
    managedRoot.value = root.replace(/[\\/]+$/, '').replace(/\\/g, '/');
  } catch {
    managedRoot.value = '';
  }
}
function isManaged(runtime: any): boolean {
  if (!managedRoot.value) return false;
  const path = String(runtime?.path || '').replace(/\\/g, '/').replace(/\/+/g, '/');
  return path.toLowerCase().startsWith(`${managedRoot.value.toLowerCase()}/`);
}
async function openJavaRoot() {
  try {
    if (!managedRoot.value) await loadManagedRoot();
    if (managedRoot.value) await props.execute('desktop.open-path', { path: managedRoot.value });
  } catch (exception) {
    error.value = message(exception);
  }
}
function runtimeTitle(runtime: any): string {
  return `Java ${runtime.version || runtime.id || '未知版本'}`;
}
function runtimeInfo(runtime: any): string {
  const bits = runtime.is64Bit ? '64 位' : '32 位';
  const architecture = runtime.architecture ? ` · ${runtime.architecture}` : '';
  return `${bits}${architecture} · ${runtime.path}`;
}

onMounted(() => {
  void loadManagedRoot();
  void scan();
});
</script>

<style scoped>
.pcl-java-list {
  max-height: 420px;
  overflow-y: auto;
}

.pcl-java-actions {
  margin-top: 0;
}

.pcl-java-summary {
  display: flex;
  align-items: center;
  gap: 8px;
  margin: 14px 0 10px;
  font-size: calc(13px * var(--pcl-font-scale, 1));
}

.pcl-java-summary strong {
  font-weight: 700;
}

.pcl-java-summary span:last-child {
  color: var(--pcl-muted);
}

.pcl-java-summary__dot {
  width: 9px;
  height: 9px;
  border-radius: 50%;
  background: var(--pcl-danger);
}

.pcl-java-summary__dot.ok {
  background: #34d399;
}

.pcl-java-meta {
  display: grid;
  grid-template-columns: 44px minmax(0, 1fr);
  gap: 5px 10px;
  color: var(--pcl-muted);
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-java-meta code {
  overflow: hidden;
  color: var(--pcl-text);
  font: calc(12px * var(--pcl-font-scale, 1))/1.5 ui-monospace, Consolas, monospace;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-java-probe {
  max-height: 160px;
  margin: 10px 0 0;
  padding: 9px 11px;
  overflow: auto;
  border-radius: 3px;
  background: var(--pcl-c8);
  color: var(--pcl-muted);
  font: calc(11px * var(--pcl-font-scale, 1))/1.5 ui-monospace, Consolas, monospace;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}

.pcl-java-placeholder {
  margin: 12px 0 0;
  font-size: calc(13px * var(--pcl-font-scale, 1));
}

.pcl-java-packages {
  margin-top: 10px;
}

.pcl-java-package {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  min-height: 44px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-java-package > div {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.pcl-java-package strong {
  overflow: hidden;
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  font-weight: 400;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-java-package small {
  margin-top: 2px;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}
</style>
