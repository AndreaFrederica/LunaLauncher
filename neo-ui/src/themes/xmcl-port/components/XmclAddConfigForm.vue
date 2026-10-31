<template>
  <div class="row q-gutter-sm">
    <q-select class="col" dark outlined v-model="form.version" :options="versions" :label="t('xmcl.add.mcVersion')" />
    <q-btn flat :label="t('xmcl.add.refreshVersions')" :disable="busy" @click="loadVersions" />
  </div>
  <q-select dark outlined v-model="form.loader" :options="['', 'fabric', 'forge', 'neoforge', 'quilt']" :label="t('xmcl.add.loader')" class="q-mt-md" />
</template>

<script setup lang="ts">
import { inject } from 'vue';
import { XmclAddInstanceContextKey } from './XmclAddInstanceContext';

const context = inject(XmclAddInstanceContextKey);
if (!context) throw new Error('XmclAddInstanceContext is missing');
const { t, form, versions, busy, loadVersions } = context;
</script>

