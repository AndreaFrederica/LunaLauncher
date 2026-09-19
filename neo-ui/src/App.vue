<template><q-layout view="hHh LpR"><q-header elevated><q-toolbar><q-toolbar-title>Luna Neo UI</q-toolbar-title><q-btn flat icon="refresh" label="Refresh" @click="refresh" /></q-toolbar></q-header><q-page-container><router-view /><div class="q-pa-md"><q-banner v-if="error" rounded class="bg-red-2">{{ error }}</q-banner><q-spinner v-if="loading" /><q-list bordered separator><q-item v-for="instance in instances" :key="instance.id"><q-item-section><q-item-label>{{ instance.name || instance.id }}</q-item-label><q-item-label caption>{{ instance.id }}</q-item-label></q-item-section></q-item></q-list></div></q-page-container></q-layout></template>
<script setup lang="ts">
import { onMounted, ref } from 'vue';
import { launcher } from './boot/launcher-api';
const instances = ref<any[]>([]); const loading = ref(false); const error = ref('');
async function refresh() { loading.value = true; error.value = ''; try { const result = await launcher.execute('instance.list', {}); instances.value = result.data ?? []; } catch (e) { error.value = String(e); } finally { loading.value = false; } }
onMounted(refresh);
</script>
