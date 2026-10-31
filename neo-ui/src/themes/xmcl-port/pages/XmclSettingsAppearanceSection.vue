<template>
  <section class="xmcl-setting-section">
    <header><q-icon name="palette" /><div><h5>{{ t('xmcl.settings.appearance') }}</h5><p>{{ t('xmcl.settings.appearanceHint') }}</p></div></header>
    <NeoUiAppearancePanel />
  </section>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import NeoUiAppearancePanel from '../../../components/NeoUiAppearancePanel.vue';
import { XmclSettingsContextKey } from './XmclSettingsContext';

const context = inject(XmclSettingsContextKey);
if (!context) throw new Error('XmclSettingsContext is missing');
const { t } = context;
</script>

