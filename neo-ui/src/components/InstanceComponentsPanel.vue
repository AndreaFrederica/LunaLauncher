<template>
  <section class="component-panel pcl-section-card">
    <header class="component-toolbar">
      <div><span class="pcl-kicker">{{ t('componentsPanel.kicker') }}</span><h2>{{ t('componentsPanel.title') }}</h2><p>{{ t('componentsPanel.hint') }}</p></div>
      <div class="component-actions">
        <PclButton size="sm" icon="refresh-cw" :disabled="working" @click="reload">{{ t('common.refresh') }}</PclButton>
        <PclButton size="sm" icon="download" :disabled="working || profile.running" @click="downloadAll">{{ t('componentsPanel.downloadAll') }}</PclButton>
      </div>
    </header>
    <PclHint v-if="error || componentError" theme="red">{{ error || componentError }}</PclHint>
    <PclHint v-if="profile.readOnly" theme="blue">{{ t('componentsPanel.readOnly') }}</PclHint>
    <PclHint v-else-if="profile.running" theme="yellow">{{ t('componentsPanel.running') }}</PclHint>
    <div v-if="task" class="component-progress" role="status">
      <span>{{ task.status || task.title || t('componentsPanel.downloadAll') }}</span>
      <q-linear-progress :value="(taskItemPercent(task) || 0) / 100" :indeterminate="taskItemPercent(task) === null" />
    </div>
    <PclInput v-model="query" :placeholder="t('componentsPanel.search')" :aria-label="t('componentsPanel.search')" />
    <div class="component-list" :aria-busy="working">
      <article v-for="component in filtered" :key="component.id" class="component-row" :class="{ 'is-disabled': component.enabled === false }">
        <div class="component-row-head">
          <span class="component-order">{{ rows.indexOf(component) + 1 }}</span>
          <PclCheckbox :model-value="component.enabled !== false" :label="t('componentsPanel.enabled')" :disabled="locked || !component.canDisable" @update:model-value="change('set-enabled', component, { enabled: $event })" />
          <button type="button" class="component-heading" :aria-expanded="expanded === component.id" @click="expanded = expanded === component.id ? '' : component.id">
            <span class="component-name">{{ component.name || component.id }} <small v-if="component.custom">{{ t('componentsPanel.custom') }}</small><small v-if="component.important">{{ t('componentsPanel.required') }}</small><small v-if="component.dependencyOnly">{{ t('componentsPanel.dependency') }}</small></span>
            <code>{{ component.id }}</code>
            <span v-for="loader in component.contents?.embeddedLoaders || []" :key="loader.name" class="component-loader">{{ loader.name }} {{ loader.version }} · {{ t('componentsPanel.embedded') }}</span>
          </button>
          <span class="component-version">{{ component.version || '—' }}</span>
          <PclIconButton :icon="expanded === component.id ? 'chevron-up' : 'chevron-down'" :label="t('componentsPanel.details')" @click="expanded = expanded === component.id ? '' : component.id" />
        </div>
        <div v-for="(problem, index) in component.problems || []" :key="index" class="component-problem" :class="{ severe: problem.severity >= 2 }">{{ problem.description }}</div>
        <div v-if="expanded === component.id" class="component-body">
          <dl>
            <template v-for="detail in details(component)" :key="detail[0]"><dt>{{ detail[0] }}</dt><dd>{{ detail[1] }}</dd></template>
          </dl>
          <details v-if="component.contents?.libraries?.length"><summary>{{ t('componentsPanel.libraries') }} ({{ component.contents.libraries.length }})</summary><pre class="component-libraries">{{ component.contents.libraries.join('\n') }}</pre></details>
          <div class="component-actions">
            <PclButton v-if="component.canChangeVersion" size="sm" :disabled="locked" @click="showVersion(component)">{{ t('componentsPanel.changeVersion') }}</PclButton>
            <PclButton v-if="component.canCustomize" size="sm" :disabled="locked" @click="change('customize', component)">{{ t('componentsPanel.customize') }}</PclButton>
            <PclButton v-if="component.canEdit" size="sm" icon="file" :disabled="locked" @click="editJson(component)">{{ t('componentsPanel.editJson') }}</PclButton>
            <PclIconButton v-if="component.canMove" icon="chevron-up" :label="t('componentsPanel.moveUp')" :disabled="locked || rows.indexOf(component) === 0" @click="change('move', component, { direction: 'up' })" />
            <PclIconButton v-if="component.canMove" icon="chevron-down" :label="t('componentsPanel.moveDown')" :disabled="locked || rows.indexOf(component) === rows.length - 1" @click="change('move', component, { direction: 'down' })" />
            <PclButton v-if="component.canRevert" size="sm" :disabled="locked" @click="confirmChange('revert', component)">{{ t('componentsPanel.revert') }}</PclButton>
            <PclButton v-if="component.canRemove" size="sm" variant="red" :disabled="locked" @click="confirmChange('remove', component)">{{ t('componentsPanel.remove') }}</PclButton>
          </div>
        </div>
      </article>
      <p v-if="!filtered.length">{{ loading ? t('componentsPanel.loading') : t('componentsPanel.empty') }}</p>
    </div>
    <div class="component-actions">
      <PclSelect v-model="importType" :options="importTypes" :aria-label="t('componentsPanel.importType')" :disabled="!canInstall" />
      <PclButton icon="file-input" :disabled="!canInstall" @click="importFiles">{{ t('componentsPanel.import') }}</PclButton>
      <PclButton icon="plus" :disabled="!canInstall" @click="openEmpty">{{ t('componentsPanel.addEmpty') }}</PclButton>
      <PclButton variant="ghost" icon="folder-open" :disabled="working" @click="openFolder('libraries')">{{ t('componentsPanel.localLibraries') }}</PclButton>
      <PclButton variant="ghost" icon="folder-open" :disabled="working" @click="openFolder('game')">{{ t('componentsPanel.gameFolder') }}</PclButton>
    </div>
    <details v-if="allowBrowse && !profile.readOnly" class="component-install-more">
      <summary>{{ t('componentsPanel.installMore') }}</summary>
      <p>{{ t('componentsPanel.installMoreHint') }}</p>
      <div class="component-actions"><PclButton v-for="loader in loaders" :key="loader.uid" :disabled="locked" @click="emit('browse', loader.uid)">{{ loader.name }}</PclButton></div>
    </details>

    <q-dialog v-model="dialog" :persistent="pending">
      <div class="component-dialog" :class="{ 'component-dialog-json': dialogKind === 'json' }">
        <h3>{{ dialogTitle }}</h3>
        <p v-if="dialogKind === 'remove' || dialogKind === 'revert'">{{ t(`componentsPanel.${dialogKind}Warning`) }}</p>
        <PclInput v-if="dialogKind === 'empty'" v-model="newName" :placeholder="t('componentsPanel.name')" :aria-label="t('componentsPanel.name')" />
        <PclInput v-if="dialogKind === 'empty'" v-model="newUid" placeholder="com.example.component" :aria-label="t('componentsPanel.uid')" />
        <template v-if="dialogKind === 'version'"><PclInput v-model="newVersion" :aria-label="t('componentsPanel.version')" /><PclButton v-if="allowBrowse" :disabled="locked" @click="browseVersion">{{ t('componentsPanel.browse') }}</PclButton></template>
        <template v-if="dialogKind === 'json'"><code class="component-json-path">{{ jsonFilename }}</code><PclInput v-model="jsonContent" multiline :rows="18" :aria-label="t('componentsPanel.editJson')" spellcheck="false" /></template>
        <PclHint v-if="dialogError" theme="red">{{ dialogError }}</PclHint>
        <div class="component-actions component-dialog-actions"><PclButton :disabled="pending" @click="dialog = false">{{ t('componentsPanel.cancel') }}</PclButton><PclButton variant="highlight" :disabled="locked" @click="submitDialog">{{ t('componentsPanel.confirm') }}</PclButton></div>
      </div>
    </q-dialog>
  </section>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import PclButton from '../themes/pcl/components/PclButton.vue';
import PclCheckbox from '../themes/pcl/components/PclCheckbox.vue';
import PclHint from '../themes/pcl/components/PclHint.vue';
import PclIconButton from '../themes/pcl/components/PclIconButton.vue';
import PclInput from '../themes/pcl/components/PclInput.vue';
import PclSelect from '../themes/pcl/components/PclSelect.vue';
import { pickPclFiles } from '../themes/pcl/composables/usePclFileDialog';
import { isTaskItemFinished, taskItemPercent } from '../themes/pcl/composables/useTaskProgress';

const props = defineProps<{ instance: string; components: any[]; componentError?: string; busy?: boolean; allowBrowse?: boolean; execute: (op: string, params?: Record<string, unknown>) => Promise<any> }>();
const emit = defineEmits<{ refresh: []; browse: [uid: string]; 'install-start': [info: any]; 'install-progress': [task: any]; 'install-done': [info: any] }>();
const { t } = useI18n();
const rows = ref<any[]>([]), profile = ref({ readOnly: true, running: false, canInstallCustom: false });
const query = ref(''), expanded = ref(''), error = ref(''), loading = ref(false), pending = ref(false), task = ref<any>(null);
const working = computed(() => Boolean(props.busy || loading.value || pending.value));
const locked = computed(() => working.value || profile.value.readOnly || profile.value.running);
const canInstall = computed(() => !locked.value && profile.value.canInstallCustom);
const filtered = computed(() => rows.value.filter(c => `${c.name} ${c.id} ${c.version}`.toLowerCase().includes(query.value.toLowerCase())));
let generation = 0;
const message = (e: unknown) => e instanceof Error ? e.message : String(e);
watch(() => props.components, value => { rows.value = value || []; }, { immediate: true });
watch(() => props.instance, () => { ++generation; rows.value = props.components || []; profile.value = { readOnly: true, running: false, canInstallCustom: false }; expanded.value = ''; error.value = ''; dialog.value = false; pending.value = false; task.value = null; void refresh(); });
onBeforeUnmount(() => { ++generation; });
async function refresh(reload = false) {
  const token = generation, instance = props.instance;
  loading.value = true;
  try {
    const result = await props.execute(reload ? 'instance.components.reload' : 'instance.components.list', { instance });
    if (token !== generation) return;
    rows.value = result.components || [];
    profile.value = { readOnly: result.readOnly !== false, running: Boolean(result.running), canInstallCustom: Boolean(result.canInstallCustom) };
  } catch (e) { if (token === generation) error.value = message(e); }
  finally { if (token === generation) loading.value = false; }
}
async function reload() { error.value = ''; await refresh(true); emit('refresh'); }
async function change(operation: string, component: any, parameters: Record<string, unknown> = {}) {
  if (locked.value) return false;
  return mutate(`instance.component.${operation}`, { component: component.id, ...parameters });
}
async function mutate(operation: string, parameters: Record<string, unknown>) {
  const token = generation;
  pending.value = true; error.value = ''; dialogError.value = '';
  try {
    await props.execute(operation, { instance: props.instance, ...parameters });
    if (token !== generation) return false;
    await refresh(); emit('refresh'); return true;
  } catch (e) { if (token === generation) { error.value = message(e); dialogError.value = message(e); } return false; }
  finally { if (token === generation) pending.value = false; }
}
function details(c: any): string[][] {
  const contents = c.contents || {};
  return [[t('componentsPanel.file'), c.filename], [t('componentsPanel.releaseDate'), c.releaseDate], ['Main class', contents.mainClass], ['Main JAR', contents.mainJar], ['Java', contents.javaVersions?.join(', ')], ['JAR mods', contents.jarModCount ? String(contents.jarModCount) : ''], ['Java agents', contents.agentCount ? String(contents.agentCount) : '']].filter((entry): entry is string[] => Boolean(entry[1]));
}
const importType = ref('component');
const importTypes = computed(() => ['component', 'jarmod', 'jar', 'agent'].map(value => ({ value, label: t(`componentsPanel.types.${value}`) })));
async function importFiles() {
  if (!canInstall.value) return;
  const token = generation, type = importType.value;
  try {
    const files = await pickPclFiles({ title: t('componentsPanel.import'), multiple: type !== 'jar', filters: [{ name: type === 'component' ? 'JSON' : 'JAR / ZIP', extensions: type === 'component' ? ['json'] : ['jar', 'zip'] }] });
    if (token !== generation || !canInstall.value) return;
    for (const source of files) {
      if (token !== generation || !await mutate('instance.component.install-custom', { type, source })) break;
    }
  } catch (e) { if (token === generation) error.value = message(e); }
}
async function openFolder(kind: string) { try { await props.execute('instance.open-folder', { instance: props.instance, kind }); } catch (e) { error.value = message(e); } }
const dialog = ref(false), dialogKind = ref(''), selected = ref<any>(null), dialogError = ref('');
const newName = ref(''), newUid = ref(''), newVersion = ref(''), jsonContent = ref(''), jsonRevision = ref(''), jsonFilename = ref('');
const dialogTitle = computed(() => `${t(`componentsPanel.${dialogKind.value === 'json' ? 'editJson' : dialogKind.value === 'empty' ? 'addEmpty' : dialogKind.value === 'version' ? 'changeVersion' : dialogKind.value || 'details'}`)}${selected.value ? ` · ${selected.value.name || selected.value.id}` : ''}`);
function showDialog(kind: string, c: any = null) { dialogKind.value = kind; selected.value = c; dialogError.value = ''; dialog.value = true; }
function openEmpty() { newName.value = ''; newUid.value = ''; showDialog('empty'); }
function confirmChange(kind: string, c: any) { showDialog(kind, c); }
function showVersion(c: any) { newVersion.value = c.version || ''; showDialog('version', c); }
function browseVersion() { dialog.value = false; emit('browse', selected.value.id); }
async function editJson(c: any) {
  const token = generation;
  pending.value = true;
  try {
    const result = await props.execute('instance.component.read-json', { instance: props.instance, component: c.id });
    if (token !== generation) return;
    jsonContent.value = result.content; jsonRevision.value = result.revision; jsonFilename.value = result.filename; showDialog('json', c);
  } catch (e) { if (token === generation) error.value = message(e); }
  finally { if (token === generation) pending.value = false; }
}
async function submitDialog() {
  if (locked.value) return;
  let success = false;
  if (dialogKind.value === 'empty') success = await mutate('instance.component.add-empty', { component: newUid.value.trim(), name: newName.value.trim() });
  else if (dialogKind.value === 'json') success = await change('write-json', selected.value, { content: jsonContent.value, revision: jsonRevision.value });
  else if (dialogKind.value === 'version') success = await change('set-version', selected.value, { version: newVersion.value.trim() });
  else success = await change(dialogKind.value, selected.value, { confirm: true });
  if (success) dialog.value = false;
}
const loaders = [ { uid: 'net.minecraft', name: 'Minecraft' }, { uid: 'net.minecraftforge', name: 'Forge' }, { uid: 'net.neoforged', name: 'NeoForge' }, { uid: 'com.cleanroommc.cleanroom', name: 'Cleanroom' }, { uid: 'net.fabricmc.fabric-loader', name: 'Fabric' }, { uid: 'org.quiltmc.quilt-loader', name: 'Quilt' }, { uid: 'com.mumfrey.liteloader', name: 'LiteLoader' } ];
async function downloadAll() {
  if (working.value || profile.value.running) return;
  const token = generation;
  const info = { title: t('componentsPanel.downloadAll'), name: props.instance };
  pending.value = true; error.value = ''; emit('install-start', info);
  let failure = '';
  try {
    let current = await props.execute('neo.task.start', { operation: 'instance.update', parameters: { instance: props.instance }, priority: 'high' });
    while (token === generation && current && !isTaskItemFinished(current)) {
      task.value = current; emit('install-progress', current);
      await new Promise(resolve => setTimeout(resolve, 350));
      if (token !== generation) return;
      current = await props.execute('neo.task.status', { taskId: current.taskId });
    }
    if (token !== generation) return;
    emit('install-progress', current);
    if (!current || current.state !== 'succeeded') throw new Error(current?.error || t('componentsPanel.downloadFailed'));
    await refresh(); emit('refresh');
  } catch (e) { failure = message(e); if (token === generation) error.value = failure; }
  finally { if (token === generation) { pending.value = false; task.value = null; emit('install-done', { ...info, error: failure }); } }
}
void refresh();
</script>

<style>
.component-panel.pcl-section-card { display:flex; flex-direction:column; gap:16px; min-width:0; padding:22px; color:var(--pcl-text, var(--neo-text)); background:var(--pcl-card, var(--neo-panel)); border:1px solid var(--pcl-card-border, var(--neo-border)); border-radius:8px; }
.component-toolbar { display:flex; justify-content:space-between; align-items:flex-start; gap:16px; flex-wrap:wrap; }
.component-toolbar h2 { margin:3px 0 8px; font-size:22px; line-height:1.4; }
.component-toolbar p,.component-install-more p { margin:0; opacity:.7; font-size:13px; }
.component-actions { display:flex; flex-wrap:wrap; align-items:center; gap:8px; }
.component-list { border:1px solid var(--pcl-border, #8885); border-radius:8px; overflow:hidden; }
.component-list > p { padding:12px; }
.component-row + .component-row { border-top:1px solid var(--pcl-border, #8885); }
.component-row-head { display:flex; gap:12px; align-items:center; padding:14px; }
.component-order { opacity:.45; font-variant-numeric:tabular-nums; }
.component-heading { display:flex; flex:1; flex-direction:column; gap:4px; min-width:0; text-align:left; color:inherit; background:none; border:0; cursor:pointer; padding:0; }
.component-heading code { font-size:11px; opacity:.6; overflow-wrap:anywhere; }
.component-name { font-weight:600; overflow-wrap:anywhere; }
.component-name small { display:inline-block; font-weight:400; font-size:10px; padding:2px 5px; border:1px solid var(--pcl-border, #8885); border-radius:4px; margin-left:6px; }
.component-loader { font-size:12px; color:var(--pcl-accent, #469de9); }
.component-version { font-size:13px; overflow-wrap:anywhere; max-width:32%; }
.component-row.is-disabled .component-heading { opacity:.5; }
.component-body { padding:0 18px 18px; display:flex; flex-direction:column; gap:14px; }
.component-body dl { display:grid; grid-template-columns:auto minmax(0,1fr); gap:6px 12px; margin:0; font-size:12px; }
.component-body dt { opacity:.6; } .component-body dd { margin:0; overflow-wrap:anywhere; }
.component-problem { padding:6px 16px; color:var(--pcl-warning, #b07810); font-size:12px; white-space:pre-wrap; overflow-wrap:anywhere; max-height:160px; overflow:auto; }
.component-problem.severe { color:var(--pcl-error, #e25a5a); }
.component-libraries { max-height:230px; overflow:auto; font-size:11px; white-space:pre-wrap; overflow-wrap:anywhere; margin:8px 0 0; }
.component-install-more { border-top:1px solid var(--pcl-border, #8885); padding-top:16px; }
.component-install-more summary,.component-body summary { cursor:pointer; font-size:13px; }
.component-install-more p { margin:12px 0; }
.component-dialog { background:var(--pcl-menu-bg, var(--neo-panel)); color:var(--pcl-text, var(--neo-text)); border:1px solid var(--pcl-card-border, var(--neo-border)); padding:22px; border-radius:10px; width:540px; max-width:94vw!important; max-height:90vh; overflow:auto; display:flex; flex-direction:column; gap:14px; }
.component-dialog h3 { font-size:18px; line-height:1.5; margin:0; overflow-wrap:anywhere; }
.component-dialog p { margin:0; white-space:pre-wrap; overflow-wrap:anywhere; }
.component-dialog-json { width:880px; } .component-json-path { font-size:11px; overflow-wrap:anywhere; }
.component-dialog-json textarea { font-family:monospace; max-height:52vh; overflow:auto; resize:vertical; }
.component-dialog-actions { justify-content:flex-end; } .component-progress { display:flex; flex-direction:column; gap:8px; font-size:12px; overflow-wrap:anywhere; }
@media(max-width:600px) { .component-panel {padding:14px;} .component-row-head {gap:8px; padding:12px 8px;} .component-order {display:none;} .component-row-head .pcl-check__label {position:absolute;width:1px;height:1px;overflow:hidden;clip-path:inset(50%);} }
</style>
