<template>
  <section class="xmcl-setting-section">
    <header><q-icon name="code" /><div><h5>{{ t('xmcl.settings.runtime') }}</h5><p>{{ t('xmcl.settings.runtimeHint') }}</p></div></header>
    <JavaPanel :scope="scope" :settings="settings" :busy="busy" :execute="execute" @changed="emit('changed')" />
  </section>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import JavaPanel from '../../../components/JavaPanel.vue';
import { XmclSettingsContextKey } from './XmclSettingsContext';

const context = inject(XmclSettingsContextKey);
if (!context) throw new Error('XmclSettingsContext is missing');
const { t, emit, scope, settings, busy, execute } = context;
</script>

