<template>
  <q-input dark outlined v-model="form.source" :label="t('xmcl.add.packSource')" />
  <q-btn v-if="desktop" :label="t('xmcl.add.chooseFile')" :disable="busy" class="q-mt-md" @click="chooseFile" />
  <q-file v-else dark outlined v-model="importFile" :label="t('xmcl.add.chooseFile')" accept=".zip,.mrpack,.rar,.tar.gz" class="q-mt-md" @update:model-value="readFile" />
  <q-banner v-if="fileError" class="bg-red-9 q-mt-md" role="alert">{{ fileError }}</q-banner>
</template>

<script setup lang="ts">
import { inject, ref } from 'vue';
import { isTauri } from '@tauri-apps/api/core';
import { open } from '@tauri-apps/plugin-dialog';
import { XmclAddInstanceContextKey } from './XmclAddInstanceContext';

const context = inject(XmclAddInstanceContextKey);
if (!context) throw new Error('XmclAddInstanceContext is missing');
const { t, form, importFile, readFile, busy } = context;
const desktop = isTauri();
const fileError = ref('');

async function chooseFile() {
  fileError.value = '';
  try {
    const selected = await open({
      title: t('xmcl.add.chooseFile'),
      multiple: false,
      directory: false,
      filters: [{ name: t('download.importPack'), extensions: ['zip', 'mrpack', 'rar', 'tar.gz'] }],
    });
    if (typeof selected === 'string') form.value.source = selected;
  } catch (error) {
    fileError.value = String(error);
  }
}
</script>

