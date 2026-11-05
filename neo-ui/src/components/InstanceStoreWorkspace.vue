<template>
  <section class="instance-store-workspace" :class="`is-${variant}`">
    <header class="instance-store-workspace__header">
      <div>
        <p class="instance-store-workspace__eyebrow">// INSTANCE STORAGE</p>
        <h2>实例目录与兼容格式</h2>
        <p>管理多个 Prism、PCL、HMCL 和原版目录，并在需要时进行安全转换。</p>
      </div>
      <PclButton type="button" class="instance-store-workspace__button" :disabled="busy" @click="loadAll">刷新</PclButton>
    </header>

    <p v-if="error" class="instance-store-workspace__message is-error">{{ error }}</p>
    <p v-if="notice" class="instance-store-workspace__message is-success">{{ notice }}</p>

    <div class="instance-store-workspace__grid">
      <article class="instance-store-card instance-store-card--wide">
        <div class="instance-store-card__title"><strong>实例 Store</strong><small>{{ stores.length }} 个已注册目录</small></div>
        <div class="instance-store-list">
          <div v-for="store in stores" :key="store.id" class="instance-store-row">
            <div class="instance-store-row__main">
              <strong>{{ store.name || store.id }}</strong>
              <small>{{ store.format === 'vanilla-compatible' ? 'PCL / HMCL / Mojang' : 'Prism / MMC' }} · {{ store.path }} · {{ store.exists === false ? '目录不存在' : '目录存在' }} · {{ store.scanState === 'disabled' ? '未启用' : store.scanState === 'missing' ? '无法扫描' : `已发现 ${store.instanceCount ?? 0} 个实例` }}</small>
            </div>
            <div class="instance-store-row__flags">
              <PclCheckbox :model-value="store.enabled" label="启用" :disabled="busy" @update:model-value="updateStore(store, { enabled: $event })" />
              <PclCheckbox :model-value="store.writable" label="可写" :disabled="busy" @update:model-value="updateStore(store, { writable: $event })" />
              <PclRadio v-if="store.format === 'prism'" name="default-instance-store" :model-value="store.defaultForNewInstances ? store.id : ''" :value="store.id" label="新实例默认" :disabled="busy" @update:model-value="updateStore(store, { defaultForNewInstances: true })" />
              <button type="button" class="instance-store-workspace__link" :disabled="busy" @click="openStore(store)">打开目录</button>
              <button v-if="store.origin !== 'portable-derived'" type="button" class="instance-store-workspace__link" :disabled="busy" @click="removeStore(store)">移除</button>
            </div>
          </div>
          <p v-if="!stores.length" class="instance-store-workspace__empty">尚未注册实例目录。</p>
        </div>
        <form class="instance-store-form" @submit.prevent="addStore">
          <PclInput v-model="newStore.name" placeholder="显示名称" aria-label="Store 名称" />
          <PclInput v-model="newStore.path" placeholder="目录路径" aria-label="Store 路径" />
          <PclSelect v-model="newStore.format" aria-label="Store 格式" :options="[{ value: 'prism', label: 'Prism / MMC' }, { value: 'vanilla-compatible', label: 'PCL / HMCL / Mojang' }]" />
          <PclButton type="submit" class="instance-store-workspace__button" :disabled="busy || !newStore.path">添加目录</PclButton>
        </form>
      </article>

      <article class="instance-store-card">
        <div class="instance-store-card__title"><strong>便携模式</strong><small>{{ location?.portable ? '当前启用' : '当前关闭' }}</small></div>
        <div class="instance-store-location"><span>当前数据目录</span><code>{{ location?.current || '—' }}</code></div>
        <div class="instance-store-location"><span>便携目录</span><code>{{ location?.portablePath || '—' }}</code></div>
        <div class="instance-store-actions">
          <PclButton type="button" class="instance-store-workspace__button" :disabled="busy || location?.portable" @click="setPortable(true)">启用便携模式</PclButton>
          <PclButton type="button" class="instance-store-workspace__button is-muted" :disabled="busy || !location?.portable" @click="setPortable(false)">使用用户目录</PclButton>
        </div>
        <small class="instance-store-workspace__hint">切换只改变启动器数据配置；实例目录不会被强制迁移。</small>
      </article>

      <article class="instance-store-card instance-store-card--wide">
        <div class="instance-store-card__title"><strong>兼容目录与转换</strong><button type="button" class="instance-store-workspace__link" :disabled="busy" @click="loadVanilla">扫描</button></div>
        <div class="instance-store-list">
          <div v-for="item in vanillaInstances" :key="item.id" class="instance-store-row">
            <div class="instance-store-row__main"><strong>{{ item.name }}</strong><small>{{ item.dialect.toUpperCase() }} · {{ item.isolated ? '版本隔离' : '共享 .minecraft' }} · {{ item.root }}</small></div>
            <button type="button" class="instance-store-workspace__link" :disabled="busy || !prismStores.length" @click="importVanilla(item)">导入 Prism</button>
          </div>
          <p v-if="!vanillaInstances.length" class="instance-store-workspace__empty">没有扫描到 PCL/HMCL/Mojang 实例。</p>
        </div>
        <div class="instance-store-form instance-store-form--stack">
          <label>导入实例名称<PclInput v-model="importOptions.name" placeholder="留空使用源实例名称" /></label>
          <label>导入分组<PclInput v-model="importOptions.group" placeholder="可选，例如 Forge 服务器" /></label>
        </div>
        <p class="instance-store-workspace__hint">点击实例右侧的“导入 Prism”时会使用这里的名称和分组；留空则保留源名称。</p>
        <div class="instance-store-form instance-store-form--stack">
          <label>Prism 实例源目录<PclInput v-model="conversion.source" placeholder="source"  /></label>
          <label>原版目标目录<PclInput v-model="conversion.target" placeholder="target"  /></label>
          <label>版本 ID<PclInput v-model="conversion.versionId" placeholder="1.20.1"  /></label>
          <label>方言<PclSelect v-model="conversion.dialect" :options="[{ value: 'pcl', label: 'PCL' }, { value: 'hmcl', label: 'HMCL' }, { value: 'mojang', label: 'Mojang' }]" /></label>
          <label>导出实例名称<PclInput v-model="conversion.name" placeholder="可选" /></label>
          <label>导出分组<PclInput v-model="conversion.group" placeholder="可选" /></label>
          <PclButton type="button" class="instance-store-workspace__button" :disabled="busy || !conversion.source || !conversion.target || !conversion.versionId" @click="exportPrism">导出到兼容目录</PclButton>
        </div>
        <p class="instance-store-workspace__hint">导出会拒绝 MMC 专属数据；普通 mods、config、saves 和启动器可识别的元数据可以复制。</p>
      </article>

      <article class="instance-store-card">
        <div class="instance-store-card__title"><strong>Prism Store 迁移</strong><small>单个实例 copy / move</small></div>
        <label>实例 ID<PclInput v-model="transfer.instance" placeholder="实例 ID"  /></label>
        <label>目标 Store<PclSelect v-model="transfer.targetStoreId" placeholder="选择 Store" :options="prismStores.map(store => ({ value: store.id, label: store.name || store.id }))" /></label>
        <label>模式<PclSelect v-model="transfer.mode" :options="[{ value: 'copy', label: '复制' }, { value: 'move', label: '移动' }]" /></label>
        <PclButton type="button" class="instance-store-workspace__button" :disabled="busy || !transfer.instance || !transfer.targetStoreId" @click="transferInstance">执行迁移</PclButton>
      </article>

      <article class="instance-store-card">
        <div class="instance-store-card__title"><strong>标准 Loader</strong><small>写入兼容版本 JSON</small></div>
        <label>目录<PclInput v-model="loader.target" placeholder="PCL/HMCL 目录"  /></label>
        <label>基础版本<PclInput v-model="loader.versionId" placeholder="1.20.1"  /></label>
        <label>Loader ID<PclInput v-model="loader.loaderId" placeholder="net.fabricmc.fabric-loader"  /></label>
        <label>Loader 版本<PclInput v-model="loader.loaderVersion" placeholder="0.16.5"  /></label>
        <PclButton type="button" class="instance-store-workspace__button" :disabled="busy || !loader.target || !loader.versionId || !loader.loaderId || !loader.loaderVersion" @click="installLoader">安装 Loader</PclButton>
      </article>

      <article class="instance-store-card instance-store-card--wide">
        <div class="instance-store-card__title"><strong>客户端 / 服务端同步</strong><small>声明式 mod、config 和冲突预览</small></div>
        <div class="instance-store-form instance-store-form--stack">
          <label>配置文件 ID<PclInput v-model="sync.id" placeholder="server-pack"  /></label>
          <label>客户端目录<PclInput v-model="sync.clientRoot" placeholder="client root"  /></label>
          <label>服务端目录<PclInput v-model="sync.serverRoot" placeholder="server root"  /></label>
          <label>声明配置 JSON<PclInput v-model="sync.profileJson" multiline :rows="8" /></label>
        </div>
        <div class="instance-store-actions"><PclButton type="button" class="instance-store-workspace__button" :disabled="busy" @click="saveSync">保存配置</PclButton><PclButton type="button" class="instance-store-workspace__button is-muted" :disabled="busy" @click="planSync">预览同步</PclButton><PclButton type="button" class="instance-store-workspace__button is-danger" :disabled="busy || !syncPlan?.valid" @click="applySync">应用同步</PclButton></div>
        <pre v-if="syncPlan" class="instance-store-workspace__output">{{ JSON.stringify(syncPlan, null, 2) }}</pre>
      </article>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, onMounted, reactive, ref } from 'vue';
import PclButton from '../themes/pcl/components/PclButton.vue';
import PclCheckbox from '../themes/pcl/components/PclCheckbox.vue';
import PclInput from '../themes/pcl/components/PclInput.vue';
import PclRadio from '../themes/pcl/components/PclRadio.vue';
import PclSelect from '../themes/pcl/components/PclSelect.vue';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = withDefaults(defineProps<{ execute: Execute; variant?: string }>(), { variant: 'default' });
const stores = ref<any[]>([]), vanillaInstances = ref<any[]>([]), location = ref<any>(null), syncPlan = ref<any>(null);
const busy = ref(false), error = ref(''), notice = ref('');
const newStore = reactive({ name: '', path: '', format: 'prism' });
const conversion = reactive({ source: '', target: '', versionId: '', dialect: 'pcl', name: '', group: '' });
const importOptions = reactive({ name: '', group: '' });
const transfer = reactive({ instance: '', targetStoreId: '', mode: 'copy' });
const loader = reactive({ target: '', versionId: '', loaderId: 'net.fabricmc.fabric-loader', loaderVersion: '' });
const sync = reactive({ id: '', clientRoot: '', serverRoot: '', profileJson: '{\n  "id": "server-pack",\n  "name": "Server pack",\n  "client": { "instanceId": "client" },\n  "server": { "instanceId": "server" },\n  "mods": { "common": [], "clientOnly": [], "serverOnly": [] },\n  "configs": { "common": [], "clientOnly": [], "serverOnly": [] },\n  "sync": { "mode": "bidirectional", "conflictPolicy": "ask", "baseHashes": {}, "baseContents": {} }\n}' });
const prismStores = computed(() => stores.value.filter(store => store.format === 'prism' && store.enabled && store.writable));

function clearMessage() { error.value = ''; notice.value = ''; }
async function run(operation: string, parameters: Record<string, unknown> = {}) {
  clearMessage();
  try { return await props.execute(operation, parameters); }
  catch (exception) { error.value = exception instanceof Error ? exception.message : String(exception); throw exception; }
}
async function loadStores() { const data = await run('instance-store.list'); stores.value = data?.stores || []; }
async function loadLocation() { location.value = await run('startup.data-location'); }
async function loadVanilla() { const data = await run('vanilla-instance.list'); vanillaInstances.value = data?.instances || []; }
async function loadAll() { busy.value = true; try { await Promise.all([loadStores(), loadLocation(), loadVanilla()]); notice.value = '实例目录状态已刷新。'; } finally { busy.value = false; } }
async function addStore() { busy.value = true; try { await waitForTask('instance-store.add', { ...newStore, enabled: true, writable: true }); Object.assign(newStore, { name: '', path: '' }); await loadStores(); notice.value = '实例目录已添加。'; } finally { busy.value = false; } }
async function updateStore(store: any, changes: Record<string, unknown>) { busy.value = true; try { await waitForTask('instance-store.update', { id: store.id, path: store.path, format: store.format, name: store.name, ...changes }); await loadStores(); } finally { busy.value = false; } }
async function openStore(store: any) { try { await run('desktop.open-path', { path: store.path }); notice.value = `已打开「${store.name || store.id}」。`; } catch { /* run() exposes the backend error in the workspace message. */ } }
async function removeStore(store: any) { if (!window.confirm(`从注册表移除「${store.name || store.id}」？目录文件不会被删除。`)) return; busy.value = true; try { await waitForTask('instance-store.remove', { id: store.id }); await loadStores(); notice.value = '实例目录已从注册表移除。'; } finally { busy.value = false; } }
async function setPortable(portable: boolean) { busy.value = true; try { await waitForTask('startup.set-data-location', { portable }); await loadLocation(); notice.value = '模式切换将在重启启动器后生效。'; } finally { busy.value = false; } }
async function waitForTask(operation: string, parameters: Record<string, unknown>) {
  let task = await run('neo.task.start', { operation, priority: 'normal', parameters });
  if (!task?.taskId) throw new Error('异步任务未能启动。');
  // The shell task center listens for this event and opens immediately. The
  // local polling keeps the conversion button's result and error state in sync
  // even when a theme has no always-visible task panel.
  window.dispatchEvent(new CustomEvent('luna-neo-task-started', { detail: task }));
  while (task && !task.finished) {
    await new Promise(resolve => setTimeout(resolve, 350));
    task = await run('neo.task.status', { taskId: task.taskId });
  }
  if (!task || task.state !== 'succeeded') throw new Error(task?.error || '操作失败。');
  return task;
}
async function importVanilla(item: any) { const store = prismStores.value[0]; if (!store) return; busy.value = true; try { await waitForTask('instance.convert.import', { source: item.root, targetStoreId: store.id, name: importOptions.name.trim() || item.name, group: importOptions.group.trim(), copyGameContent: true }); await loadStores(); await loadVanilla(); notice.value = '兼容实例已导入 Prism Store。'; } finally { busy.value = false; } }
async function exportPrism() { busy.value = true; try { await waitForTask('instance.convert.export', { ...conversion, copyGameContent: true }); await loadVanilla(); notice.value = 'Prism 实例已导出到兼容目录。'; } finally { busy.value = false; } }
async function transferInstance() { busy.value = true; try { await waitForTask('instance.store-transfer', { ...transfer }); await loadStores(); notice.value = '实例迁移完成。'; } finally { busy.value = false; } }
async function installLoader() { busy.value = true; try { await waitForTask('vanilla.loader.install', { ...loader }); await loadVanilla(); notice.value = 'Loader profile 已写入兼容目录。'; } finally { busy.value = false; } }
function profile() { const parsed = JSON.parse(sync.profileJson); if (sync.id.trim()) parsed.id = sync.id.trim(); return parsed; }
async function saveSync() { busy.value = true; try { const value = profile(); await waitForTask('sync.profile.save', { profile: value }); sync.id = value.id || sync.id; notice.value = '同步配置已保存。'; } catch (exception) { if (exception instanceof SyntaxError) error.value = `同步配置 JSON 无效：${exception.message}`; } finally { busy.value = false; } }
async function planSync() { busy.value = true; try { syncPlan.value = await run('sync.profile.plan', { id: sync.id || undefined, profile: profile(), clientRoot: sync.clientRoot, serverRoot: sync.serverRoot }); notice.value = '同步预览已生成。'; } catch (exception) { if (exception instanceof SyntaxError) error.value = `同步配置 JSON 无效：${exception.message}`; } finally { busy.value = false; } }
async function applySync() { busy.value = true; try { await waitForTask('sync.profile.apply', { id: sync.id || undefined, profile: profile(), clientRoot: sync.clientRoot, serverRoot: sync.serverRoot }); notice.value = '同步已应用。'; } finally { busy.value = false; } }
onMounted(() => { void loadAll().catch(() => {}); });
</script>

<style scoped>
/* Shared workspace tokens are mapped to the active shell. This keeps the
   component readable in every theme and prevents native controls from falling
   back to the browser's default light styling. */
.instance-store-workspace {
  --instance-store-accent: var(--neo-primary, #3b82f6);
  --instance-store-card-bg: var(--neo-panel, rgba(255,255,255,.94));
  --instance-store-row-bg: color-mix(in srgb, var(--instance-store-accent) 9%, transparent);
  --instance-store-fg: var(--neo-text, #1f2937);
  --instance-store-muted: var(--neo-muted, #64748b);
  --instance-store-border: var(--neo-border, rgba(100,116,139,.28));
  --instance-store-input-bg: var(--neo-panel-raised, #fff);
  display:block; min-width:0; color:var(--instance-store-fg); font:inherit; line-height:1.45;
}
.instance-store-workspace.is-pcl {
  --instance-store-accent: var(--pcl-c3, #2a80e3);
  --instance-store-card-bg: var(--pcl-card, var(--neo-panel, rgba(255,255,255,.94)));
  --instance-store-row-bg: var(--pcl-selected, rgba(42,128,227,.12));
  --instance-store-fg: var(--pcl-text, var(--neo-text, #e9f3ff));
  --instance-store-muted: var(--pcl-muted, #a9b3c2);
  --instance-store-border: var(--pcl-border, rgba(255,255,255,.1));
  --instance-store-input-bg: var(--pcl-half-white, rgba(40,40,40,.33));
}
.instance-store-workspace.is-xmcl {
  --instance-store-accent: var(--neo-primary, #818cf4);
  --instance-store-card-bg: var(--neo-panel, rgba(35,35,42,.76));
  --instance-store-row-bg: rgba(255,255,255,.06);
  --instance-store-fg: var(--neo-text, rgba(255,255,255,.9));
  --instance-store-muted: var(--neo-muted, rgba(255,255,255,.58));
  --instance-store-border: var(--neo-border, rgba(255,255,255,.14));
  --instance-store-input-bg: var(--neo-panel-raised, rgba(0,0,0,.18));
}
.instance-store-workspace.is-endfield {
  --instance-store-accent: var(--ef-signal, #fffa00);
  --instance-store-card-bg: var(--ef-panel, rgba(245,240,224,.9));
  --instance-store-row-bg: rgba(37,35,29,.06);
  --instance-store-fg: var(--ef-ink, #25231d);
  --instance-store-muted: var(--ef-ink-2, #696454);
  --instance-store-border: var(--ef-rule, rgba(37,35,29,.25));
  --instance-store-input-bg: var(--ef-panel, #fff);
}
.instance-store-workspace.is-melody {
  --instance-store-accent: var(--melody-focus, #174fd2);
  --instance-store-card-bg: var(--melody-input-fill, rgba(240,248,250,.58));
  --instance-store-row-bg: rgba(213,168,255,.08);
  --instance-store-fg: var(--melody-ink, rgba(23,55,67,.92));
  --instance-store-muted: var(--melody-line, rgba(28,62,75,.7));
  --instance-store-border: var(--melody-line, rgba(28,62,75,.7));
  --instance-store-input-bg: var(--melody-input-fill, rgba(240,248,250,.58));
}
.instance-store-workspace__header{display:flex;align-items:flex-start;justify-content:space-between;gap:20px;margin-bottom:18px}
.instance-store-workspace__eyebrow{margin:0 0 5px;color:var(--instance-store-accent);font-size:11px;font-weight:800;letter-spacing:.12em}
.instance-store-workspace h2{margin:0;color:var(--instance-store-fg);font-size:clamp(22px,3vw,30px);line-height:1.15}
.instance-store-workspace__header>div>p:last-child{margin:7px 0 0;color:var(--instance-store-muted);font-size:13px}
.instance-store-workspace__grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}
.instance-store-card{min-width:0;padding:16px;border:1px solid var(--instance-store-border);border-radius:var(--pcl-radius,10px);background:var(--instance-store-card-bg);box-shadow:var(--pcl-shadow,0 8px 24px rgba(15,23,42,.08))}
.instance-store-card--wide{grid-column:1/-1}
.instance-store-card__title{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:12px}
.instance-store-card__title strong{color:var(--instance-store-fg);font-size:16px}
.instance-store-card__title small,.instance-store-row small,.instance-store-card small{color:var(--instance-store-muted);font-size:12px}
.instance-store-list{display:grid;gap:8px}
.instance-store-row{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:10px 12px;border:1px solid transparent;border-radius:var(--pcl-radius,8px);background:var(--instance-store-row-bg)}
.instance-store-row__main{min-width:0;display:grid;gap:2px}
.instance-store-row__main strong,.instance-store-row__main small{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.instance-store-row__flags{display:flex;align-items:center;justify-content:flex-end;flex-wrap:wrap;gap:8px 12px;flex-shrink:0}
.instance-store-row__flags label{display:inline-flex;align-items:center;gap:5px;color:var(--instance-store-muted);font-size:12px;white-space:nowrap}
.instance-store-workspace input:not([type='checkbox']):not([type='radio']),.instance-store-workspace select,.instance-store-workspace textarea{box-sizing:border-box;width:100%;min-width:0;border:1px solid var(--instance-store-border);border-radius:var(--pcl-radius,6px);background:var(--instance-store-input-bg);color:var(--instance-store-fg);font:inherit;font-size:13px;padding:8px 10px;outline:none}
.instance-store-workspace textarea{resize:vertical;min-height:120px;font-family:ui-monospace,SFMono-Regular,Consolas,monospace;line-height:1.45}
.instance-store-workspace input:not([type='checkbox']):not([type='radio']):focus,.instance-store-workspace select:focus,.instance-store-workspace textarea:focus{border-color:var(--instance-store-accent);box-shadow:0 0 0 2px color-mix(in srgb,var(--instance-store-accent) 22%,transparent)}
.instance-store-workspace label{display:grid;gap:5px;color:var(--instance-store-muted);font-size:12px}
.instance-store-workspace :deep(.pcl-select){width:100%;min-width:0}
.instance-store-workspace :deep(.pcl-select__control){height:32px;min-height:32px;padding:0 9px;border:1px solid var(--instance-store-border);border-radius:var(--pcl-radius,6px);background:var(--instance-store-input-bg);color:var(--instance-store-fg);font:inherit;font-size:13px}
.instance-store-workspace :deep(.pcl-select__control:hover),.instance-store-workspace :deep(.pcl-select__control.open){border-color:var(--instance-store-accent);background:var(--instance-store-row-bg)}
.instance-store-row__flags :deep(.pcl-check),.instance-store-row__flags :deep(.pcl-radio){display:inline-flex;align-items:center;gap:5px;color:var(--instance-store-muted);font-size:12px;white-space:nowrap}
.instance-store-form{display:grid;grid-template-columns:1fr 1.6fr 1fr auto;align-items:end;gap:8px;margin-top:12px}
.instance-store-form--stack{grid-template-columns:repeat(2,minmax(0,1fr))}
.instance-store-form--stack button{grid-column:1/-1;justify-self:start}
.instance-store-location{display:grid;gap:4px;margin:10px 0;color:var(--instance-store-muted);font-size:12px}
.instance-store-location code{overflow:auto;padding:7px 8px;border-radius:6px;background:var(--instance-store-row-bg);color:var(--instance-store-fg);font:12px ui-monospace,SFMono-Regular,Consolas,monospace}
.instance-store-actions{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px}
.instance-store-workspace__button,.instance-store-workspace__link{border:0;cursor:pointer;font:inherit}
.instance-store-workspace__button{min-height:32px;padding:0 14px;border:1px solid var(--instance-store-accent);border-radius:var(--pcl-radius,5px);background:var(--instance-store-accent);color:#fff;font-size:13px;font-weight:700}
.is-endfield .instance-store-workspace__button{color:var(--instance-store-fg)}
.instance-store-workspace__button.is-muted{border-color:var(--instance-store-border);background:var(--instance-store-row-bg);color:var(--instance-store-fg)}
.instance-store-workspace__button.is-danger{border-color:var(--pcl-danger,#c2410c);background:var(--pcl-danger,#c2410c)}
.instance-store-workspace__button:hover:not(:disabled){filter:brightness(1.08)}
.instance-store-workspace__button:disabled,.instance-store-workspace__link:disabled{cursor:not-allowed;opacity:.5}
.instance-store-workspace__link{padding:2px 0;background:transparent;color:var(--instance-store-accent);font-size:12px;text-decoration:underline;text-underline-offset:2px}
.instance-store-workspace__message{margin:0 0 12px;padding:9px 11px;border-radius:7px;font-size:13px}
.instance-store-workspace__message.is-error{background:rgba(220,38,38,.12);color:var(--pcl-danger,#b91c1c)}
.instance-store-workspace__message.is-success{background:rgba(22,163,74,.12);color:#22c55e}
.instance-store-workspace__hint,.instance-store-workspace__empty{display:block;margin:10px 0 0;color:var(--instance-store-muted);font-size:12px}
.instance-store-workspace__output{max-height:260px;overflow:auto;margin:12px 0 0;padding:10px;border:1px solid var(--instance-store-border);border-radius:7px;background:var(--instance-store-row-bg);color:var(--instance-store-fg);font:12px/1.45 ui-monospace,SFMono-Regular,Consolas,monospace;white-space:pre-wrap}
@media (max-width:900px){.instance-store-workspace__grid{grid-template-columns:1fr}.instance-store-form{grid-template-columns:1fr 1fr}.instance-store-form>button{grid-column:1/-1;justify-self:start}}
@media (max-width:620px){.instance-store-workspace__header{display:block}.instance-store-workspace__header>.instance-store-workspace__button{margin-top:12px}.instance-store-row{align-items:flex-start;flex-direction:column}.instance-store-row__flags{justify-content:flex-start}.instance-store-form,.instance-store-form--stack{grid-template-columns:1fr}.instance-store-form--stack button{grid-column:auto}}
</style>

