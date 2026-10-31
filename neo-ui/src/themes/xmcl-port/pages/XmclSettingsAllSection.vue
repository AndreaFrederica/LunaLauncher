<template>
  <section class="xmcl-setting-section">
    <header><q-icon name="tune" /><div><h5>{{ isInstance ? t('xmcl.settings.instance') : t('xmcl.settings.launcher') }}</h5><p>{{ t('xmcl.settings.allHint') }}</p></div></header>
    <InstanceSettingsPanel v-show="isInstance" :settings="settings" :busy="busy" :scope="scope" :execute="execute" @changed="emit('changed')" />
    <LauncherSettingsPanel v-show="!isInstance" :settings="settings" :busy="busy" :scope="scope" :execute="execute" @changed="emit('changed')" />
  </section>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import InstanceSettingsPanel from '../../../components/InstanceSettingsPanel.vue';
import LauncherSettingsPanel from '../../../components/LauncherSettingsPanel.vue';
import { XmclSettingsContextKey } from './XmclSettingsContext';

const context = inject(XmclSettingsContextKey);
if (!context) throw new Error('XmclSettingsContext is missing');
const { t, emit, isInstance, settings, busy, scope, execute } = context;
</script>

