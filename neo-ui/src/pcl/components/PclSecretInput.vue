<template>
  <div class="pcl-secret-input">
    <PclInput v-model="draft" type="password" :aria-label="label" autocomplete="new-password"
      :placeholder="configured ? '已保存，输入新值以替换' : '未设置，可留空'" :disabled="disabled || saving" />
    <PclButton size="sm" :disabled="disabled || saving || !draft" @click="commit(draft)">保存</PclButton>
    <PclButton size="sm" variant="ghost" :disabled="disabled || saving || !configured" @click="commit('')">清除</PclButton>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue';
import PclInput from './PclInput.vue';
import PclButton from './PclButton.vue';
const props = defineProps<{
  label: string; configured: boolean; disabled?: boolean; save: (value: string) => Promise<boolean>;
}>();
const draft = ref('');
const saving = ref(false);
async function commit(value: string) {
  saving.value = true;
  try { if (await props.save(value)) draft.value = ''; }
  finally { saving.value = false; }
}
</script>

<style scoped>
.pcl-secret-input { display: flex; align-items: center; flex-wrap: wrap; gap: 8px; width: 100%; }
.pcl-secret-input .pcl-input { flex: 1; min-width: 140px; }
</style>
