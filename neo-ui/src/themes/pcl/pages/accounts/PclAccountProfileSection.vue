<template>
<div class="pcl-account-section">
        <PclCard :title="t('account.account')">
          <template #actions>
            <PclButton size="sm" variant="highlight" icon="user-plus" :disabled="busy || refreshing || loginBusy" @click="openLogin(offlineAccountAllowed ? 'offline' : 'microsoft')">{{ t('account.add') }}</PclButton>
            <PclIconButton icon="refresh-cw" :label="t('account.refresh')" :size="24" :disabled="busy || refreshing || loginBusy" @click="refreshAll" />
          </template>
          <PclAccountPanel v-if="accounts.length" :accounts="accounts" :busy="busy || refreshing || loginBusy" :execute="execute" @changed="emit('changed')" />
          <div v-else class="pcl-empty pcl-account-empty">
            <PclIcon name="user" :size="40" />
            <strong>{{ t('account.empty') }}</strong>
            <span>{{ t('account.emptyHint') }}</span>
            <div class="pcl-actions">
              <PclButton v-if="offlineAccountAllowed" variant="highlight" icon="user-plus" :disabled="busy || refreshing || loginBusy" @click="openLogin('offline')">{{ t('account.addOffline') }}</PclButton>
              <PclButton icon="cloud" :disabled="busy || refreshing || loginBusy" @click="openLogin('microsoft')">{{ t('account.microsoftLogin') }}</PclButton>
            </div>
          </div>
        </PclCard>
      </div>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclAccountContextKey } from './PclAccountContext';
import PclAccountPanel from './PclAccountPanel.vue'; import PclCard from '../../components/PclCard.vue'; import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclHint from '../../components/PclHint.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclIconButton from '../../components/PclIconButton.vue'; import PclInput from '../../components/PclInput.vue'; import PclListItem from '../../components/PclListItem.vue'; import PclLoading from '../../components/PclLoading.vue'; import PclSelect from '../../components/PclSelect.vue';
const context = inject(PclAccountContextKey); if (!context) throw new Error('PclAccountContext is missing');
const { t, props, emit, accounts, busy, execute, embedded, items, section, presets, helper, runtimeInfo, loginShown, presetShown, editingPreset, loginType, presetTest, testingPreset, loginBusy, loginError, loginTaskId, loginDeviceUrl, loginDeviceCode, loginStatus, loginSteps, localSettings, localDataRoot, helperChecked, localExternal, localPort, localPublicUrl, localUsers, localUserShown, localUserForm, refreshing, microsoftModes, offlineAccountAllowed, loginTypes, offlineUuidModes, loginPreset, loginForm, loginPresetOptions, presetForm, presetInfo, openLogin, openPreset, removePreset, testPreset, refreshAll, checkAuthlib, installAuthlib, localBool, setLocalSetting, openLocalDir, loadLocalServiceStatus, applyLocalService, openLocalUser, removeLocalUser, createLocalUser, applyLoginPreset, toggleManagedLocal, cancelLogin, openDeviceUrl, copyDeviceCode, submitLogin, savePreset, openSection } = context;
</script>
