<template>
  <q-input dark outlined v-model="form.version" :label="t('xmcl.add.serverVersion')" class="q-mb-md" />
  <q-input dark outlined v-model="form.source" :label="t('xmcl.add.serverJar')" />
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { XmclAddInstanceContextKey } from './XmclAddInstanceContext';

const context = inject(XmclAddInstanceContextKey);
if (!context) throw new Error('XmclAddInstanceContext is missing');
const { t, form } = context;
</script>

