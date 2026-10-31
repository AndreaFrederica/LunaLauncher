<template>
  <section class="xmcl-setting-section">
    <header><q-icon :name="categoryIcon" /><div><h5>{{ categoryLabel }}</h5><p>{{ categoryDescription }}</p></div></header>
    <SettingsPanel v-model:category="category" :settings="settings" :scope="isInstance ? 'instance' : 'launcher'" :busy="busy" @save="(key, value) => execute('settings.set', { ...scope, key, value })" @reset="key => execute('settings.reset', { ...scope, key })" />
  </section>
</template>

<script setup lang="ts">
import { inject } from 'vue';
import SettingsPanel from '../../../components/SettingsPanel.vue';
import { XmclSettingsContextKey } from './XmclSettingsContext';

const context = inject(XmclSettingsContextKey);
if (!context) throw new Error('XmclSettingsContext is missing');
const { isInstance, settings, busy, scope, execute, category, categoryLabel, categoryIcon, categoryDescription } = context;
</script>

