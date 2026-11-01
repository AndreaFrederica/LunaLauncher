<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef-tools">
    <nav class="ef-tabs">
      <button v-for="tab in tabs" :key="tab.id" type="button" :class="{ current: section === tab.id }" @click="section = tab.id">
        <small>// {{ tab.code }}</small>{{ tab.label }}
      </button>
    </nav>

    <section v-if="section === 'java'">
      <div class="ef-section-bar">
        <span>Java 运行时</span>
        <EfButton size="sm" icon="refresh" :disabled="busy" @click="loadJava">扫描</EfButton>
      </div>
      <div class="ef-rows">
        <article class="ef-row">
          <div class="ef-row__main">
            <strong>自动选择</strong>
            <small>由启动器根据 Minecraft 版本选择</small>
          </div>
          <EfButton size="sm" variant="accent" :disabled="busy" @click="selectJava('')">使用</EfButton>
        </article>
        <article v-for="runtime in javaRuntimes" :key="runtime.path" class="ef-row">
          <div class="ef-row__main">
            <strong>{{ runtime.version || runtime.name || runtime.path }}</strong>
            <small>{{ runtime.path }}</small>
          </div>
          <span class="ef-row__state">{{ runtime.majorVersion || '—' }}</span>
          <EfButton size="sm" variant="ghost" :disabled="busy" @click="selectJava(runtime.path)">选择</EfButton>
          <EfButton size="sm" variant="red" :disabled="busy" @click="removeJava(runtime)">删除</EfButton>
        </article>
      </div>
      <div class="ef-tools__actions">
        <EfButton icon="fact_check" :disabled="busy" @click="diagnoseJava">检测</EfButton>
        <EfButton icon="build" :disabled="busy" @click="repairJava">修复</EfButton>
      </div>
      <pre v-if="javaDiagnostic" class="ef-log">{{ javaDiagnostic }}</pre>
    </section>

    <section v-else-if="section === 'memory'">
      <div class="ef-tools__stats">
        <EfStat label="总内存" :value="memory.totalMiB" unit="MiB" />
        <EfStat label="已用" :value="memory.usedMiB" unit="MiB" :alert="memoryRatio > 85" />
        <EfStat label="占用" :value="memoryRatio" unit="%" />
      </div>
      <EfCard title="内存分配">
        <EfSlider v-model="memoryMax" :min="512" :max="Math.max(4096, memory.totalMiB)" :step="256" suffix=" MiB" />
        <div class="ef-tools__actions">
          <EfButton variant="accent" :disabled="busy" @click="saveMemory">保存到实例</EfButton>
          <EfButton icon="refresh" :disabled="busy" @click="loadMemory">刷新</EfButton>
        </div>
      </EfCard>
    </section>

    <section v-else-if="section === 'network'">
      <EfCard title="代理">
        <div class="ef-form">
          <label>类型<EfSelect v-model="proxy.type" :options="proxyTypes" /></label>
          <label v-if="proxy.type !== 'None'">主机<EfInput v-model="proxy.host" placeholder="127.0.0.1" /></label>
          <label v-if="proxy.type !== 'None'">端口<EfInput v-model="proxy.port" type="number" /></label>
        </div>
        <div class="ef-tools__actions">
          <EfButton variant="accent" :disabled="busy" @click="saveProxy">保存代理</EfButton>
          <EfButton icon="network_check" :disabled="busy" @click="testProxy">测试连接</EfButton>
        </div>
        <p v-if="proxyResult" class="ef-tools__result">{{ proxyResult }}</p>
      </EfCard>
    </section>

    <section v-else-if="section === 'cache'">
      <div class="ef-tools__stats">
        <EfStat label="HTTP 缓存" :value="cache.httpSize || '—'" :unit="cache.httpSize ? 'MB' : ''" />
        <EfStat label="认证缓存" :value="cache.authCount" note="账户条目" />
      </div>
      <div class="ef-tools__actions">
        <EfButton icon="refresh" :disabled="busy" @click="loadCache">刷新</EfButton>
        <EfButton variant="red" icon="delete_sweep" :disabled="busy" @click="clearCache">清理缓存</EfButton>
      </div>
    </section>

    <section v-else-if="section === 'diagnostics'">
      <EfCard title="运行环境">
        <div class="ef-info-grid">
          <div><span>数据目录</span><code>{{ runtimeInfo.dataRoot || '—' }}</code></div>
          <div><span>程序目录</span><code>{{ runtimeInfo.root || runtimeInfo.programRoot || '—' }}</code></div>
          <div><span>代理</span><code>{{ runtimeInfo.proxy || '—' }}</code></div>
          <div><span>实例</span><code>{{ instances.length }} 个</code></div>
        </div>
      </EfCard>
      <div class="ef-tools__actions">
        <EfButton icon="refresh" :disabled="busy" @click="loadRuntime">刷新</EfButton>
        <EfButton icon="folder_open" :disabled="busy" @click="openLogs">打开日志目录</EfButton>
      </div>
    </section>

    <section v-else-if="section === 'tasks'">
      <div class="ef-section-bar">
        <span>{{ tasks.length }} 个任务</span>
        <EfButton size="sm" icon="refresh" :disabled="busy" @click="loadTasks">刷新</EfButton>
      </div>
      <div v-if="tasks.length" class="ef-rows">
        <article v-for="task in tasks" :key="task.taskId || task.id" class="ef-row">
          <div class="ef-row__main">
            <strong>{{ task.title || task.operation || task.name || '任务' }}</strong>
            <small>{{ task.message || task.state || '—' }}</small>
          </div>
          <EfButton v-if="!finished(task)" size="sm" variant="red" :disabled="busy" @click="cancelTask(task)">取消</EfButton>
        </article>
      </div>
      <EfEmpty v-else title="没有任务" description="下载、安装、启动等操作会出现在这里。" icon="task_alt" />
    </section>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import EfButton from '../components/EfButton.vue';
import EfCard from '../components/EfCard.vue';
import EfEmpty from '../components/feedback/EfEmpty.vue';
import EfInput from '../components/EfInput.vue';
import EfSelect from '../components/EfSelect.vue';
import EfSlider from '../components/controls/EfSlider.vue';
import EfStat from '../components/data/EfStat.vue';

const props = defineProps<{
  busy: boolean;
  execute: (op: string, p?: Record<string, unknown>) => Promise<any>;
  instances: any[];
  selected?: any;
}>();
const emit = defineEmits<{
  toast: [type: 'success' | 'error' | 'info', title: string, message?: string];
}>();

const tabs = [
  { id: 'java', label: 'Java', code: '01' },
  { id: 'memory', label: '内存', code: '02' },
  { id: 'network', label: '网络', code: '03' },
  { id: 'cache', label: '缓存', code: '04' },
  { id: 'diagnostics', label: '诊断', code: '05' },
  { id: 'tasks', label: '任务', code: '06' },
];
const section = ref('java');
const javaRuntimes = ref<any[]>([]);
const javaDiagnostic = ref('');
const memory = ref({ totalMiB: 0, usedMiB: 0 });
const memoryMax = ref(4096);
const proxyTypes = [
  { label: '不使用', value: 'None' },
  { label: 'HTTP', value: 'Http' },
  { label: 'SOCKS5', value: 'Socks5' },
];
const proxy = ref({ type: 'None', host: '', port: 0 });
const proxyResult = ref('');
const cache = ref<any>({ httpSize: 0, authCount: 0 });
const runtimeInfo = ref<any>({});
const tasks = ref<any[]>([]);

const memoryRatio = computed(() => memory.value.totalMiB > 0 ? Math.round(memory.value.usedMiB / memory.value.totalMiB * 100) : 0);
const scope = computed(() => props.selected?.id ? { scope: 'instance', instance: props.selected.id } : { scope: 'launcher' });

function finished(task: any) {
  return Boolean(task?.finished) || ['succeeded', 'failed', 'cancelled', 'canceled', 'aborted'].includes(String(task?.state || '').toLowerCase());
}

async function loadJava() {
  try {
    const result: any = await props.execute('java.refresh', {});
    javaRuntimes.value = Array.isArray(result) ? result : (result?.runtimes || []);
  } catch { javaRuntimes.value = []; }
}
async function selectJava(path: string) {
  try {
    await props.execute('java.select', { ...scope.value, path });
    emit('toast', 'success', path ? '已选择 Java' : '已切换为自动选择');
  } catch { /* shell toast */ }
}
async function removeJava(runtime: any) {
  if (!window.confirm(`删除 Java“${runtime.version || runtime.path}”？`)) return;
  try {
    await props.execute('java.remove', { name: runtime.path, path: runtime.path, confirm: true });
    await loadJava();
    emit('toast', 'success', '已删除');
  } catch { /* shell toast */ }
}
async function diagnoseJava() {
  try {
    const result = await props.execute('java.diagnose', { ...scope.value });
    javaDiagnostic.value = typeof result === 'string' ? result : JSON.stringify(result, null, 2);
  } catch { /* shell toast */ }
}
async function repairJava() {
  try {
    const result = await props.execute('java.repair', scope.value);
    javaDiagnostic.value = typeof result === 'string' ? result : JSON.stringify(result, null, 2);
    await loadJava();
    emit('toast', 'success', '修复完成');
  } catch { /* shell toast */ }
}
async function loadMemory() {
  try {
    const result: any = await props.execute('runtime.memory', {});
    const data = result?.data || result || {};
    memory.value = {
      totalMiB: Number(data.totalMiB || 0),
      usedMiB: Number(data.usedMiB || 0),
    };
  } catch { /* ignore */ }
}
async function saveMemory() {
  if (!props.selected?.id) {
    emit('toast', 'error', '没有选择实例', '内存分配需要目标实例。');
    return;
  }
  try {
    await props.execute('settings.set', { scope: 'instance', instance: props.selected.id, key: 'MaxMemAlloc', value: memoryMax.value });
    emit('toast', 'success', '内存已保存', `${memoryMax.value} MiB`);
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
    const [http, auth] = await Promise.allSettled([
      props.execute('launcher.cache.status'),
      props.execute('yggdrasil.cache.status'),
    ]);
    const httpData: any = http.status === 'fulfilled' ? http.value : {};
    const authData: any = auth.status === 'fulfilled' ? auth.value : {};
    cache.value = {
      httpSize: httpData.sizeMiB ?? httpData.totalMiB ?? 0,
      authCount: authData.entries ?? authData.count ?? 0,
    };
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
async function loadRuntime() {
  try { runtimeInfo.value = await props.execute('runtime.info'); } catch { /* ignore */ }
}
async function openLogs() {
  try {
    const path = runtimeInfo.value.logs || runtimeInfo.value.dataRoot;
    if (!path) return;
    await props.execute('desktop.open-path', { path });
  } catch { /* shell toast */ }
}
async function loadTasks() {
  try {
    const result: any = await props.execute('task.list');
    tasks.value = Array.isArray(result) ? result : (result?.tasks || []);
  } catch { tasks.value = []; }
}
async function cancelTask(task: any) {
  try {
    await props.execute('task.cancel', { taskId: task.taskId || task.id });
    await loadTasks();
  } catch { /* shell toast */ }
}

watch(section, value => {
  if (value === 'java') void loadJava();
  else if (value === 'memory') void loadMemory();
  else if (value === 'network') void loadProxy();
  else if (value === 'cache') void loadCache();
  else if (value === 'diagnostics') void loadRuntime();
  else if (value === 'tasks') void loadTasks();
}, { immediate: true });
</script>

<style scoped>
.ef-tools { width: 100%; }
.ef-tabs { display: flex; border-bottom: 1px solid var(--ef-ink); margin-bottom: 12px; overflow: auto; }
.ef-tabs button {
  display: flex;
  align-items: baseline;
  gap: 5px;
  padding: 6px 12px;
  border: 0;
  border-right: 1px solid var(--ef-rule);
  background: transparent;
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 11px;
  font-weight: 600;
  white-space: nowrap;
  cursor: pointer;
}
.ef-tabs button small { font-size: 8px; letter-spacing: .1em; color: var(--ef-ink-3, #8a8a86); }
.ef-tabs button.current { background: var(--ef-signal); color: var(--ef-on-signal); }
.ef-tabs button.current small { color: #4a4700; }
.ef-section-bar { display: flex; align-items: center; justify-content: space-between; gap: 8px; margin-bottom: 10px; }
.ef-section-bar > span { color: var(--ef-ink-2); font-family: var(--ef-font-ui); font-size: 9px; letter-spacing: .12em; text-transform: uppercase; }
.ef-rows { display: flex; flex-direction: column; }
.ef-row { display: flex; align-items: center; gap: 10px; padding: 8px 0; border-bottom: 1px solid var(--ef-rule); }
.ef-row__main { flex: 1; min-width: 0; }
.ef-row__main strong { display: block; overflow: hidden; font-family: var(--ef-font-ui); font-size: 12px; font-weight: 700; text-overflow: ellipsis; white-space: nowrap; }
.ef-row__main small { display: block; margin-top: 1px; overflow: hidden; color: var(--ef-ink-2); font-size: 9px; text-overflow: ellipsis; white-space: nowrap; }
.ef-row__state { padding: 1px 6px; border: 1px solid var(--ef-rule); color: var(--ef-ink-2); font-family: var(--ef-font-ui); font-size: 8px; text-transform: uppercase; }
.ef-tools__actions { display: flex; gap: 6px; flex-wrap: wrap; margin-top: 10px; }
.ef-tools__stats { display: flex; gap: 8px; margin-bottom: 10px; overflow: auto; }
.ef-form { display: flex; flex-direction: column; gap: 10px; max-width: 420px; }
.ef-form label { display: flex; flex-direction: column; gap: 4px; color: var(--ef-ink-2); font-family: var(--ef-font-ui); font-size: 9px; letter-spacing: .1em; text-transform: uppercase; }
.ef-tools__result { margin: 10px 0 0; color: var(--ef-ink-2); font-size: 11px; }
.ef-info-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(230px, 1fr)); gap: 8px; }
.ef-info-grid div { padding-left: 8px; border-left: 2px solid var(--ef-rule); }
.ef-info-grid span { display: block; color: var(--ef-ink-2); font-family: var(--ef-font-ui); font-size: 8px; letter-spacing: .12em; text-transform: uppercase; }
.ef-info-grid code { display: block; margin-top: 2px; overflow: hidden; color: var(--ef-ink); font-family: var(--ef-font-mono); font-size: 10px; text-overflow: ellipsis; white-space: nowrap; }
.ef-log { margin: 10px 0 0; max-height: 40vh; overflow: auto; font-family: var(--ef-font-mono); font-size: 10px; line-height: 1.55; color: var(--ef-ink-2); white-space: pre-wrap; }
</style>
