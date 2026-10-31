<template>
  <MelodyWindow kind="setup" @close="$emit('close')">
    <form class="melody-setup" @submit.prevent="$emit('complete', { name, memory, javaPath })">
      <div class="melody-setup__first">
        <MelodyFormRow label="游戏名字"><MelodyInput v-model="name" required /></MelodyFormRow>
        <MelodyFormRow label="最大内存" suffix="MB"><MelodyInput v-model="memory" inputmode="numeric" /></MelodyFormRow>
      </div>
      <div class="melody-setup__java"><MelodyFormRow label="java路径"><MelodyInput v-model="javaPath" /></MelodyFormRow><MelodyButton @click="browse">浏览</MelodyButton></div>
      <footer><MelodyButton type="submit">完成设置</MelodyButton></footer>
    </form>
  </MelodyWindow>
</template>
<script setup lang="ts">
import { ref } from 'vue';
import { isTauri } from '@tauri-apps/api/core';
import { open } from '@tauri-apps/plugin-dialog';
import MelodyButton from '../../components/MelodyButton.vue';
import MelodyFormRow from '../../components/MelodyFormRow.vue';
import MelodyInput from '../../components/MelodyInput.vue';
import MelodyWindow from '../../components/MelodyWindow.vue';
const emit = defineEmits<{ close: []; message: [text: string]; complete: [value: { name: string; memory: string; javaPath: string }] }>();
const name = ref(''), memory = ref('1024'), javaPath = ref('');
async function browse() {
  if (!isTauri()) { emit('message', '请填写 Java 的完整路径。'); return; }
  try { const path = await open({ multiple: false, directory: false, title: '选择 Java' }); if (typeof path === 'string') javaPath.value = path; }
  catch (error) { emit('message', String(error)); }
}
</script>
