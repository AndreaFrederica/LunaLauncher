<template>
  <q-dialog v-model="versionShown">
    <q-card class="more-card text-white">
      <q-card-section class="text-h6">{{ t('xmcl.instance.changeComponentVersion', { name: editing?.name || editing?.id }) }}</q-card-section>
      <q-card-section><q-input v-model="version" dark outlined :label="t('xmcl.instance.componentVersion')" autofocus /></q-card-section>
      <q-card-actions align="right"><q-btn flat :label="t('common.cancel')" @click="versionShown = false" /><q-btn color="primary" :label="t('common.save')" :disable="!version.trim()" @click="saveVersion" /></q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { XmclInstanceContextKey } from './XmclInstanceContext';
const context = inject(XmclInstanceContextKey);
if (!context) throw new Error('XmclInstanceContext is missing');
const { t, versionShown, editing, version, saveVersion } = context;
</script>

