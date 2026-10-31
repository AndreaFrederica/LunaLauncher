<template>
  <div class="xmcl-account-header">
    <div>
      <div class="eyebrow">{{ t('ui.authority') }}</div>
      <h4>{{ t('xmcl.account.yggdrasil') }}</h4>
      <p>{{ t('xmcl.account.presetHint') }}</p>
    </div>
    <q-btn color="primary" :label="t('xmcl.account.addService')" @click="addPreset" />
  </div>
  <q-list bordered separator class="xmcl-preset-list">
    <q-item v-for="preset in presets" :key="preset.name">
      <q-item-section>
        <q-item-label>
          {{ preset.name }}
          <q-badge v-if="preset.builtin">{{ t('xmcl.account.builtin') }}</q-badge>
          <q-badge v-if="preset.managedLocal" color="positive">{{ t('xmcl.account.localService') }}</q-badge>
        </q-item-label>
        <q-item-label caption>{{ preset.managedLocal ? t('xmcl.account.managedLocal') : `${preset.authUrl} · ${preset.sessionUrl}` }}</q-item-label>
      </q-item-section>
      <q-item-section side>
        <q-btn v-if="!preset.builtin" flat :label="t('common.edit')" :disable="busy" @click="editPreset(preset)" />
        <q-btn v-if="!preset.builtin" flat color="negative" :label="t('common.delete')" :disable="busy" @click="removePreset(preset)" />
      </q-item-section>
    </q-item>
    <q-item v-if="!presets.length"><q-item-section class="text-grey-5">{{ t('xmcl.account.noPresets') }}</q-item-section></q-item>
  </q-list>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { XmclAccountContextKey } from './XmclAccountContext';

const context = inject(XmclAccountContextKey);
if (!context) throw new Error('XmclAccountContext is missing');
const { t, busy, presets, addPreset, editPreset, removePreset } = context;
</script>

