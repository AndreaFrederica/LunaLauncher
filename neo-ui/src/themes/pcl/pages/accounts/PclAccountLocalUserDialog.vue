<template>
<q-dialog v-model="localUserShown">
      <q-card class="pcl-dialog">
        <q-card-section>
          <div class="pcl-kicker">{{ t('account.localUser') }}</div>
          <h3>{{ t('account.addLocalUser') }}</h3>
        </q-card-section>
        <q-card-section class="pcl-login-body">
          <label class="pcl-login-field"><span>{{ t('account.username') }}</span><PclInput v-model="localUserForm.username" :placeholder="t('account.username')" /></label>
          <label class="pcl-login-field"><span>{{ t('account.password') }}</span><PclInput v-model="localUserForm.password" type="password" :placeholder="t('account.password')" /></label>
        </q-card-section>
        <q-card-actions align="right">
          <PclButton variant="ghost" @click="localUserShown = false">{{ t('account.cancel') }}</PclButton>
          <PclButton variant="highlight" :disabled="busy || !localUserForm.username || !localUserForm.password" @click="createLocalUser">{{ t('account.create') }}</PclButton>
        </q-card-actions>
      </q-card>
    </q-dialog>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclAccountContextKey } from './PclAccountContext';
import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclHint from '../../components/PclHint.vue'; import PclInput from '../../components/PclInput.vue'; import PclLoading from '../../components/PclLoading.vue'; import PclSelect from '../../components/PclSelect.vue';
const context = inject(PclAccountContextKey); if (!context) throw new Error('PclAccountContext is missing');
const { t, props, emit, accounts, busy, execute, embedded, items, section, presets, helper, runtimeInfo, loginShown, presetShown, editingPreset, loginType, presetTest, testingPreset, loginBusy, loginError, loginTaskId, loginDeviceUrl, loginDeviceCode, loginStatus, loginSteps, localSettings, localDataRoot, helperChecked, localExternal, localPort, localPublicUrl, localUsers, localUserShown, localUserForm, refreshing, microsoftModes, offlineAccountAllowed, loginTypes, offlineUuidModes, loginPreset, loginForm, loginPresetOptions, presetForm, presetInfo, openLogin, openPreset, removePreset, testPreset, refreshAll, checkAuthlib, installAuthlib, localBool, setLocalSetting, openLocalDir, loadLocalServiceStatus, applyLocalService, openLocalUser, removeLocalUser, createLocalUser, applyLoginPreset, toggleManagedLocal, cancelLogin, openDeviceUrl, copyDeviceCode, submitLogin, savePreset } = context;
</script>
