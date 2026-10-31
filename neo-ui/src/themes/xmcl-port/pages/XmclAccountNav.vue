<template>
  <aside class="xmcl-account-nav">
    <div class="xmcl-account-avatar">
      <span v-if="activeAccount?.profile?.skin?.url" class="account-face" :style="skinFaceStyle(activeAccount.profile.skin.url)" aria-hidden="true"></span>
      <q-icon v-else name="person" size="38px" />
    </div>
    <strong>{{ t('xmcl.account.center') }}</strong>
    <button v-for="item in sections" :key="item.id" :class="{ active: section === item.id }" @click="section = item.id">
      <q-icon :name="item.icon" />{{ item.label }}
    </button>
  </aside>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { XmclAccountContextKey } from './XmclAccountContext';

const context = inject(XmclAccountContextKey);
if (!context) throw new Error('XmclAccountContext is missing');
const { t, activeAccount, skinFaceStyle, sections, section } = context;
</script>

