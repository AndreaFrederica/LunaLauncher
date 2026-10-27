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
        <PclButton variant="highlight" icon="play" block :disabled="busy || instance.running" @click="emit('launch')">{{ t('instance.launch') }}</PclButton>
        <PclButton icon="folder-open" block :disabled="busy" @click="emit('open-folder')">{{ t('instance.openFolder') }}</PclButton>
      </div>
    </aside>

    <main class="pcl-page-main">
      <div class="pcl-page-view">
          <div class="pcl-instance-toolbar">
            <div>
              <h1>{{ current.label }}</h1>
              <p>{{ current.description }}</p>
            </div>
            <div class="pcl-actions">
              <PclButton size="sm" icon="refresh-cw" :disabled="busy" @click="emit('refresh')">{{ t('instance.refresh') }}</PclButton>
            </div>
          </div>

          <PclInstanceLiveLog
            v-if="section === 'live'"
            :instance="instance"
            :execute="execute"
          />

          <PclInstanceOverview
            v-else-if="section === 'overall'"
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
            v-else-if="['launch', 'java', 'network', 'advanced', 'advanced-settings'].includes(section)"
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
            :component-error="componentError"
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

          <PclCard v-else-if="section === 'notes'" :title="t('pcl.auto2.kd66b6bca45')">
            <PclInput v-model="localNotes" multiline :rows="9" :placeholder="t('instance.notesPlaceholder')" />
            <div class="pcl-actions">
              <PclButton variant="highlight" icon="save" :disabled="busy" @click="emit('save-notes', localNotes)">{{ t('instance.saveNotes') }}</PclButton>
              <PclButton variant="ghost" icon="rotate-ccw" :disabled="busy" @click="localNotes = notes">{{ t('instance.restore') }}</PclButton>
            </div>
          </PclCard>

          <InstanceLogViewer v-else-if="section === 'logs'" :instance="instance.id" :log-files="logFiles" :execute="execute" pcl />

          <PclInstanceCustomUi v-else-if="section === 'customui'" :instance="instance" :busy="busy" :execute="execute" />

          <PclCard v-else :title="current.label">
            <div class="pcl-empty">
              <PclIcon name="tool-case" :size="38" />
              <strong>{{ current.label }} {{ t('instance.noBackend') }}</strong>
              <span>{{ t('instance.apiNotice') }}</span>
              <PclButton icon="arrow-left" @click="section = 'overall'">{{ t('instance.backOverview') }}</PclButton>
            </div>
          </PclCard>
        </div>
    </main>
  </section>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
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
import PclInstanceLiveLog from './PclInstanceLiveLog.vue';
import InstanceLogViewer from '../../../components/InstanceLogViewer.vue';
import PclSetupPage from '../settings/PclSetupPage.vue';
import fallback from '../../../xmcl-port/assets/minecraft.webp';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ instance: any; details: any; icon?: string; components: any[]; settings?: any[]; componentError?: string; notes: string; busy: boolean; execute: Execute; consoleLines: string[]; consoleState: string; logFiles: any[]; fileLog: string }>();
const { t } = useI18n();
const emit = defineEmits<{
  launch: []; 'open-folder': []; settings: []; delete: []; 'change-icon': []; 'save-notes': [notes: string]; refresh: [];
  'connect-console': []; 'clear-console': []; 'read-log': [file: string]; 'component-enabled': [component: any, enabled: boolean];
  'component-version': [component: any, version: string]; action: [op: string]; browse: [uid: string]; export: [payload: any];
  'save-setting': [key: string, value: any]; 'install-start': [info: any]; 'install-progress': [task: any]; 'install-done': [info: any];
}>();

const navGroups = [
  {
    label: t('pcl.auto2.kd6c5da9c81'),
    items: [
      { id: 'overall', label: t('pcl.auto2.k5060421d15'), icon: 'layout-dashboard', description: t('pcl.auto2.k2daee9496a') },
      { id: 'live', label: t('pcl.auto2.k8ad868abbc'), icon: 'scroll-text', description: t('pcl.auto2.kc7b9dd7040') },
      { id: 'memory', label: t('pcl.auto2.kd014ab7703'), icon: 'gauge', description: t('pcl.auto2.k37a994a735') },
      { id: 'install', label: t('pcl.auto2.kc9c77517fe'), icon: 'download', description: t('pcl.auto2.kbf376a6e82') },
      { id: 'export', label: t('pcl.auto2.k188896795f'), icon: 'file-archive', description: t('pcl.auto2.kc54b4b046e') },
    ],
  },
  {
    label: t('pcl.auto2.k1836ba7e88'),
    items: [
      { id: 'launch', label: t('pcl.auto2.k6c4942326d'), icon: 'rocket', description: t('pcl.auto2.k6af73797d2') },
      { id: 'java', label: 'Java', icon: 'coffee', description: t('pcl.auto2.k518618f138') },
      { id: 'network', label: t('pcl.auto2.kb046fe277d'), icon: 'server', description: t('pcl.auto2.ke6f0d7118c') },
      { id: 'advanced', label: t('pcl.auto2.kc009d0ab82'), icon: 'tool-case', description: t('pcl.auto2.k32d383188b') },
      { id: 'advanced-settings', label: t('pcl.auto2.k482e1ba326'), icon: 'settings', description: t('pcl.auto2.kd73fcdcfc0') },
    ],
  },
  {
    label: t('pcl.auto2.k1694e79bf1'),
    items: [
      { id: 'worlds', label: t('pcl.auto2.k40e191bd7b'), icon: 'earth', description: t('pcl.auto2.ka14c52a0f6') },
      { id: 'screenshots', label: t('pcl.auto2.k49830b4f74'), icon: 'image', description: t('pcl.auto2.kbca4e3744a') },
      { id: 'mods', label: t('pcl.auto2.kda121cbcd9'), icon: 'puzzle', description: t('pcl.auto2.kf1eb3cbda7') },
      { id: 'coremods', label: t('pcl.auto2.kaf20a2dc69'), icon: 'package', description: t('pcl.auto2.kd246f27a0d') },
      { id: 'nilmods', label: 'NilMod', icon: 'package-plus', description: t('pcl.auto2.ka7df3b7943') },
      { id: 'resourcepacks', label: t('pcl.auto2.k826f79355b'), icon: 'palette', description: t('pcl.auto2.k66e5a38a6e') },
      { id: 'texturepacks', label: t('pcl.auto2.k9e4110cfdf'), icon: 'image', description: t('pcl.auto2.kc382ee2036') },
      { id: 'shaderpacks', label: t('pcl.auto2.k71adc562a4'), icon: 'sparkles', description: t('pcl.auto2.k46f5776573') },
      { id: 'datapacks', label: t('pcl.auto2.kf0e3fc625e'), icon: 'layers', description: t('pcl.auto2.kc84634681a') },
      { id: 'yesstevemodels', label: t('pcl.auto2.k021e5eedff'), icon: 'shirt', description: t('pcl.auto2.k4ed50d1251') },
      { id: 'customplayermodels', label: t('pcl.auto2.k59f7f631e1'), icon: 'user', description: t('pcl.auto2.kf8c74f6ae0') },
      { id: 'schematics', label: t('pcl.auto2.k07144e9760'), icon: 'grid-2x2', description: t('pcl.auto2.k08727b36d8') },
    ],
  },
  {
    label: t('pcl.auto2.kd319f3bf2e'),
    items: [
      { id: 'servers', label: t('pcl.auto2.k9164a5508e'), icon: 'network', description: t('pcl.auto2.k87f7a40e4f') },
      { id: 'notes', label: t('pcl.auto2.k697dbde096'), icon: 'scroll-text', description: t('pcl.auto2.ke7f355aca7') },
      { id: 'logs', label: t('pcl.auto2.k4de50894b8'), icon: 'scroll', description: t('pcl.auto2.k1379420d73') },
      { id: 'server', label: t('pcl.auto2.k6aca62f330'), icon: 'server', description: t('pcl.auto2.k327966cc1e') },
      { id: 'customui', label: t('pcl.auto2.kfdda79752b'), icon: 'layout-list', description: t('pcl.auto2.k5ae2dd0973') },
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
watch(() => props.notes, value => { localNotes.value = value; });

function select(id: string) {
  section.value = id;
}
</script>

<style scoped>
.pcl-instance-page {
  width: 100%;
  max-width: none;
  margin: 0;
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
  font-size: calc(13px * var(--pcl-font-scale, 1));
  font-weight: 700;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-instance-title small {
  display: block;
  margin-top: 3px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
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
  font-size: calc(10px * var(--pcl-font-scale, 1));
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
  font-size: calc(19px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-instance-toolbar p {
  margin: 3px 0 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

</style>
