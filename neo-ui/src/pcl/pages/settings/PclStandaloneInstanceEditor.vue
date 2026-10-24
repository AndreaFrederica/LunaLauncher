<template>
  <div class="pcl-standalone-editor">
    <PclSetupPage
      :settings="[]"
      :busy="false"
      :scope="{ scope: 'instance', instance: instanceId }"
      :execute="execute"
      :accounts="accounts"
      @changed="refreshAccounts"
    />
  </div>
</template>

<script setup lang="ts">
import { onMounted, ref } from 'vue';
import PclSetupPage from './PclSetupPage.vue';
import { launcher } from '../../../boot/launcher-api';

const props = defineProps<{ instanceId: string }>();
const accounts = ref<any[]>([]);

async function execute(operation: string, parameters: Record<string, unknown> = {}) {
  const result = await launcher.execute(operation, parameters);
  if (result && result.ok === false) throw new Error(result.error || operation);
  return result?.data ?? result;
}

async function refreshAccounts() {
  try {
    const result = await launcher.execute('account.list', {});
    accounts.value = Array.isArray(result?.data) ? result.data : Array.isArray(result) ? result : [];
  } catch {
    accounts.value = [];
  }
}

onMounted(() => {
  // Native editor windows can have a different WebView origin than the main
  // window, so their localStorage may not contain the PCL preferences. The
  // editor is intentionally PCL-styled regardless of where it was opened.
  const root = document.documentElement;
  root.dataset.neoShell = 'pcl';
  root.dataset.pclTitleLogo = 'off';
  root.dataset.pclTitleBadge = 'off';
  document.title = '实例设置 - Luna Launcher';
  void refreshAccounts();
});
</script>

<style scoped>
.pcl-standalone-editor {
  width: 100vw;
  height: 100vh;
  overflow: hidden;
  background: var(--pcl-page-bg);
}

.pcl-standalone-editor :deep(.pcl-setup-page) {
  width: 100%;
  height: 100%;
}

.pcl-standalone-editor :deep(.pcl-page-main) {
  height: 100%;
  overflow-y: auto;
}
</style>
