<template>
  <div class="xmcl-account-header">
    <div>
      <div class="eyebrow">{{ t('ui.accountTools') }}</div>
      <h4>{{ t('xmcl.account.toolsTitle') }}</h4>
      <p>{{ t('xmcl.account.toolsHint') }}</p>
    </div>
  </div>
  <div class="row q-gutter-sm">
    <q-btn color="primary" :label="t('xmcl.account.refreshAll')" :disable="busy" @click="refreshAll" />
    <q-btn outline :label="t('xmcl.account.installAuth')" :disable="busy" @click="installAuthlib" />
    <q-btn flat :label="t('xmcl.account.checkAuth')" :disable="busy" @click="checkAuthlib" />
  </div>
  <div v-if="helper" class="text-caption text-grey-5 q-mt-sm">
    {{ t('xmcl.account.authComponent') }}：{{ helper.installed ? t('xmcl.account.installed', { version: helper.version || '' }) : t('xmcl.account.notInstalled') }}<span v-if="helper.path"> · {{ helper.path }}</span>
  </div>
  <q-list bordered separator class="q-mt-lg">
    <q-item v-for="account in props.accounts" :key="account.internalId || account.id">
      <q-item-section>{{ account.profileName || account.displayName }}</q-item-section>
      <q-item-section side>{{ account.stateName || t('xmcl.account.unchecked') }}</q-item-section>
    </q-item>
  </q-list>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { XmclAccountContextKey } from './XmclAccountContext';

const context = inject(XmclAccountContextKey);
if (!context) throw new Error('XmclAccountContext is missing');
const { t, props, busy, helper, refreshAll, installAuthlib, checkAuthlib } = context;
</script>

