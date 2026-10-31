<template>
  <q-input dark outlined v-model="form.source" :label="t('xmcl.add.packSource')" />
  <q-file dark outlined v-model="importFile" :label="t('xmcl.add.chooseFile')" accept=".zip,.mrpack,.tar.gz" class="q-mt-md" @update:model-value="readFile" />
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { XmclAddInstanceContextKey } from './XmclAddInstanceContext';

const context = inject(XmclAddInstanceContextKey);
if (!context) throw new Error('XmclAddInstanceContext is missing');
const { t, form, importFile, readFile } = context;
</script>

