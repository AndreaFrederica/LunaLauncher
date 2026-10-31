<template>
  <XmclDashboard v-show="page === 'instances'" v-on="$attrs" :instances="instances" :icons="icons" :busy="busy" />
  <q-banner v-show="launchQueue?.length" class="bg-grey-9 q-mb-md">
    {{ t('app.queueWaiting', { names: launchQueue.map((item: any) => item.name).join('、') }) }}
    <template #action><q-btn flat :label="t('app.clearQueue')" @click="emit('clear-queue')" /></template>
  </q-banner>
  <XmclInstanceWorkspace v-if="selected && selected.type !== 'server' && selected.managedType !== 'server'" v-show="page === 'manage'" v-on="$attrs" :instance="selected" :details="details" :icon="icon" :components="components" :component-error="componentError" :notes="notes" :busy="busy" :execute="execute" :console-lines="consoleLines" :console-state="consoleState" :log-files="logFiles" :file-log="fileLog" />
  <NeoCreateInstancePage v-show="page === 'create'" v-on="$attrs" v-model="creation" v-model:mode="createMode" :busy="busy" :execute="execute" />
  <XmclAccountWorkspace v-show="page === 'accounts'" v-on="$attrs" :accounts="accounts" :busy="busy" :execute="execute" :pcl="false" />
  <XmclStoreWorkspace v-show="page === 'store'" v-on="$attrs" :busy="busy" :execute="execute" />
  <XmclMultiplayerWorkspace v-show="page === 'multiplayer'" v-on="$attrs" :instance="selected" :busy="busy" :execute="execute" />
  <XmclSettingsShell v-show="page === 'settings'" v-on="$attrs" :settings="settings" :busy="busy" :scope="scope" :execute="execute" />
  <ServerPanel v-if="selected && (selected.type === 'server' || selected.managedType === 'server')" v-show="page === 'manage'" v-on="$attrs" :instance="selected.id" :busy="busy" :execute="execute" :console-lines="consoleLines" :console-state="consoleState" />
</template>

<script setup lang="ts">
import { toRefs } from 'vue';
import { useI18n } from 'vue-i18n';
import NeoCreateInstancePage from './NeoCreateInstancePage.vue';
import ServerPanel from './ServerPanel.vue';
import XmclDashboard from '../themes/xmcl-port/pages/XmclDashboard.vue';
import XmclInstanceWorkspace from '../themes/xmcl-port/pages/XmclInstanceWorkspace.vue';
import XmclAccountWorkspace from '../themes/xmcl-port/pages/XmclAccountWorkspace.vue';
import XmclStoreWorkspace from '../themes/xmcl-port/pages/XmclStoreWorkspace.vue';
import XmclMultiplayerWorkspace from '../themes/xmcl-port/pages/XmclMultiplayerWorkspace.vue';
import XmclSettingsShell from '../themes/xmcl-port/pages/XmclSettingsShell.vue';

defineOptions({ inheritAttrs: false });
/** See NeoPclPageHost for why this is an explicit, loose prop contract. */
interface XmclPageHostProps {
  page?: any;
  instances?: any;
  icons?: any;
  selected?: any;
  busy?: any;
  execute?: any;
  launchQueue?: any;
  details?: any;
  icon?: any;
  components?: any;
  componentError?: any;
  notes?: any;
  consoleLines?: any;
  consoleState?: any;
  logFiles?: any;
  fileLog?: any;
  creation?: any;
  createMode?: any;
  accounts?: any;
  settings?: any;
  scope?: any;
}

const props = defineProps<XmclPageHostProps>();
const emit = defineEmits(['clear-queue']);
const { page, instances, icons, selected, busy, execute, launchQueue, details, icon, components, componentError, notes, consoleLines, consoleState, logFiles, fileLog, creation, createMode, accounts, settings, scope } = toRefs(props);
const { t } = useI18n();
</script>
