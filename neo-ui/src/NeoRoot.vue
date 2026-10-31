<!-- SPDX-License-Identifier: MPL-2.0 -->
<template><RuntimeDebugWindow v-if="ready && runtimeDebug" /><MelodyThemeApp v-else-if="ready && melody" /><DefaultApp v-else-if="ready" /></template>
<script setup lang="ts">
import { computed, defineAsyncComponent, onMounted, onUnmounted, ref } from 'vue';
import { isTauri } from '@tauri-apps/api/core';
import { neoUiPreferences, useNeoUiPreferencesStore } from './neo-ui-preferences';
import RuntimeDebugWindow from './common/components/RuntimeDebugWindow.vue';
const DefaultApp = defineAsyncComponent(() => import('./App.vue'));
const MelodyThemeApp = defineAsyncComponent(() => import('./themes/melody/pages/MelodyThemeApp.vue'));
const hash = ref(location.hash);
const ready = ref(!isTauri());
const preferencesStore = useNeoUiPreferencesStore();
onMounted(async () => { await preferencesStore.hydrate(); ready.value = true; });
const updateHash = () => { hash.value = location.hash; };
window.addEventListener('hashchange', updateHash);
onUnmounted(() => window.removeEventListener('hashchange', updateHash));
const query = new URLSearchParams(location.search);
const standalone = query.has('instanceEditor') || query.has('resourceDownload');
const runtimeDebug = query.has('runtimeDebug');
const melody = computed(() => !standalone && (neoUiPreferences.shell === 'melody' || query.get('theme') === 'melody' || /^#\/melody(?:\/|$|\?)/.test(hash.value)));
</script>
