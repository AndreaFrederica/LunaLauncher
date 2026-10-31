<template>
<div class="pcl-account-section">
      <PclCard :title="t('account.services')">
        <template #actions>
          <PclButton size="sm" variant="highlight" icon="circle-plus" :disabled="busy || refreshing || loginBusy" @click="openPreset()">{{ t('account.addService') }}</PclButton>
        </template>
        <div class="pcl-account-service-list">
          <PclListItem
            v-for="preset in presets"
            :key="preset.name"
            :title="preset.name"
            :info="presetInfo(preset)"
          >
            <template #icon><PclIcon name="shield-check" :size="18" /></template>
            <template #actions>
              <PclButton v-if="preset.official" size="sm" variant="highlight" icon="account" :disabled="busy || refreshing || loginBusy" @click.stop="openLogin('microsoft')">{{ t('account.officialLogin') }}</PclButton>
              <PclButton size="sm" variant="ghost" icon="refresh-cw" :disabled="busy || refreshing || loginBusy || testingPreset === preset.name" @click.stop="testPreset(preset, 'ping')">{{ t('ui.ping') }}</PclButton>
              <PclButton size="sm" variant="ghost" icon="fact_check" :disabled="busy || refreshing || loginBusy || testingPreset === preset.name" @click.stop="testPreset(preset, 'function')">{{ t('account.functionTest') }}</PclButton>
              <PclButton v-if="!preset.builtin" size="sm" variant="ghost" :disabled="busy || refreshing || loginBusy" icon="pencil" @click.stop="openPreset(preset)">{{ t('account.edit') }}</PclButton>
              <PclIconButton v-if="!preset.builtin" icon="trash-2" :label="t('account.deleteService')" :size="24" danger :disabled="busy || refreshing || loginBusy" @click.stop="removePreset(preset)" />
            </template>
          </PclListItem>
          <div v-if="!presets.length" class="pcl-empty">{{ t('account.noServices') }}</div>
        </div>
        <PclHint v-if="presetTest" :theme="presetTest.passed ? 'blue' : 'yellow'" closable @close="presetTest = null">
          {{ presetTest.name }} · {{ presetTest.message }}
          <span v-if="presetTest.elapsedMs !== undefined"> · {{ Math.round(Number(presetTest.elapsedMs) || 0) }} ms</span>
          <span v-if="presetTest.statusCode"> · HTTP {{ presetTest.statusCode }}</span>
        </PclHint>
      </PclCard>

      <PclCard :title="t('account.externalService')">
        <div class="pcl-local-status">
          <span class="pcl-local-status__dot" :class="{ ok: helper?.installed }" />
          <strong>{{ helper ? (helper.installed ? t('account.authInstalled') : t('account.authMissing')) : helperChecked ? t('account.authUnknown') : t('account.authCheckHint') }}</strong>
          <span>{{ t('account.onDemand') }}</span>
        </div>
        <div class="pcl-form-row">
            <span class="pcl-form-row__label">{{ t('account.dataRoot') }}</span>
          <div class="pcl-form-row__control">
            <code class="pcl-local-path">{{ localDataRoot || '—' }}</code>
          </div>
          <PclButton size="sm" icon="folder-open" :disabled="!localDataRoot" @click="openLocalDir">{{ t('account.open') }}</PclButton>
        </div>
        <div class="pcl-local-network">
          <div class="pcl-local-network__head">
            <strong>{{ t('account.externalService') }}</strong>
            <PclButton size="sm" variant="highlight" icon="save" :disabled="busy || refreshing || loginBusy" @click="applyLocalService">{{ t('account.applyRestart') }}</PclButton>
          </div>
          <PclCheckbox v-model="localExternal" :label="t('account.allowExternal')" />
          <div class="pcl-local-network__grid">
            <label class="pcl-login-field"><span>{{ t('account.listenPort') }}</span><PclInput v-model="localPort" placeholder="25585" /></label>
            <label class="pcl-login-field"><span>{{ t('account.publicUrl') }}</span><PclInput v-model="localPublicUrl" placeholder="http://192.168.1.10:25585" /></label>
          </div>
          <PclHint v-if="localExternal && !localPublicUrl" theme="yellow">{{ t('account.publicUrlHint') }}</PclHint>
        </div>
        <div class="pcl-check-grid">
          <PclCheckbox :model-value="localBool('LocalServiceCache')" :label="t('account.cacheResources')" @update:model-value="setLocalSetting('LocalServiceCache', $event)" />
          <PclCheckbox :model-value="localBool('YggdrasilAccountCache')" :label="t('account.cacheAccounts')" @update:model-value="setLocalSetting('YggdrasilAccountCache', $event)" />
        </div>
        <div class="pcl-local-users">
          <div class="pcl-local-users__head">
            <strong>{{ t('account.localUsers') }}</strong>
            <div class="pcl-actions">
              <PclButton size="sm" icon="refresh-cw" :disabled="busy || refreshing || loginBusy" @click="loadLocalServiceStatus">{{ t('common.refresh') }}</PclButton>
              <PclButton size="sm" variant="highlight" icon="user-plus" :disabled="busy || refreshing || loginBusy" @click="openLocalUser">{{ t('account.addUser') }}</PclButton>
            </div>
          </div>
          <div v-if="localUsers.length" class="pcl-local-users__list">
            <div v-for="user in localUsers" :key="user.id" class="pcl-local-users__row">
              <PclIcon name="user" :size="16" />
              <span>{{ user.username }}</span>
              <small>{{ user.enabled ? t('account.enabled') : t('account.disabled') }}</small>
              <PclIconButton icon="trash-2" :label="t('account.deleteService')" :size="22" danger :disabled="busy || refreshing || loginBusy" @click="removeLocalUser(user)" />
            </div>
          </div>
          <div v-else class="pcl-empty pcl-empty--compact">{{ t('account.noUsers') }}</div>
        </div>
        <PclHint theme="blue">
          {{ t('account.serviceHint') }}
        </PclHint>
        <div class="pcl-actions">
          <PclButton variant="highlight" icon="user-plus" :disabled="busy || refreshing || loginBusy" @click="openLogin('yggdrasil', true)">{{ t('account.addLocal') }}</PclButton>
          <PclButton icon="shield-check" :disabled="busy || refreshing || loginBusy" @click="checkAuthlib">{{ t('account.checkAuth') }}</PclButton>
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
