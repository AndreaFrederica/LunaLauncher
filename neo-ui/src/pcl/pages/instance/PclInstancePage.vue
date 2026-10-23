<template>
  <section class="pcl-page pcl-instance-page">
    <aside class="pcl-page-nav pcl-instance-nav">
      <div class="pcl-instance-title">
        <img :src="icon || fallback" :alt="instance.name" />
        <div>
          <strong>{{ instance.name }}</strong>
          <small>{{ details?.minecraftVersion || instance.runtime?.minecraft || 'Minecraft' }}</small>
        </div>
      </div>

      <div class="pcl-instance-nav__groups">
        <div v-for="group in visibleGroups" :key="group.label" class="pcl-instance-nav__group">
          <div class="pcl-instance-nav__label">{{ group.label }}</div>
          <button
            v-for="item in group.items"
            :key="item.id"
            type="button"
            class="pcl-page-nav__item"
            :class="{ active: section === item.id }"
            @click="select(item.id)"
          >
            <PclIcon :name="item.icon" :size="18" />
            <span>{{ item.label }}</span>
          </button>
        </div>
      </div>

      <div class="pcl-instance-actions">
        <PclButton variant="highlight" icon="play" block :disabled="busy || instance.running" @click="emit('launch')">启动游戏</PclButton>
        <PclButton icon="folder-open" block :disabled="busy" @click="emit('open-folder')">打开目录</PclButton>
      </div>
    </aside>

    <main class="pcl-page-main">
      <Transition name="pcl-page" mode="out-in">
        <div :key="section" class="pcl-page-view">
          <div class="pcl-instance-toolbar">
            <div>
              <h1>{{ current.label }}</h1>
              <p>{{ current.description }}</p>
            </div>
            <div class="pcl-actions">
              <PclButton size="sm" icon="refresh-cw" :disabled="busy" @click="emit('refresh')">刷新</PclButton>
            </div>
          </div>

          <PclInstanceOverview
            v-if="section === 'overall'"
            :instance="instance"
            :details="details"
            :icon="icon"
            :notes="notes"
            :busy="busy"
            @launch="emit('launch')"
            @open-folder="emit('open-folder')"
            @settings="select('launch')"
            @delete="emit('delete')"
            @change-icon="emit('change-icon')"
            @save-notes="emit('save-notes', $event)"
            @action="emit('action', $event)"
          />

          <PclMemoryPanel
            v-else-if="section === 'memory'"
            :settings="settings || []"
            :busy="busy"
            :execute="execute"
            @save="emit('save-setting', $event.key, $event.value)"
          />

          <PclServerPanel
            v-else-if="section === 'server'"
            :instance="instance"
            :busy="busy"
            :execute="execute"
            :console-lines="consoleLines"
            :console-state="consoleState"
            @changed="emit('refresh')"
            @console="emit('connect-console')"
            @clear-console="emit('clear-console')"
          />

          <PclSetupPage
            v-else-if="['launch', 'java', 'network', 'advanced'].includes(section)"
            :settings="settings || []"
            :busy="busy"
            :scope="{ scope: 'instance', instance: instance.id }"
            :execute="execute"
            embedded
            :initial-section="section"
            @changed="emit('refresh')"
          />

          <PclInstanceSections
            v-else-if="['install', 'export'].includes(section)"
            :section="section"
            :components="components"
            :settings="settings"
            :busy="busy"
            :instance="instance.id"
            :execute="execute"
            @refresh="emit('refresh')"
            @browse="emit('browse', $event)"
            @component-enabled="(component, enabled) => emit('component-enabled', component, enabled)"
            @component-version="(component, version) => emit('component-version', component, version)"
            @export="emit('export', $event)"
            @save="emit('save-setting', $event.key, $event.value)"
            @console="emit('connect-console')"
          />

          <PclInstanceResourceView v-else-if="resourceKind" :instance="instance.id" :kind="resourceKind" :busy="busy" :execute="execute" @install-start="emit('install-start', $event)" @install-progress="emit('install-progress', $event)" @install-done="emit('install-done', $event)" />

          <PclInstanceExtras v-else-if="section === 'worlds' || section === 'screenshots'" :instance="instance.id" :section="section === 'screenshots' ? 'screenshots' : 'worlds'" :busy="busy" :execute="execute" @changed="emit('refresh')" />

          <PclInstanceServers v-else-if="section === 'servers'" :instance="instance" :busy="busy" :execute="execute" />

          <PclCard v-else-if="section === 'notes'" title="实例笔记">
            <PclInput v-model="localNotes" multiline :rows="9" placeholder="记下这个实例的注意事项、整合包来源或联机信息…" />
            <div class="pcl-actions">
              <PclButton variant="highlight" icon="save" :disabled="busy" @click="emit('save-notes', localNotes)">保存笔记</PclButton>
              <PclButton variant="ghost" icon="rotate-ccw" :disabled="busy" @click="localNotes = notes">还原</PclButton>
            </div>
          </PclCard>

          <div v-else-if="section === 'logs'" class="pcl-instance-logs">
            <PclCard title="日志文件">
              <div v-if="logFiles.length" class="pcl-instance-log-list">
                <button
                  v-for="file in logFiles"
                  :key="file.name || file.path || file"
                  type="button"
                  class="pcl-instance-log-item"
                  :class="{ active: activeLog === (file.name || file.path || file) }"
                  @click="readLog(file)"
                >
                  <PclIcon name="scroll-text" :size="15" />
                  <span>{{ file.name || file.path || file }}</span>
                </button>
              </div>
              <div v-else class="pcl-empty pcl-empty--compact">没有可读取的日志。</div>
            </PclCard>
            <PclCard title="日志内容">
              <pre class="pcl-instance-log-view">{{ fileLog || '选择左侧日志文件后在这里查看内容。' }}</pre>
            </PclCard>
          </div>

          <PclInstanceCustomUi v-else-if="section === 'customui'" :instance="instance" :busy="busy" :execute="execute" />

          <PclCard v-else :title="current.label">
            <div class="pcl-empty">
              <PclIcon name="tool-case" :size="38" />
              <strong>{{ current.label }} 还没有对应的后端操作</strong>
              <span>当前 Qt 页面需要新增实例级 API；此入口先保留，避免把功能藏到旧 UI 里。</span>
              <PclButton icon="arrow-left" @click="section = 'overall'">返回概览</PclButton>
            </div>
          </PclCard>
        </div>
      </Transition>
    </main>
  </section>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclInput from '../../components/PclInput.vue';
import PclMemoryPanel from '../../features/memory/PclMemoryPanel.vue';
import PclInstanceOverview from './PclInstanceOverview.vue';
import PclInstanceSections from './PclInstanceSections.vue';
import PclInstanceResourceView from './PclInstanceResourceView.vue';
import PclInstanceServers from './PclInstanceServers.vue';
import PclInstanceCustomUi from './PclInstanceCustomUi.vue';
import PclServerPanel from './PclServerPanel.vue';
import PclInstanceExtras from './PclInstanceExtras.vue';
import PclSetupPage from '../settings/PclSetupPage.vue';
import fallback from '../../../xmcl-port/assets/minecraft.webp';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ instance: any; details: any; icon?: string; components: any[]; settings?: any[]; componentError?: string; notes: string; busy: boolean; execute: Execute; consoleLines: string[]; consoleState: string; logFiles: any[]; fileLog: string }>();
const emit = defineEmits<{
  launch: []; 'open-folder': []; settings: []; delete: []; 'change-icon': []; 'save-notes': [notes: string]; refresh: [];
  'connect-console': []; 'clear-console': []; 'read-log': [file: string]; 'component-enabled': [component: any, enabled: boolean];
  'component-version': [component: any, version: string]; action: [op: string]; browse: [uid: string]; export: [payload: any];
  'save-setting': [key: string, value: any]; 'install-start': [info: any]; 'install-progress': [task: any]; 'install-done': [info: any];
}>();

const navGroups = [
  {
    label: '游戏本体',
    items: [
      { id: 'overall', label: '概览', icon: 'layout-dashboard', description: '查看实例信息、备注和常用操作。' },
      { id: 'memory', label: '内存', icon: 'gauge', description: '查看系统内存并调整此实例的内存分配。' },
      { id: 'install', label: '修改', icon: 'download', description: '安装或更新 Minecraft 与加载器组件。' },
      { id: 'export', label: '导出', icon: 'file-archive', description: '导出实例或制作整合包。' },
    ],
  },
  {
    label: '实例设置',
    items: [
      { id: 'launch', label: '启动选项', icon: 'rocket', description: '调整此实例的窗口、控制台和启动行为。' },
      { id: 'java', label: 'Java', icon: 'coffee', description: '选择此实例使用的 Java 和 JVM 参数。' },
      { id: 'network', label: '网络代理', icon: 'server', description: '配置此实例使用的代理和联机设置。' },
      { id: 'advanced', label: '高级', icon: 'tool-case', description: '渲染器、JVM 参数、游戏参数和启动前命令。' },
    ],
  },
  {
    label: '游戏资源',
    items: [
      { id: 'worlds', label: '存档', icon: 'earth', description: '管理存档、导入世界和导出备份。' },
      { id: 'screenshots', label: '截图', icon: 'image', description: '浏览和管理游戏截图。' },
      { id: 'mods', label: '模组', icon: 'puzzle', description: '管理已安装模组。' },
      { id: 'coremods', label: '核心模组', icon: 'package', description: '管理核心模组。' },
      { id: 'nilmods', label: 'NilMod', icon: 'package-plus', description: '管理 NilMod。' },
      { id: 'resourcepacks', label: '资源包', icon: 'palette', description: '管理资源包。' },
      { id: 'texturepacks', label: '材质包', icon: 'image', description: '管理旧版材质包。' },
      { id: 'shaderpacks', label: '光影包', icon: 'sparkles', description: '管理光影包。' },
      { id: 'datapacks', label: '数据包', icon: 'layers', description: '管理全局数据包。' },
      { id: 'yesstevemodels', label: 'YSM模型', icon: 'shirt', description: '管理 Yes Steve Model 模型。' },
      { id: 'customplayermodels', label: '自定义玩家模型', icon: 'user', description: '管理自定义玩家模型。' },
      { id: 'schematics', label: '投影原理图', icon: 'grid-2x2', description: '管理投影原理图。' },
    ],
  },
  {
    label: '实例工具',
    items: [
      { id: 'servers', label: '多人服务器', icon: 'network', description: '管理 servers.dat 多人服务器列表。' },
      { id: 'notes', label: '笔记', icon: 'scroll-text', description: '记录实例备注。' },
      { id: 'logs', label: '日志', icon: 'scroll', description: '查看实例日志文件。' },
      { id: 'server', label: '服务端', icon: 'server', description: '管理服务端配置和终端。' },
      { id: 'customui', label: '自定义 UI', icon: 'layout-list', description: 'Qt 版的自定义 UI 面板。' },
    ],
  },
];
const allItems = navGroups.flatMap(group => group.items);
const isServer = computed(() => /server/i.test(String(props.instance?.type || props.details?.type || '')));
const visibleGroups = computed(() => navGroups
  .map(group => ({ ...group, items: group.items.filter(item => (item.id !== 'server' || isServer.value) && (item.id !== 'servers' || !isServer.value)) }))
  .filter(group => group.items.length));
const section = ref('overall');
const current = computed(() => allItems.find(item => item.id === section.value) || allItems[0]);
const resourceKind = computed(() => ({
  mods: 'mods', coremods: 'coremods', nilmods: 'nilmods', resourcepacks: 'resourcepacks',
  texturepacks: 'texturepacks', shaderpacks: 'shaderpacks', datapacks: 'datapacks',
  yesstevemodels: 'yesstevemodels', customplayermodels: 'customplayermodels', schematics: 'schematics',
} as Record<string, string>)[section.value] || '');
const localNotes = ref(props.notes);
const activeLog = ref('');
watch(() => props.notes, value => { localNotes.value = value; });
watch(() => props.fileLog, () => { /* fileLog is rendered directly */ });

function select(id: string) {
  section.value = id;
  if (id === 'logs' && !activeLog.value && props.logFiles.length) readLog(props.logFiles[0]);
}
function readLog(file: any) {
  const name = String(file?.name || file?.path || file || '');
  activeLog.value = name;
  emit('read-log', name);
}
</script>

<style scoped>
.pcl-instance-page {
  max-width: 1420px;
  margin: 0 auto;
}

.pcl-instance-nav {
  width: 220px;
  min-width: 220px;
  padding: 14px 0 16px;
}

.pcl-instance-title {
  display: flex;
  align-items: center;
  gap: 9px;
  padding: 0 13px 12px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-instance-title img {
  width: 38px;
  height: 38px;
  border-radius: 4px;
  object-fit: cover;
  image-rendering: pixelated;
}

.pcl-instance-title strong {
  display: block;
  max-width: 145px;
  overflow: hidden;
  color: var(--pcl-text);
  font-size: 13px;
  font-weight: 700;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-instance-title small {
  display: block;
  margin-top: 3px;
  color: var(--pcl-muted);
  font-size: 10px;
}

.pcl-instance-nav__groups {
  flex: 1;
  min-height: 0;
  padding: 6px 0;
  overflow-y: auto;
}

.pcl-instance-nav__group {
  padding: 3px 0;
}

.pcl-instance-nav__label {
  padding: 6px 13px 4px;
  color: var(--pcl-muted);
  font-size: 10px;
  font-weight: 700;
  letter-spacing: .08em;
  opacity: .8;
}

.pcl-instance-actions {
  display: flex;
  flex-direction: column;
  gap: 6px;
  padding: 10px 12px 0;
  border-top: 1px solid var(--pcl-border);
}

.pcl-instance-toolbar {
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  gap: 12px;
  padding-bottom: 14px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-instance-toolbar h1 {
  margin: 0;
  color: var(--pcl-text);
  font-size: 19px;
  font-weight: 700;
}

.pcl-instance-toolbar p {
  margin: 3px 0 0;
  color: var(--pcl-muted);
  font-size: 11px;
}

.pcl-instance-logs {
  display: grid;
  grid-template-columns: 280px minmax(0, 1fr);
  gap: 12px;
}

.pcl-instance-log-list {
  display: flex;
  flex-direction: column;
}

.pcl-instance-log-item {
  display: flex;
  align-items: center;
  gap: 7px;
  width: 100%;
  min-height: 32px;
  padding: 0 8px;
  overflow: hidden;
  border: 0;
  border-bottom: 1px solid var(--pcl-border);
  background: transparent;
  color: var(--pcl-text);
  font: 400 12px/1 var(--pcl-font);
  text-align: left;
  text-overflow: ellipsis;
  white-space: nowrap;
  cursor: pointer;
}

.pcl-instance-log-item:hover,
.pcl-instance-log-item.active {
  background: var(--pcl-c7);
  color: var(--pcl-c2);
}

.pcl-instance-log-view {
  min-height: 360px;
  max-height: 60vh;
  margin: 0;
  overflow: auto;
  padding: 10px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 4px;
  background: var(--pcl-c8);
  color: var(--pcl-text);
  white-space: pre-wrap;
  font: 11px/1.6 ui-monospace, Consolas, monospace;
}
</style>
