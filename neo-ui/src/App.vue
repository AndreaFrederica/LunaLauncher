<template>
  <q-layout view="hHh lpR fFf" class="bg-grey-10 text-white">
    <q-header elevated><q-toolbar>
      <q-toolbar-title>Luna Neo UI</q-toolbar-title>
      <q-btn flat icon="refresh" label="刷新" :disable="loading" @click="refresh" />
    </q-toolbar></q-header>
    <q-page-container><q-page padding>
      <h5 class="q-mt-sm">实例</h5>
      <q-banner v-if="error" rounded class="bg-red-9 q-mb-md">{{ error }}</q-banner>
      <div v-if="loading"><q-spinner class="q-mr-sm" />正在连接启动器…</div>
      <q-banner v-else-if="!error && !instances.length" class="bg-grey-9">当前数据目录还没有实例。</q-banner>
      <q-list bordered separator>
        <q-item v-for="instance in instances" :key="instance.id">
          <q-item-section>
            <q-item-label>{{ instance.name || instance.id }}</q-item-label>
            <q-item-label caption class="text-grey-4">{{ instance.id }}</q-item-label>
          </q-item-section>
        </q-item>
      </q-list>
    </q-page></q-page-container>
  </q-layout>
</template>
<script setup lang="ts">
import { onMounted, ref } from 'vue';
import { launcher } from './boot/launcher-api';
type Instance = { id: string; name?: string };
const instances = ref<Instance[]>([]);
const loading = ref(false);
const error = ref('');
async function refresh() {
  loading.value = true;
  error.value = '';
  try {
    const result = await launcher.execute<Instance[]>('instance.list', {});
    if (!result.ok) throw new Error(result.error || '读取实例失败');
    if (!Array.isArray(result.data)) throw new Error('启动器返回的实例列表格式无效');
    instances.value = result.data;
  } catch (e) {
    error.value = typeof e === 'object' && e !== null && 'message' in e ? String(e.message) : String(e);
  } finally {
    loading.value = false;
  }
}
onMounted(refresh);
</script>
