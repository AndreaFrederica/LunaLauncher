<template>
  <q-dialog v-model="loginShown" persistent>
    <q-card class="login-card text-white">
      <q-card-section>
        <div class="eyebrow">{{ t('ui.accountLogin') }}</div>
        <div class="text-h6">{{ loginTitle }}</div>
        <p class="text-caption text-grey-5">{{ t('xmcl.account.loginHint') }}</p>
      </q-card-section>
      <q-card-section>
        <q-input v-if="loginType === 'yggdrasil' || loginType === 'unified-pass'" v-model="loginForm.username" dark outlined :label="t('xmcl.account.username')" autofocus class="q-mb-md" />
        <q-input v-if="loginType === 'offline'" v-model="loginForm.username" dark outlined :label="t('xmcl.account.offlinePlayer')" />
        <template v-if="loginType === 'yggdrasil'">
          <q-select v-model="loginForm.presetName" dark outlined :options="presets" option-label="name" option-value="name" emit-value map-options :label="t('xmcl.account.authService')" class="q-mb-md" @update:model-value="applyPreset" />
          <q-input v-model="loginForm.password" dark outlined type="password" :label="t('xmcl.account.password')" class="q-mb-md" />
          <q-input v-if="!loginForm.managedLocal" v-model="loginForm.authUrl" dark outlined :label="t('xmcl.account.authUrl')" class="q-mb-md" />
          <q-input v-if="!loginForm.managedLocal" v-model="loginForm.sessionUrl" dark outlined :label="t('xmcl.account.sessionUrl')" />
        </template>
        <template v-if="loginType === 'unified-pass'">
          <q-input v-model="loginForm.password" dark outlined type="password" :label="t('xmcl.account.password')" class="q-mb-md" />
          <q-input v-model="loginForm.serverId" dark outlined :label="t('xmcl.account.serverId')" class="q-mb-md" />
          <q-input v-model="loginForm.baseUrl" dark outlined :label="t('xmcl.account.apiBase')" />
        </template>
        <q-banner v-if="loginType === 'microsoft'" class="bg-blue-grey-9">{{ t('xmcl.account.microsoftHint') }}</q-banner>
      </q-card-section>
      <q-card-actions align="right">
        <q-btn flat :label="t('common.cancel')" @click="loginShown = false" />
        <q-btn color="primary" :label="t('xmcl.account.continue')" :loading="busy" @click="submitLogin" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { XmclAccountContextKey } from './XmclAccountContext';

const context = inject(XmclAccountContextKey);
if (!context) throw new Error('XmclAccountContext is missing');
const { t, busy, presets, loginShown, loginType, loginForm, loginTitle, applyPreset, submitLogin } = context;
</script>

