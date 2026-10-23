<template>
  <section class="pcl-instance-select">
    <header class="pcl-instance-select__head">
      <PclIconButton icon="arrow-left" label="返回启动页" :size="28" @click="emit('back')" />
      <div>
        <h1>选择 Minecraft 实例</h1>
        <p>选择一个实例后返回启动页。</p>
      </div>
    </header>

    <div class="pcl-instance-select__body">
      <aside class="pcl-instance-select__folders">
        <div class="pcl-instance-select__label">实例文件夹</div>
        <button class="pcl-folder" :class="{ active: activeFolder === '' }" type="button" @click="activeFolder = ''">
          <PclIcon name="box" :size="17" />
          <span>全部实例</span>
          <small>{{ instances.length }}</small>
        </button>
        <button
          v-for="folder in folders"
          :key="folder"
          class="pcl-folder"
          :class="{ active: activeFolder === folder }"
          type="button"
          @click="activeFolder = folder"
        >
          <PclIcon name="folder-open" :size="17" />
          <span>{{ folder }}</span>
          <small>{{ folderCount(folder) }}</small>
        </button>

        <div class="pcl-folder-actions">
          <button type="button" @click="createFolder"><PclIcon name="folder-plus" :size="16" />新建文件夹</button>
          <button type="button" @click="emit('download')"><PclIcon name="file-input" :size="16" />导入整合包</button>
        </div>
      </aside>

      <main class="pcl-instance-select__main">
        <PclSearchBox v-model="search" placeholder="搜索实例" class="pcl-instance-select__search" />

        <div class="pcl-instance-select__list">
          <PclCard
            v-for="group in instanceGroups"
            :key="group.id"
            collapsible
            :open="expandedGroups.has(group.id)"
            @update:open="setGroup(group.id, $event)"
          >
            <template #title>
              <span class="pcl-instance-group-title">{{ group.label }} <small>({{ group.items.length }})</small></span>
            </template>
            <template #actions>
              <span class="pcl-instance-group-hint">{{ group.items.length }} 个实例</span>
            </template>

            <div class="pcl-instance-card__items">
              <div v-for="item in group.items" :key="item.id" class="pcl-instance-row-wrap">
                <PclListItem
                  :title="item.name"
                  :info="instanceInfo(item)"
                  :selected="item.id === selected?.id"
                  @click="emit('select', item)"
                >
                  <template #icon>
                    <img class="pcl-instance-select__icon" :src="icons[item.id] || fallback" :alt="item.name" />
                  </template>
                  <template #actions>
                    <PclIcon v-if="item.running" name="play" :size="14" class="pcl-instance-running" />
                    <PclIconButton icon="settings" label="实例操作" :size="26" @click.stop="openContext(item)" />
                  </template>
                </PclListItem>
              </div>
            </div>
          </PclCard>

          <div v-if="!filteredInstances.length" class="pcl-instance-select__empty">
            <PclIcon name="boxes" :size="38" />
            <strong>没有找到实例</strong>
            <span>可以前往下载页面安装 Minecraft。</span>
            <PclButton variant="highlight" icon="download" @click="emit('download')">打开下载页</PclButton>
          </div>
        </div>
      </main>
    </div>

    <q-menu v-model="contextOpen" class="pcl-context-menu">
      <q-list dense style="min-width: 190px">
        <q-item-label header>{{ contextItem?.name || '实例操作' }}</q-item-label>
        <q-item clickable v-close-popup @click="emit('action', contextItem, 'rename')"><q-item-section avatar><PclIcon name="pencil" :size="16" /></q-item-section><q-item-section>重命名</q-item-section></q-item>
        <q-item clickable v-close-popup @click="emit('action', contextItem, 'group')"><q-item-section avatar><PclIcon name="folder-open" :size="16" /></q-item-section><q-item-section>移动到分组</q-item-section></q-item>
        <q-item clickable v-close-popup @click="emit('action', contextItem, 'copy')"><q-item-section avatar><PclIcon name="copy" :size="16" /></q-item-section><q-item-section>复制实例</q-item-section></q-item>
        <q-item clickable v-close-popup @click="emit('action', contextItem, 'verify')"><q-item-section avatar><PclIcon name="shield-check" :size="16" /></q-item-section><q-item-section>校验文件</q-item-section></q-item>
        <q-item clickable v-close-popup @click="emit('action', contextItem, 'stop')"><q-item-section avatar><PclIcon name="power" :size="16" /></q-item-section><q-item-section>停止实例</q-item-section></q-item>
        <q-separator />
        <q-item clickable v-close-popup @click="emit('action', contextItem, 'delete')"><q-item-section avatar><PclIcon name="trash-2" :size="16" class="pcl-instance-danger" /></q-item-section><q-item-section class="pcl-instance-danger">删除实例</q-item-section></q-item>
      </q-list>
    </q-menu>
  </section>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import fallback from '../../../xmcl-port/assets/minecraft.webp';
import PclSearchBox from '../../components/PclSearchBox.vue';
import PclCard from '../../components/PclCard.vue';
import PclButton from '../../components/PclButton.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclListItem from '../../components/PclListItem.vue';

const props = defineProps<{ instances: any[]; icons: Record<string, string>; selected?: any }>();
const emit = defineEmits<{ select: [instance: any]; back: []; download: []; action: [instance: any, action: string] }>();
const search = ref(''), activeFolder = ref('');
const expandedGroups = ref(new Set<string>());
const contextItem = ref<any>(null), contextOpen = ref(false);
const folders = computed(() => { const values = props.instances.map(item => String(item.group || '').trim()).filter(Boolean); return [...new Set(values)].sort((a, b) => a.localeCompare(b)); });
const filteredInstances = computed(() => { const query = search.value.trim().toLowerCase(); return props.instances.filter(item => (!activeFolder.value || String(item.group || '') === activeFolder.value) && (!query || `${item.name} ${item.group || ''} ${item.runtime?.minecraftVersion || item.minecraftVersion || ''}`.toLowerCase().includes(query))); });
const instanceGroups = computed(() => { const groups = new Map<string, any[]>(); for (const item of filteredInstances.value) { const id = String(item.group || 'regular'); const list = groups.get(id) || []; list.push(item); groups.set(id, list); } return [...groups.entries()].map(([id, items]) => ({ id, label: id === 'regular' ? '常规实例' : id, items })); });
function setGroup(id: string, open: boolean) { const next = new Set(expandedGroups.value); if (open) next.add(id); else next.delete(id); expandedGroups.value = next; }
function ensureGroups() { const next = new Set(expandedGroups.value); for (const group of instanceGroups.value) next.add(group.id); expandedGroups.value = next; }
watch(instanceGroups, ensureGroups, { immediate: true });
function folderCount(folder: string) { return props.instances.filter(item => String(item.group || '') === folder).length; }
function createFolder() { const name = window.prompt('新建实例文件夹名称'); if (name?.trim()) activeFolder.value = name.trim(); }
function openContext(item: any) { contextItem.value = item; contextOpen.value = true; }
function instanceInfo(item: any) {
  const version = item.runtime?.minecraftVersion || item.runtime?.minecraft || item.minecraftVersion || 'Minecraft';
  return `${version}${item.group ? ` · ${item.group}` : ''}`;
}
</script>

<style scoped>
.pcl-instance-select {
  display: flex;
  flex-direction: column;
  width: 100%;
  height: 100%;
  min-height: calc(100vh - 48px);
  overflow: hidden;
  background: var(--pcl-page-bg);
  color: var(--pcl-text);
}

.pcl-instance-select__head {
  display: flex;
  align-items: center;
  gap: 12px;
  flex: 0 0 48px;
  height: 48px;
  padding: 0 25px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-instance-select__head h1 {
  margin: 0;
  color: var(--pcl-text);
  font-size: calc(15px * var(--pcl-font-scale, 1));
  font-weight: 700;
  line-height: 1.2;
}

.pcl-instance-select__head p {
  margin: 2px 0 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-instance-select__body {
  display: grid;
  grid-template-columns: 250px minmax(0, 1fr);
  flex: 1;
  min-height: 0;
}

.pcl-instance-select__folders {
  min-height: 0;
  padding: 12px 0;
  overflow-y: auto;
  background: var(--pcl-nav-bg);
  box-shadow: 4px 0 4px rgba(0, 0, 0, .05);
}

.pcl-instance-select__label {
  padding: 4px 13px 8px;
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  opacity: .6;
}

.pcl-folder {
  position: relative;
  display: flex;
  align-items: center;
  gap: 8px;
  width: 100%;
  height: 36px;
  padding: 0 13px;
  border: 0;
  background: transparent;
  color: var(--pcl-text);
  font: 400 calc(13px * var(--pcl-font-scale, 1))/1 var(--pcl-font);
  text-align: left;
  cursor: pointer;
  transition: background .12s ease, color .12s ease;
}

.pcl-folder:hover {
  background: var(--pcl-hover);
  color: var(--pcl-c2);
}

.pcl-folder.active {
  color: var(--pcl-c3);
}

.pcl-folder.active::before {
  content: "";
  position: absolute;
  left: 0;
  top: 8px;
  width: 5px;
  height: 20px;
  border-radius: 2px;
  background: var(--pcl-c3);
}

.pcl-folder small {
  margin-left: auto;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-folder-actions {
  display: flex;
  flex-direction: column;
  gap: 2px;
  margin: 18px 10px 0;
  padding-top: 10px;
  border-top: 1px solid var(--pcl-border);
}

.pcl-folder-actions button {
  display: flex;
  align-items: center;
  gap: 7px;
  height: 32px;
  padding: 0 9px;
  border: 0;
  border-radius: 3px;
  background: transparent;
  color: var(--pcl-muted);
  font: 400 calc(12px * var(--pcl-font-scale, 1))/1 var(--pcl-font);
  text-align: left;
  cursor: pointer;
}

.pcl-folder-actions button:hover {
  background: var(--pcl-hover);
  color: var(--pcl-c2);
}

.pcl-instance-select__main {
  min-width: 0;
  padding: 25px;
  overflow-y: auto;
}

.pcl-instance-select__search {
  max-width: 520px;
  margin-bottom: 15px;
}

.pcl-instance-select__list {
  display: flex;
  flex-direction: column;
  gap: 15px;
}

.pcl-instance-group-title {
  display: inline-flex;
  align-items: baseline;
  gap: 5px;
}

.pcl-instance-group-title small,
.pcl-instance-group-hint {
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  font-weight: 400;
}

.pcl-instance-card__items {
  display: flex;
  flex-direction: column;
}

.pcl-instance-row-wrap {
  display: flex;
  align-items: center;
}

.pcl-instance-row-wrap :deep(.pcl-list-item) {
  flex: 1;
}

.pcl-instance-select__icon {
  width: 30px;
  height: 30px;
  border-radius: 6px;
  object-fit: cover;
  image-rendering: pixelated;
  image-rendering: crisp-edges;
}

.pcl-instance-running {
  color: #34d399;
}

.pcl-instance-select__empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 7px;
  min-height: 280px;
  color: var(--pcl-muted);
  text-align: center;
}

.pcl-instance-select__empty strong {
  color: var(--pcl-text);
  font-size: calc(14px * var(--pcl-font-scale, 1));
  font-weight: 400;
}

.pcl-instance-select__empty span {
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-instance-danger {
  color: #ff6b6b !important;
}

.pcl-context-menu :deep(.q-item) {
  min-height: 30px;
  color: var(--pcl-text);
}
</style>
