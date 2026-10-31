<template>
<q-dialog v-model="loginShown">
      <q-card class="pcl-dialog pcl-login-dialog">
        <q-card-section>
          <div class="pcl-kicker">{{ t('account.login') }}</div>
          <h3>{{ t('account.add') }}</h3>
        </q-card-section>
        <q-card-section class="pcl-login-body">
          <label class="pcl-login-field">
            <span>{{ t('account.loginMethod') }}</span>
            <PclSelect v-model="loginType" :options="loginTypes" />
          </label>
          <label class="pcl-login-field">
            <span>{{ loginType === 'microsoft' ? t('account.playerNameOptional') : t('account.playerName') }}</span>
            <PclInput v-model="loginForm.username" :placeholder="loginType === 'microsoft' ? t('account.optional') : t('account.enterPlayerName')" />
          </label>
          <template v-if="loginType === 'offline'">
            <label class="pcl-login-field">
              <span>{{ t('account.offlineUuidMode') }}</span>
              <PclSelect v-model="loginForm.offlineUuidMode" :options="offlineUuidModes" />
            </label>
            <label v-if="loginForm.offlineUuidMode === 'custom'" class="pcl-login-field">
              <span>{{ t('account.customUuid') }}</span>
              <PclInput v-model="loginForm.offlineUuid" :placeholder="t('account.uuidPlaceholder')" />
            </label>
          </template>
          <label v-if="loginType === 'yggdrasil' || loginType === 'unified-pass'" class="pcl-login-field">
            <span>{{ t('account.password') }}</span>
            <PclInput v-model="loginForm.password" type="password" :placeholder="t('account.password')" />
          </label>
          <template v-if="loginType === 'yggdrasil'">
            <label class="pcl-login-field">
              <span>{{ t('account.authServer') }}</span>
              <PclSelect v-model="loginPreset" :options="loginPresetOptions" @update:model-value="applyLoginPreset" />
            </label>
            <PclCheckbox v-model="loginForm.managedLocal" :label="t('account.managedLocal')" @update:model-value="toggleManagedLocal" />
            <label v-if="!loginForm.managedLocal" class="pcl-login-field">
              <span>{{ t('account.authUrl') }}</span>
              <PclInput v-model="loginForm.authUrl" placeholder="https://..." />
            </label>
            <label v-if="!loginForm.managedLocal" class="pcl-login-field">
              <span>{{ t('account.sessionUrl') }}</span>
              <PclInput v-model="loginForm.sessionUrl" placeholder="https://..." />
            </label>
            <label class="pcl-login-field">
              <span>{{ t('account.serviceNameOptional') }}</span>
              <PclInput v-model="loginForm.sourceName" :placeholder="t('account.displayName')" />
            </label>
          </template>
          <template v-if="loginType === 'unified-pass'">
            <label class="pcl-login-field"><span>{{ t('ui.serverId') }}</span><PclInput v-model="loginForm.serverId" :placeholder="t('ui.serverId')" /></label>
            <label class="pcl-login-field"><span>{{ t('account.apiBase') }}</span><PclInput v-model="loginForm.baseUrl" placeholder="https://..." /></label>
          </template>
          <template v-if="loginType === 'microsoft'">
            <label class="pcl-login-field">
              <span>{{ t('account.microsoftMode') }}</span>
              <PclSelect v-model="loginForm.microsoftMode" :options="microsoftModes" />
            </label>
            <PclHint theme="blue">
              {{ loginForm.microsoftMode === 'oauth' ? 'The browser will open and return locally; no code is required.' : 'A device code will be shown for entry in the browser.' }}
            </PclHint>
            <label class="pcl-login-field">
              <span>{{ t('account.profileName') }}</span>
              <PclInput v-model="loginForm.minecraftProfileName" :placeholder="t('account.optional')" />
            </label>
          </template>
        </q-card-section>
        <div v-if="loginBusy" class="pcl-login-progress">
          <div class="pcl-login-progress__head"><PclLoading inline /><span>{{ loginStatus || t('account.checkingStatus') }}</span></div>
          <div v-for="step in loginSteps.slice(0, 8)" :key="step.id || step.label" class="pcl-login-progress__step">
            <span class="pcl-login-progress__dot" :class="step.state" />
            <span>{{ step.label || step.status || t('account.checking') }}</span>
            <small>{{ step.status || step.details || '' }}</small>
          </div>
        </div>
        <div v-if="loginDeviceUrl" class="pcl-login-device">
          <div class="pcl-login-device__label">{{ loginDeviceCode ? t('account.browserCode') : t('account.browserOpened') }}</div>
          <code v-if="loginDeviceCode" class="pcl-login-device__code">{{ loginDeviceCode }}</code>
          <code class="pcl-login-device__url">{{ loginDeviceUrl }}</code>
          <div class="pcl-actions">
            <PclButton size="sm" icon="external-link" @click="openDeviceUrl">{{ t('account.openBrowser') }}</PclButton>
            <PclButton v-if="loginDeviceCode" size="sm" icon="copy" @click="copyDeviceCode">{{ t('account.copyCode') }}</PclButton>
          </div>
        </div>
        <PclHint v-if="loginError" theme="red" closable @close="loginError = ''">{{ loginError }}</PclHint>
        <q-card-actions align="right">
          <PclButton variant="ghost" @click="cancelLogin">{{ t('account.cancel') }}</PclButton>
          <PclButton variant="highlight" :disabled="busy || loginBusy || (!loginForm.username && loginType !== 'microsoft')" @click="submitLogin">
            {{ loginBusy ? t('account.waitLogin') : t('account.continue') }}
          </PclButton>
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
