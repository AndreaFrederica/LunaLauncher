<template>
  <MelodyWindow kind="manager" @close="$emit('close')">
    <div class="melody-manager">
      <MelodyTable v-model="selected" label="版本列表" :columns="columns" :rows="rows" />
      <footer class="melody-manager__controls">
        <MelodyRadioGroup v-model="pack" label="安装类型" :options="packOptions" />
        <MelodySelect v-model="version" :options="versions" aria-label="请选择游戏版本" />
        <MelodyButton @click="$emit('update-sounds', version)">更新声音文件</MelodyButton>
        <MelodyButton :disabled="pack !== 'simple'" @click="$emit('install', selected || version, pack)">更新游戏</MelodyButton>
      </footer>
    </div>
  </MelodyWindow>
</template>
<script setup lang="ts">
import { ref } from 'vue';
import MelodyButton from '../../components/MelodyButton.vue';
import MelodyRadioGroup from '../../components/MelodyRadioGroup.vue';
import MelodySelect from '../../components/MelodySelect.vue';
import MelodyTable from '../../components/MelodyTable.vue';
import MelodyWindow from '../../components/MelodyWindow.vue';
import type { MelodyOption } from '../../types';
withDefaults(defineProps<{ rows?: { id: string; [key: string]: string }[]; versions?: MelodyOption[] }>(), { rows: () => [], versions: () => [] });
defineEmits<{ close: []; install: [version: string, pack: string]; 'update-sounds': [version: string] }>();
const pack = ref('simple'), selected = ref(''), version = ref('');
const packOptions = [{ value: 'simple', label: '简单整合' }, { value: 'official', label: '官方原版' }, { value: 'forge', label: 'Forge' }, { value: 'forge-optifine', label: 'Forge+OptiFine' }];
const columns = [{ key: 'version', label: '版本', width: '80px' }, { key: 'name', label: '名称', width: '150px' }, { key: 'description', label: '简介' }];
</script>
