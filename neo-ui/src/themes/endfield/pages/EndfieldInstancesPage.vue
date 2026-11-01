<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef-instances">
    <div class="ef-instances__toolbar">
      <EfSearchBox v-model="query" placeholder="按名称、版本、分组筛选" />
      <EfButton variant="accent" icon="add" @click="openCreate">新建实例</EfButton>
      <EfButton icon="upload_file" :disabled="busy" @click="importInstance">导入</EfButton>
    </div>

    <div v-if="filtered.length" class="ef-instances__table">
      <div class="ef-instances__head">
        <span>No.</span>
        <span>名称</span>
        <span>版本</span>
        <span>分组</span>
        <span>状态</span>
        <span>操作</span>
      </div>
      <article v-for="(item, index) in filtered" :key="item.id" class="ef-instance-row" :class="{ current: item.id === selected?.id }">
        <EfField :value="String(index + 1).padStart(3, '0')" />
        <button type="button" class="ef-instance-row__name" @click="emit('select', item)">
          <img v-if="icons?.[item.id]" :src="icons[item.id]" alt="" />
          <span v-else class="material-icons">inventory_2</span>
          <strong>{{ item.name }}</strong>
        </button>
        <span class="ef-instance-row__version">{{ item.version || item.runtime?.minecraft || 'Minecraft' }}</span>
        <span class="ef-instance-row__group">{{ item.group || '—' }}</span>
        <span class="ef-instance-row__state" :class="{ running: item.running }">{{ item.running ? '运行中' : '空闲' }}</span>
        <div class="ef-instance-row__actions">
          <EfIconButton icon="play_arrow" label="启动" :disabled="busy || item.running" @click="emit('launch', item)" />
          <EfIconButton icon="folder_open" label="打开文件夹" :disabled="busy" @click="openFolder(item)" />
          <EfIconButton icon="tune" label="详情" @click="emit('open', item)" />
          <EfIconButton icon="more_horiz" label="更多" @click="openActions(item)" />
        </div>
      </article>
    </div>
    <EfEmpty v-else title="没有实例" description="新建一个实例，或从整合包导入。" icon="inventory_2" />

    <EfDialog v-model="createOpen" title="新建实例">
      <div class="ef-form">
        <label>名称<EfInput v-model="form.name" placeholder="自动生成" /></label>
        <label>Minecraft<EfSelect v-model="form.version" :options="versionOptions" placeholder="选择版本" /></label>
        <label>加载器<EfSelect v-model="form.loader" :options="loaderOptions" /></label>
        <label v-if="form.loader">加载器版本<EfSelect v-model="form.loaderVersion" :options="loaderVersionOptions" placeholder="选择版本" /></label>
      </div>
      <template #actions>
        <EfButton variant="ghost" @click="createOpen = false">取消</EfButton>
        <EfButton variant="accent" :busy="creating" :disabled="!form.version" @click="createInstance">创建</EfButton>
      </template>
    </EfDialog>

    <EfDialog v-model="actionOpen" :title="actionTitle">
      <p class="ef-action-copy">{{ actionTarget?.name }}</p>
      <EfInput v-if="actionKind" v-model="actionValue" :placeholder="actionKind === 'group' ? '分组名称' : '新名称'" />
      <div class="ef-action-list">
        <EfButton variant="ghost" icon="edit" @click="startRename('rename')">重命名</EfButton>
        <EfButton variant="ghost" icon="content_copy" @click="startRename('copy')">复制</EfButton>
        <EfButton variant="ghost" icon="folder" @click="startRename('group')">移动分组</EfButton>
        <EfButton variant="ghost" icon="fact_check" @click="runAction('verify')">校验</EfButton>
        <EfButton variant="ghost" icon="stop_circle" :disabled="!actionTarget?.running" @click="runAction('stop')">停止</EfButton>
        <EfButton variant="red" icon="delete" @click="removeInstance">删除</EfButton>
      </div>
      <template #actions>
        <EfButton variant="ghost" @click="actionOpen = false">关闭</EfButton>
        <EfButton v-if="actionKind" variant="accent" :busy="busy" @click="applyRename">确定</EfButton>
      </template>
    </EfDialog>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import EfButton from '../components/EfButton.vue';
import EfDialog from '../components/EfDialog.vue';
import EfEmpty from '../components/feedback/EfEmpty.vue';
import EfField from '../components/data/EfField.vue';
import EfIconButton from '../components/controls/EfIconButton.vue';
import EfInput from '../components/EfInput.vue';
import EfSearchBox from '../components/controls/EfSearchBox.vue';
import EfSelect from '../components/EfSelect.vue';

const props = defineProps<{
  instances: any[];
  icons?: Record<string, string>;
  selected?: any;
  busy: boolean;
  execute: (op: string, p?: Record<string, unknown>) => Promise<any>;
}>();
const emit = defineEmits<{
  select: [instance: any];
  open: [instance: any];
  launch: [instance: any];
  changed: [];
  toast: [type: 'success' | 'error' | 'info', title: string, message?: string];
}>();

const query = ref('');
const createOpen = ref(false);
const creating = ref(false);
const versions = ref<any[]>([]);
const loaderVersions = ref<any[]>([]);
const form = ref({ name: '', version: '', loader: '', loaderVersion: '' });
const actionOpen = ref(false);
const actionTarget = ref<any>(null);
const actionKind = ref<'' | 'rename' | 'copy' | 'group'>('');
const actionValue = ref('');
const actionTitle = computed(() => actionKind.value ? '编辑实例' : '实例操作');

const loaderOptions = [
  { label: '原版', value: '' },
  { label: 'Fabric', value: 'fabric' },
  { label: 'Quilt', value: 'quilt' },
  { label: 'Forge', value: 'forge' },
  { label: 'NeoForge', value: 'neoforge' },
  { label: 'Legacy Fabric', value: 'legacyfabric' },
];
const loaderUid: Record<string, string> = {
  fabric: 'net.fabricmc.fabric-loader',
  quilt: 'org.quiltmc.quilt-loader',
  forge: 'net.minecraftforge',
  neoforge: 'net.neoforged',
};

const versionOptions = computed(() => versions.value.map(entry => ({ label: versionOf(entry), value: versionOf(entry) })));
const loaderVersionOptions = computed(() => loaderVersions.value.map(entry => ({ label: versionOf(entry), value: versionOf(entry) })));
const filtered = computed(() => {
  const text = query.value.trim().toLowerCase();
  if (!text) return props.instances;
  return props.instances.filter((item: any) => `${item.name} ${item.group || ''} ${item.version || ''} ${item.runtime?.minecraft || ''}`.toLowerCase().includes(text));
});

function versionOf(entry: any) {
  return String(entry?.version || entry?.versionId || entry?.id || entry?.name || '');
}

watch(() => [form.value.loader, form.value.version], () => {
  if (form.value.loader) void loadLoaderVersions(form.value.loader);
}, { flush: 'post' });

async function openCreate() {
  createOpen.value = true;
  form.value = { name: '', version: '', loader: '', loaderVersion: '' };
  if (!versions.value.length) {
    try {
      const result: any = await props.execute('component.versions', { uid: 'net.minecraft' });
      const list = Array.isArray(result) ? result : (result?.versions || []);
      versions.value = list;
      const latest = list[list.length - 1];
      if (latest) form.value.version = versionOf(latest);
    } catch {
      /* handled by shell */
    }
  }
}

async function loadLoaderVersions(loader: string) {
  loaderVersions.value = [];
  form.value.loaderVersion = '';
  if (!loader || !form.value.version) return;
  try {
    const result: any = loader === 'legacyfabric'
      ? await props.execute('special.loader.versions', { loader: 'legacyfabric', minecraftVersion: form.value.version })
      : await props.execute('component.versions', { uid: loaderUid[loader], minecraftVersion: form.value.version });
    const list = Array.isArray(result) ? result : (result?.versions || []);
    loaderVersions.value = list;
    const latest = list[list.length - 1];
    if (latest) form.value.loaderVersion = versionOf(latest);
  } catch {
    /* ignore */
  }
}

async function createInstance() {
  if (!form.value.version) return;
  creating.value = true;
  try {
    const parameters: Record<string, unknown> = {
      name: form.value.name.trim() || `${form.value.version} ${form.value.loader || 'Vanilla'}`.trim(),
      version: form.value.version,
    };
    if (form.value.loader && form.value.loader !== 'legacyfabric') {
      parameters.loader = form.value.loader;
      parameters.loaderVersion = form.value.loaderVersion;
    }
    let task: any = await props.execute('neo.task.start', { operation: 'instance.create', priority: 'high', parameters });
    while (task && !task.finished) {
      await new Promise(resolve => setTimeout(resolve, 350));
      task = await props.execute('neo.task.status', { taskId: task.taskId });
    }
    if (task?.state !== 'succeeded') throw new Error(task?.error || '创建失败');
    emit('toast', 'success', '实例已创建', parameters.name as string);
    createOpen.value = false;
    emit('changed');
  } catch (error: any) {
    emit('toast', 'error', '创建失败', error?.message || String(error));
  } finally {
    creating.value = false;
  }
}

async function importInstance() {
  try {
    const path = window.prompt('整合包或实例压缩包路径');
    if (!path) return;
    let task: any = await props.execute('neo.task.start', { operation: 'instance.import', priority: 'high', parameters: { path } });
    while (task && !task.finished) {
      await new Promise(resolve => setTimeout(resolve, 400));
      task = await props.execute('neo.task.status', { taskId: task.taskId });
    }
    if (task?.state !== 'succeeded') throw new Error(task?.error || '导入失败');
    emit('toast', 'success', '导入完成');
    emit('changed');
  } catch (error: any) {
    emit('toast', 'error', '导入失败', error?.message || String(error));
  }
}

async function openFolder(instance: any) {
  try { await props.execute('instance.open-folder', { instance: instance.id }); } catch { /* shell toast */ }
}

function openActions(instance: any) {
  actionTarget.value = instance;
  actionKind.value = '';
  actionValue.value = '';
  actionOpen.value = true;
}

function startRename(kind: 'rename' | 'copy' | 'group') {
  actionKind.value = kind;
  actionValue.value = kind === 'group' ? (actionTarget.value?.group || '') : (actionTarget.value?.name || '');
}

async function applyRename() {
  if (!actionTarget.value || !actionKind.value) return;
  try {
    const parameters: Record<string, unknown> = { instance: actionTarget.value.id };
    if (actionKind.value === 'group') parameters.group = actionValue.value;
    else parameters.name = actionValue.value;
    await props.execute(`instance.${actionKind.value}`, parameters);
    emit('toast', 'success', '已更新');
    actionKind.value = '';
    actionOpen.value = false;
    emit('changed');
  } catch { /* shell toast */ }
}

async function runAction(op: string) {
  if (!actionTarget.value) return;
  try {
    await props.execute(`instance.${op}`, { instance: actionTarget.value.id });
    emit('toast', 'success', op === 'verify' ? '校验完成' : '已停止');
    actionOpen.value = false;
    emit('changed');
  } catch { /* shell toast */ }
}

async function removeInstance() {
  if (!actionTarget.value) return;
  if (!window.confirm(`删除实例“${actionTarget.value.name}”？此操作不可撤销。`)) return;
  try {
    await props.execute('instance.delete', { instance: actionTarget.value.id, confirm: true });
    emit('toast', 'success', '实例已删除');
    actionOpen.value = false;
    emit('changed');
  } catch { /* shell toast */ }
}
</script>

<style scoped>
.ef-instances { width: 100%; }
.ef-instances__toolbar {
  display: flex;
  gap: 8px;
  align-items: center;
  margin-bottom: 12px;
}
.ef-instances__toolbar > :first-child { flex: 1; }
.ef-instances__table {
  border-top: 1px solid var(--ef-ink);
}
.ef-instances__head,
.ef-instance-row {
  display: grid;
  grid-template-columns: 56px minmax(160px, 1.4fr) minmax(110px, .8fr) minmax(90px, .6fr) 74px auto;
  gap: 8px;
  align-items: center;
}
.ef-instances__head {
  padding: 5px 4px;
  border-bottom: 1px solid var(--ef-rule);
  color: var(--ef-ink-3, #8a8a86);
  font-family: var(--ef-font-ui);
  font-size: 8px;
  letter-spacing: .14em;
  text-transform: uppercase;
}
.ef-instance-row {
  padding: 7px 4px;
  border-bottom: 1px solid var(--ef-rule);
}
.ef-instance-row.current { background: var(--ef-signal); }
.ef-instance-row.current .ef-instance-row__version,
.ef-instance-row.current .ef-instance-row__group { color: var(--ef-on-signal); }
.ef-instance-row__name {
  display: flex;
  align-items: center;
  gap: 8px;
  min-width: 0;
  padding: 0;
  border: 0;
  background: transparent;
  color: inherit;
  text-align: left;
  cursor: pointer;
}
.ef-instance-row__name img {
  width: 24px;
  height: 24px;
  object-fit: cover;
  border: 1px solid var(--ef-rule);
}
.ef-instance-row__name .material-icons { font-size: 18px; color: var(--ef-ink-2); }
.ef-instance-row__name strong {
  overflow: hidden;
  font-family: var(--ef-font-ui);
  font-size: 12px;
  font-weight: 700;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-instance-row__version,
.ef-instance-row__group {
  overflow: hidden;
  color: var(--ef-ink-2);
  font-size: 10px;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ef-instance-row__state {
  padding: 1px 5px;
  border: 1px dashed var(--ef-rule);
  color: var(--ef-ink-2);
  font-family: var(--ef-font-ui);
  font-size: 8px;
  letter-spacing: .08em;
  text-align: center;
  text-transform: uppercase;
}
.ef-instance-row__state.running {
  border-style: solid;
  border-color: var(--ef-ink);
  background: var(--ef-signal);
  color: var(--ef-on-signal);
}
.ef-instance-row__actions { display: flex; gap: 2px; }
.ef-form { display: flex; flex-direction: column; gap: 10px; min-width: min(380px, 80vw); }
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
.ef-action-copy {
  margin: 0 0 8px;
  color: var(--ef-ink-2);
  font-size: 11px;
}
.ef-action-list {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  margin-top: 10px;
  max-width: 380px;
}
@media (max-width: 900px) {
  .ef-instances__head { display: none; }
  .ef-instance-row { grid-template-columns: 36px minmax(120px, 1fr) auto; }
  .ef-instance-row__version,
  .ef-instance-row__group,
  .ef-instance-row__state { display: none; }
}
</style>
