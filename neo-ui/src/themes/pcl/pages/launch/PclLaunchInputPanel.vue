<template>
<div class="pcl-launch-left">
            <div v-show="launchState === 'launching' || launchState === 'launched'" class="pcl-launching-panel" :class="{ 'pcl-launching-panel--done': launchState === 'launched' }">
              <PclLoading />
              <strong class="pcl-launching-title">{{ launchTitle }}</strong>
              <span class="pcl-launching-name">{{ selected?.name || 'Minecraft' }}</span>
              <div class="pcl-progress-track"><span :style="{ width: `${progressPercent}%` }" /></div>
              <div class="pcl-launch-info" :class="{ 'pcl-launch-info--done': launchState === 'launched' }">
                <span><em>{{ t('launch.step') }}</em><b>{{ launchStage || t('launch.init') }}</b></span>
                <span><em>{{ t('launch.loginMethod') }}</em><b>{{ launchLoginMethod || loginMethodLabel }}</b></span>
                <span><em>{{ t('launch.progress') }}</em><b>{{ progressLabel }}</b></span>
                <span v-if="launchSpeed > 0"><em>{{ t('launch.speed') }}</em><b>{{ formatRate(launchSpeed) }}</b></span>
              </div>
              <Transition name="pcl-hint">
                <PclHint v-if="launchState === 'launched' && launchHint" theme="blue" class="pcl-launch-trivia">{{ launchHint }}</PclHint>
              </Transition>
              <PclButton v-if="launchState === 'launching'" class="pcl-cancel-button" icon="x" @click="emit('cancel')">{{ t('launch.cancel') }}</PclButton>
            </div>

            <div v-show="launchState !== 'launching' && launchState !== 'launched'" class="pcl-input-panel">
              <div class="pcl-account-area">
                <div class="pcl-login-slot">
                  <button class="pcl-login-slot__avatar" type="button" :title="t('pcl.launchPage.switchAccount')" @click="accountMenu = !accountMenu">
                    <PclAccountAvatar v-if="activeAccount?.profile?.skin?.url" :src="activeAccount.profile.skin.url" :name="activeAccount.profileName || activeAccount.displayName" />
                    <PclIcon v-else name="user" :size="26" />
                  </button>
                  <div class="pcl-login-slot__copy">
                    <strong>{{ activeAccount?.profileName || activeAccount?.displayName || t('nav.noAccount') }}</strong>
                    <small>{{ loginMethodLabel }}</small>
                  </div>
                  <button class="pcl-account-toggle" type="button" :aria-label="t('pcl.launchPage.switchAccount')" @click="accountMenu = !accountMenu">
                    <PclIcon name="chevron-left" :size="14" class="pcl-account-toggle__icon" />
                  </button>
                </div>

                <div v-if="accountMenu" class="pcl-account-menu">
                  <div v-if="accounts.length" class="pcl-account-menu__list">
                    <PclListItem
                      v-for="account in accounts"
                      :key="accountKey(account)"
                      :title="account.profileName || account.displayName || t('account.unnamed')"
                      :info="accountTypeLabel(account) + (account.default ? ` · ${t('account.default')}` : '')"
                      :selected="accountKey(account) === accountId"
                      @click="chooseAccount(account)"
                    >
                      <template #icon>
                        <span v-if="account.profile?.skin?.url" class="pcl-account-face--menu">
                          <PclAccountAvatar :src="account.profile.skin.url" :name="account.profileName || account.displayName" />
                        </span>
                        <PclIcon v-else :name="account.type === 'msa' || account.type === 'Microsoft' ? 'cloud' : 'user'" :size="18" />
                      </template>
                      <template #actions>
                        <PclIcon v-if="accountKey(account) === accountId" name="circle-check" :size="16" />
                      </template>
                    </PclListItem>
                  </div>
                  <div v-else class="pcl-account-menu__empty">{{ t('launch.noAccount') }}</div>
                  <div class="pcl-account-menu__actions">
                    <PclButton variant="ghost" icon="user-plus" @click="openAccountPage('add')">{{ t('launch.addAccount') }}</PclButton>
                    <PclButton variant="ghost" icon="settings" @click="openAccountPage('manage')">{{ t('launch.manageAccounts') }}</PclButton>
                  </div>
                </div>
              </div>

              <div class="pcl-launch-state">
                <div v-show="launchState === 'failed' || launchState === 'cancelled'" class="pcl-launch-error">
                  <PclHint theme="red" :closable="false">
                    <b>{{ launchState === 'cancelled' ? t('launch.cancelled') : t('launch.failed') }}</b>
                    <span>{{ launchError || t('launch.retryHint') }}</span>
                  </PclHint>
                  <PclButton icon="refresh-cw" @click="emit('launch')">{{ t('common.retry') }}</PclButton>
                </div>
                <PclButton
                  v-show="launchState !== 'failed' && launchState !== 'cancelled'"
                  class="pcl-launch-button"
                  variant="accent"
                  size="xl"
                  :disabled="launchButtonDisabled"
                  :data-launch-state="launchState"
                  @click="onLaunch"
                >
                  <span class="pcl-launch-button__copy">
                    <span class="pcl-launch-button__label"><PclIcon name="play" :size="18" />{{ launchButtonLabel }}</span>
                  </span>
                </PclButton>
              </div>

              <button class="pcl-instance-button" :disabled="launchState === 'loading'" @click="selectionOpen = true">
                <PclIcon name="layout-list" :size="18" />
                <span class="pcl-instance-button__text">
                  <small>{{ t('launch.currentInstance') }}</small>
                  <strong>{{ selected?.name || (instances.length ? t('launch.chooseInstance') : t('launch.noInstances')) }}</strong>
                </span>
                <PclIcon name="chevron-right" :size="16" class="pcl-instance-button__arrow" />
              </button>
              <div class="pcl-launch-links">
                <PclButton variant="ghost" icon="folder-open" :disabled="!selected" @click="emit('open-folder')">{{ t('launch.openFolder') }}</PclButton>
                <PclButton variant="ghost" icon="settings" :disabled="!selected" @click="emit('settings')">{{ t('launch.instanceSettings') }}</PclButton>
              </div>
            </div>
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
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclLoading from '../../components/PclLoading.vue';
const context = inject(PclLaunchContextKey);
if (!context) throw new Error('PclLaunchContext is missing');
const { props, emit, selected, busy, notice, error, instances, icons, selectionOpen, accountMenu, accounts, accountId, launchState, activeAccount, launchDialogOpen, hasAccount, loginMethodLabel, launchButtonLabel, launchButtonDisabled, progressPercent, progressLabel, launchTitle, launchStage, launchLoginMethod, launchSpeed, launchDetails, launchChildren, launchError, launchHint, homeCards, preferences, homeInstanceUsesBelow, homeInstanceStyle, homeInstanceGridStyle, homeInstanceMaxHeight, orderedInstances, homeInstanceGroups, draggingInstance, dragOverId, contextMenu, servers, serversLoading, serversError, instanceClickTimer, logElement, logLines, hintVisible, hintDialog, hintCode, accountKey, accountTypeLabel, chooseAccount, openAccountPage, selectInstance, onLaunch, confirmHint, formatRate, instanceSubtitle, homeCardOrder, startDrag, endDrag, moveInstanceToGroup, dropOnGroup, dropOnInstance, openContextMenu, closeContextMenu, contextActions, runContextMenu, loadServers, clickInstance, setInstanceLayout, quickLaunch, connectServer, scrollToLog, refreshHome, copyLog, loadHintVisibility, nextTick, showPclToast, pclMusic, togglePclMusic, t } = context;
</script>
