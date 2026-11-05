<template>
<div class="pcl-launch-right">
          <PclCard v-if="hintVisible" :title="t('pcl.launchPage.neoHint')" :style="{ order: homeCardOrder('hint') }">
            <template #actions>
              <PclIconButton icon="x" :label="t('pcl.common.closeHint')" :size="24" @click="hintDialog = true" />
            </template>
            <p class="pcl-launch-card-copy">{{ t('launch.thanks') }}</p>
            <p class="pcl-launch-card-copy pcl-muted">{{ t('launch.feedback') }}</p>
          </PclCard>

          <PclCard v-for="card in homeCards" :key="card.id" :title="card.title" class="pcl-home-card" :style="{ order: homeCardOrder(`custom:${card.id}`) }">
            <p class="pcl-home-card__content">{{ card.content }}</p>
          </PclCard>

          <PclCard :title="t('launch.log')" :style="{ order: homeCardOrder('log') }">
            <template #actions>
              <label v-if="consoleMonitorOptions?.length" class="pcl-launch-log__monitor">
                <span>{{ t('launch.monitorInstance') }}</span>
                <select :value="consoleMonitorId || ''" @change="emit('select-console-instance', ($event.target as HTMLSelectElement).value)">
                  <option v-for="item in consoleMonitorOptions" :key="item.id" :value="item.id">{{ item.name }}{{ item.running ? ` · ${t('launch.runningStatus')}` : '' }}</option>
                </select>
              </label>
              <PclCheckbox v-model="preferences.pclLaunchLogAutoScroll" :label="t('launch.autoScroll')" />
              <PclIconButton icon="copy" :label="t('common.copyLog')" :size="24" :disabled="!logLines.length" @click="copyLog" />
              <PclIconButton icon="trash-2" :label="t('common.clearLog')" :size="24" :disabled="!logLines.length" @click="emit('clear-log')" />
              <PclIconButton icon="refresh-cw" :label="t('common.refresh')" :size="24" :disabled="busy" @click="emit('refresh')" />
            </template>
            <div ref="logElement" class="pcl-launch-log" aria-live="polite">
              <div v-if="logLines.length" v-for="(line, index) in logLines" :key="index">{{ line }}</div>
              <span v-else>{{ notice || (selected ? t('launch.selected', { name: selected.name }) : t('launch.notSelected')) }}</span>
            </div>
          </PclCard>

          <PclCard v-if="preferences.pclHomeShowInstances" :title="t('pcl.launchPage.allInstances')" class="pcl-home-instances" :style="[homeInstanceStyle, { order: homeCardOrder('instances') }]">
            <template #actions>
              <PclIconButton icon="grid-2x2" :label="t('pcl.launchPage.grid')" :size="24" :class="{ active: preferences.pclHomeInstanceLayout === 'grid' }" @click="setInstanceLayout('grid')" />
              <PclIconButton icon="list" :label="t('pcl.launchPage.list')" :size="24" :class="{ active: preferences.pclHomeInstanceLayout === 'list' }" @click="setInstanceLayout('list')" />
              <PclIconButton icon="refresh-cw" :label="t('common.refresh')" :size="24" :disabled="busy" @click="emit('refresh')" />
            </template>
            <div v-if="instances.length" class="pcl-home-instance-groups" :style="{ maxHeight: homeInstanceMaxHeight }">
              <div v-for="group in homeInstanceGroups" :key="group.id" class="pcl-home-instance-group">
                <button
                  v-if="preferences.pclHomeInstanceGrouping === 'grouped'"
                  class="pcl-home-instance-group__head"
                  type="button"
                  :aria-expanded="!isHomeInstanceGroupCollapsed(group.id)"
                  @dragover.prevent
                  @drop.prevent="dropOnGroup(group.id)"
                  @click="toggleHomeInstanceGroup(group.id)"
                >
                  <PclIcon :name="isHomeInstanceGroupCollapsed(group.id) ? 'chevron-right' : 'chevron-down'" :size="13" />
                  <strong>{{ group.label }}</strong>
                  <small>{{ group.items.length }}</small>
                </button>
                <div
                  v-if="!isHomeInstanceGroupCollapsed(group.id)"
                  class="pcl-home-instance-list"
                  :class="`is-${preferences.pclHomeInstanceLayout}`"
                  :style="preferences.pclHomeInstanceLayout === 'grid' ? homeInstanceGridStyle : undefined"
                >
                  <button
                    v-for="item in group.items"
                    :key="item.id"
                    type="button"
                    class="pcl-home-instance"
                    :class="{ active: item.id === selected?.id, 'is-drag-over': dragOverId === item.id, 'text-below': homeInstanceUsesBelow }"
                    :draggable="true"
                    :title="t('pcl.launchPage.doubleLaunchNamed', { name: item.name })"
                    @click="clickInstance(item)"
                    @dblclick="quickLaunch(item)"
                    @contextmenu.prevent.stop="openContextMenu(item, $event)"
                    @dragstart="startDrag(item, $event)"
                    @dragend="endDrag"
                    @dragover.prevent="dragOverId = item.id"
                    @dragleave="dragOverId = ''"
                    @drop.prevent="dropOnInstance(item, $event)"
                  >
                    <span class="pcl-home-instance__icon-wrap"><img v-if="icons[item.id]" :src="icons[item.id]" alt="" /><span v-else class="pcl-home-instance__fallback"><PclIcon name="box" :size="20" /></span><InstanceFormatBadge :instance="item" /></span>
                    <span class="pcl-home-instance__copy">
                      <strong>{{ item.name }}</strong>
                      <small v-if="instanceSubtitle(item)">{{ instanceSubtitle(item) }}</small>
                    </span>
                    <span v-if="item.running" class="pcl-home-instance__running">{{ t('launch.runningStatus') }}</span>
                    <span v-else class="pcl-home-instance__hint">{{ preferences.pclHomeInstanceSort === 'manual' ? t('launch.dragLaunch') : t('launch.doubleLaunch') }}</span>
                  </button>
                </div>
              </div>
            </div>
            <div v-else class="pcl-empty pcl-empty--compact">{{ t('launch.noInstancesYet') }}</div>
          </PclCard>

          <PclCard v-if="preferences.pclHomeShowServers" :title="t('pcl.sidebar.currentServers')" class="pcl-home-servers" :style="{ order: homeCardOrder('servers') }">
            <template #actions>
              <PclIconButton icon="refresh-cw" :label="t('server.refresh')" :size="24" :disabled="busy || serversLoading" @click="loadServers" />
            </template>
            <div v-if="!selected" class="pcl-empty pcl-empty--compact">{{ t('launch.selectInstance') }}</div>
            <div v-else-if="serversLoading" class="pcl-home-server-loading"><PclLoading inline /> {{ t('launch.loadingServers') }}</div>
            <div v-else-if="serversError" class="pcl-home-server-error">{{ serversError }}</div>
            <div v-else-if="servers.length" class="pcl-home-server-list">
              <button v-for="server in servers" :key="server.ip" type="button" class="pcl-home-server" @click="connectServer(server)">
                <PclIcon name="server" :size="17" />
                <span class="pcl-home-server__copy"><strong>{{ server.name || server.ip }}</strong><small>{{ server.ip }}</small></span>
                <PclIcon name="play" :size="14" class="pcl-home-server__play" />
              </button>
            </div>
            <div v-else class="pcl-empty pcl-empty--compact">{{ t('launch.noServers') }}</div>
            <p class="pcl-home-servers__hint">{{ t('launch.serverHint') }}</p>
          </PclCard>
        </div>
</template>
<script setup lang="ts">
import { inject } from 'vue';
import { PclLaunchContextKey } from './PclLaunchContext';
import PclAccountAvatar from '../../components/PclAccountAvatar.vue';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclExtraButton from '../../components/PclExtraButton.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import InstanceFormatBadge from '../../../../components/InstanceFormatBadge.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';
const context = inject(PclLaunchContextKey);
if (!context) throw new Error('PclLaunchContext is missing');
const { props, emit, selected, busy, notice, error, instances, icons, selectionOpen, accountMenu, accounts, accountId, launchState, activeAccount, launchDialogOpen, hasAccount, loginMethodLabel, launchButtonLabel, launchButtonDisabled, progressPercent, progressLabel, launchTitle, launchStage, launchLoginMethod, launchSpeed, launchDetails, launchChildren, launchError, launchHint, homeCards, preferences, homeInstanceUsesBelow, homeInstanceStyle, homeInstanceGridStyle, homeInstanceMaxHeight, orderedInstances, homeInstanceGroups, isHomeInstanceGroupCollapsed, toggleHomeInstanceGroup, draggingInstance, dragOverId, contextMenu, servers, serversLoading, serversError, instanceClickTimer, logElement, logLines, hintVisible, hintDialog, hintCode, accountKey, accountTypeLabel, chooseAccount, openAccountPage, selectInstance, onLaunch, confirmHint, formatRate, instanceSubtitle, homeCardOrder, startDrag, endDrag, moveInstanceToGroup, dropOnGroup, dropOnInstance, openContextMenu, closeContextMenu, contextActions, runContextMenu, loadServers, clickInstance, setInstanceLayout, quickLaunch, connectServer, scrollToLog, refreshHome, copyLog, loadHintVisibility, nextTick, showPclToast, pclMusic, togglePclMusic, t, consoleMonitorId, consoleMonitorOptions } = context;
</script>
<style scoped>
.pcl-home-instance__icon-wrap { position: relative; display: inline-flex; width: 38px; height: 38px; flex: 0 0 38px; align-items: center; justify-content: center; }
.pcl-home-instance__icon-wrap > img, .pcl-home-instance__icon-wrap > .pcl-home-instance__fallback { width: 38px; height: 38px; }
.pcl-home-instance__icon-wrap :deep(.instance-format-badge) { right: -4px; bottom: -4px; transform: scale(.78); transform-origin: bottom right; }
</style>
