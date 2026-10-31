<template>
  <div class="xmcl-account-header">
    <div>
      <div class="eyebrow">{{ t('ui.myAccount') }}</div>
      <h4>{{ t('xmcl.account.profileTitle') }}</h4>
      <p>{{ t('xmcl.account.profileHint') }}</p>
    </div>
    <div class="row q-gutter-sm">
      <q-btn color="primary" :label="t('xmcl.account.addMicrosoft')" :disable="busy" @click="openLogin('microsoft')" />
      <q-btn v-if="offlineAccountAllowed" flat :label="t('xmcl.account.addOffline')" :disable="busy" @click="openLogin('offline')" />
      <q-btn flat :label="t('xmcl.account.addYggdrasil')" :disable="busy" @click="openLogin('yggdrasil')" />
      <q-btn flat :label="t('xmcl.account.addUnifiedPass')" :disable="busy" @click="openLogin('unified-pass')" />
    </div>
  </div>
  <AccountPanel :accounts="props.accounts" :busy="busy" :execute="execute" @changed="onChanged" />
</template>

<script setup lang="ts">
import { inject } from 'vue';
import AccountPanel from '../../../components/AccountPanel.vue';
import { XmclAccountContextKey } from './XmclAccountContext';

const context = inject(XmclAccountContextKey);
if (!context) throw new Error('XmclAccountContext is missing');
const { t, props, busy, execute, offlineAccountAllowed, openLogin, onChanged } = context;
</script>

