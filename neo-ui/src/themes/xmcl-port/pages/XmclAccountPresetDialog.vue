<template>
  <q-dialog v-model="presetShown">
    <q-card class="login-card text-white">
      <q-card-section>
        <div class="eyebrow">{{ t('ui.authority') }}</div>
        <div class="text-h6">{{ presetEditing ? t('xmcl.account.editService') : t('xmcl.account.addService') }}</div>
      </q-card-section>
      <q-card-section>
        <q-input v-model="presetForm.name" dark outlined :label="t('xmcl.account.serviceName')" class="q-mb-md" />
        <q-input v-model="presetForm.authUrl" dark outlined :label="t('xmcl.account.authUrl')" class="q-mb-md" />
        <q-input v-model="presetForm.sessionUrl" dark outlined :label="t('xmcl.account.sessionUrl')" class="q-mb-md" />
        <q-select v-model="presetForm.tokenType" dark outlined :options="['Standard', 'OAuth']" :label="t('xmcl.account.tokenType')" />
      </q-card-section>
      <q-card-actions align="right">
        <q-btn flat :label="t('common.cancel')" @click="presetShown = false" />
        <q-btn color="primary" :label="t('common.save')" :disable="!presetForm.name || !presetForm.authUrl || !presetForm.sessionUrl" @click="savePreset" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { XmclAccountContextKey } from './XmclAccountContext';

const context = inject(XmclAccountContextKey);
if (!context) throw new Error('XmclAccountContext is missing');
const { t, presetShown, presetEditing, presetForm, savePreset } = context;
</script>

