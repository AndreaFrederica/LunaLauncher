<template>
  <div class="pcl-route-shell" :class="routeAnimation">
    <PclLaunchPage v-show="page === 'instances'" v-bind="launchProps" @select="emit('select', $event)" @quick-launch="(instance, switchSelected) => emit('quick-launch', instance, switchSelected)" @connect-server="emit('connect-server', $event)" @launch-instance="emit('launch-instance', $event)" @edit-instance="emit('edit-instance', $event)" @open-instance-folder="emit('open-instance-folder', $event)" @instance-action="(instance, action) => emit('instance-action', instance, action)" @launch="emit('launch')" @cancel="emit('cancel')" @download="emit('download')" @account-change="emit('account-change', $event)" @account-page="emit('account-page')" @open-folder="emit('open-folder')" @settings="emit('settings')" @refresh="emit('refresh')" @tasks="emit('tasks')" @clear-log="emit('clear-log')" @select-console-instance="emit('select-console-instance', $event)" />
    <PclDownloadPage v-show="page === 'pcl-download'" :active="page === 'pcl-download'" :selected="selected" :busy="busy" :execute="execute" :instances="instances" :initial-component-uid="downloadComponentUid" @refresh="emit('refresh')" @launch="emit('download-launch')" @installed="emit('installed', $event)" @install-start="emit('install-start', $event)" @install-progress="emit('install-progress', $event)" @install-done="emit('install-done', $event)" />
    <PclToolsPage v-show="page === 'pcl-tools'" :active="page === 'pcl-tools'" :instances="instances" :selected="selected" :busy="busy" :execute="execute" @refresh="emit('refresh')" @open-instance="emit('open-instance', $event)" />
    <PclSetupPage v-show="page === 'pcl-settings'" :active="page === 'pcl-settings'" :settings="settings" :busy="busy" :scope="scope" :execute="execute" :accounts="accounts" @changed="emit('changed')" />
    <PclAccountPage v-show="page === 'accounts'" :accounts="accounts" :busy="busy" :execute="execute" @changed="emit('changed')" @cancel-login="emit('cancel-login')" />
    <PclInstancePage v-if="selected" v-show="page === 'manage'" :instance="selected" :details="details" :icon="icon" :components="components" :settings="settings" :component-error="componentError" :notes="notes" :busy="busy" :execute="execute" :console-lines="consoleLines" :console-state="consoleState" :log-files="logFiles" :file-log="fileLog" @launch="emit('manage-launch')" @open-folder="emit('manage-open-folder')" @settings="emit('manage-settings')" @delete="emit('delete')" @change-icon="emit('change-icon', $event)" @save-notes="emit('save-notes', $event)" @refresh="emit('refresh')" @connect-console="emit('connect-console')" @clear-console="emit('clear-console')" @read-log="emit('read-log', $event)" @component-enabled="(component, enabled) => emit('component-enabled', component, enabled)" @component-version="(component, version) => emit('component-version', component, version)" @browse="emit('browse', $event)" @export="emit('export', $event)" @save-setting="(key, value) => emit('save-setting', key, value)" @install-start="emit('install-start', $event)" @install-progress="emit('install-progress', $event)" @install-done="emit('install-done', $event)" @action="emit('action', $event)" />
  </div>
</template>

<script setup lang="ts">
import { computed, toRefs } from 'vue';
import PclDownloadPage from '../themes/pcl/pages/download/PclDownloadPage.vue';
import PclToolsPage from '../themes/pcl/pages/tools/PclToolsPage.vue';
import PclSetupPage from '../themes/pcl/pages/settings/PclSetupPage.vue';
import PclAccountPage from '../themes/pcl/pages/accounts/PclAccountPage.vue';
import PclInstancePage from '../themes/pcl/pages/instance/PclInstancePage.vue';
import PclLaunchPage from '../themes/pcl/pages/launch/PclLaunchPage.vue';

/**
 * Keep the host's public surface explicit.  `Record<string, any>` used to
 * work with the type checker, but Vue's SFC compiler cannot turn that
 * unresolved mapped type into runtime props during the production build.
 * The host deliberately forwards a broad set of catalog values, so each
 * value remains loosely typed while the prop names stay statically visible.
 */
interface PclPageHostProps {
  page?: any;
  routeAnimation?: any;
  instances?: any;
  icons?: any;
  selected?: any;
  busy?: any;
  notice?: any;
  error?: any;
  accounts?: any;
  accountId?: any;
  launchState?: any;
  launchProgress?: any;
  launchStage?: any;
  launchLoginMethod?: any;
  launchSpeed?: any;
  launchDetails?: any;
  launchChildren?: any;
  launchError?: any;
  logLines?: any;
  consoleMonitorId?: any;
  consoleMonitorOptions?: any;
  execute?: any;
  settings?: any;
  scope?: any;
  details?: any;
  icon?: any;
  components?: any;
  componentError?: any;
  notes?: any;
  consoleLines?: any;
  consoleState?: any;
  logFiles?: any;
  fileLog?: any;
  downloadComponentUid?: any;
}

const props = defineProps<PclPageHostProps>();
const emit = defineEmits([
  'select', 'quick-launch', 'connect-server', 'launch-instance', 'edit-instance', 'open-instance-folder', 'instance-action', 'launch', 'cancel', 'download', 'account-change', 'account-page', 'open-folder', 'settings', 'refresh', 'tasks', 'clear-log', 'select-console-instance', 'download-launch', 'installed', 'install-start', 'install-progress', 'install-done', 'open-instance', 'changed', 'cancel-login', 'manage-launch', 'manage-open-folder', 'manage-settings', 'delete', 'change-icon', 'save-notes', 'connect-console', 'clear-console', 'read-log', 'component-enabled', 'component-version', 'browse', 'export', 'save-setting', 'action',
]);
const { page, routeAnimation, selected, busy, execute, instances, accounts, downloadComponentUid, settings, scope, details, icon, components, componentError, notes, consoleLines, consoleState, logFiles, fileLog } = toRefs(props);
const launchProps = computed(() => ({
  instances: props.instances, icons: props.icons, selected: props.selected, icon: props.selected ? props.icons?.[props.selected.id] : undefined,
  busy: props.busy, notice: props.notice, error: props.error, accounts: props.accounts, accountId: props.accountId,
  launchState: props.launchState, launchProgress: props.launchProgress, launchStage: props.launchStage, launchLoginMethod: props.launchLoginMethod,
  launchSpeed: props.launchSpeed, launchDetails: props.launchDetails, launchChildren: props.launchChildren, launchError: props.launchError,
  logLines: props.logLines, consoleMonitorId: props.consoleMonitorId, consoleMonitorOptions: props.consoleMonitorOptions, execute: props.execute,
}));
</script>
