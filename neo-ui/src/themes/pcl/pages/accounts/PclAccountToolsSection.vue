<template>
<div class="pcl-account-section">
      <PclCard :title="t('account.tools')">
        <div class="pcl-actions pcl-account-actions">
          <PclButton variant="highlight" icon="refresh-cw" :disabled="busy || refreshing || loginBusy" @click="refreshAll">{{ t('account.refreshAll') }}</PclButton>
          <PclButton icon="shield-check" :disabled="busy || refreshing || loginBusy" @click="checkAuthlib">{{ t('account.checkAuth') }}</PclButton>
          <PclButton icon="download" :disabled="busy || refreshing || loginBusy" @click="installAuthlib">{{ t('account.installAuth') }}</PclButton>
        </div>
        <p class="pcl-muted">{{ helper ? `Authentication component: ${helper.installed ? `Installed ${helper.version || ''}` : 'Not installed'}` : t('account.readingAuth') }}</p>
      </PclCard>
      </div>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclAccountContextKey } from './PclAccountContext';
import PclAccountPanel from './PclAccountPanel.vue'; import PclCard from '../../components/PclCard.vue'; import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclHint from '../../components/PclHint.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclIconButton from '../../components/PclIconButton.vue'; import PclInput from '../../components/PclInput.vue'; import PclListItem from '../../components/PclListItem.vue'; import PclLoading from '../../components/PclLoading.vue'; import PclSelect from '../../components/PclSelect.vue';
const context = inject(PclAccountContextKey); if (!context) throw new Error('PclAccountContext is missing');
const { t, props, emit, accounts, busy, execute, embedded, items, section, presets, helper, runtimeInfo, loginShown, presetShown, editingPreset, loginType, presetTest, testingPreset, loginBusy, loginError, loginTaskId, loginDeviceUrl, loginDeviceCode, loginStatus, loginSteps, localSettings, localDataRoot, helperChecked, localExternal, localPort, localPublicUrl, localUsers, localUserShown, localUserForm, refreshing, microsoftModes, offlineAccountAllowed, loginTypes, offlineUuidModes, loginPreset, loginForm, loginPresetOptions, presetForm, presetInfo, openLogin, openPreset, removePreset, testPreset, refreshAll, checkAuthlib, installAuthlib, localBool, setLocalSetting, openLocalDir, loadLocalServiceStatus, applyLocalService, openLocalUser, removeLocalUser, createLocalUser, applyLoginPreset, toggleManagedLocal, cancelLogin, openDeviceUrl, copyDeviceCode, submitLogin, savePreset, openSection } = context;
</script>
