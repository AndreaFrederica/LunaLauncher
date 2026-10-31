<template>
  <section class="xmcl-setting-section">
    <header><q-icon name="info" /><div><h5>{{ t('xmcl.settings.about') }}</h5><p>{{ t('xmcl.settings.aboutHint') }}</p></div></header>
    <NeoUiAboutPanel :execute="execute" />
  </section>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import NeoUiAboutPanel from '../../../components/NeoUiAboutPanel.vue';
import { XmclSettingsContextKey } from './XmclSettingsContext';

const context = inject(XmclSettingsContextKey);
if (!context) throw new Error('XmclSettingsContext is missing');
const { t, execute } = context;
</script>

