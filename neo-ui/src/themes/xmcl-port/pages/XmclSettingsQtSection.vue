<template>
  <section class="xmcl-setting-section">
    <header><q-icon name="desktop_windows" /><div><h5>{{ t('xmcl.settings.qt') }}</h5><p>{{ t('xmcl.settings.qtHint') }}</p></div></header>
    <LauncherControlPanel :busy="busy" :execute="execute" />
    <SettingsPanel class="q-mt-xl" :settings="settings" category="qt-ui" :scope="isInstance ? 'instance' : 'launcher'" :busy="busy" @save="(key, value) => execute('settings.set', { ...scope, key, value })" @reset="key => execute('settings.reset', { ...scope, key })" />
  </section>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import LauncherControlPanel from '../../../components/LauncherControlPanel.vue';
import SettingsPanel from '../../../components/SettingsPanel.vue';
import { XmclSettingsContextKey } from './XmclSettingsContext';

const context = inject(XmclSettingsContextKey);
if (!context) throw new Error('XmclSettingsContext is missing');
const { t, isInstance, settings, busy, scope, execute } = context;
</script>

