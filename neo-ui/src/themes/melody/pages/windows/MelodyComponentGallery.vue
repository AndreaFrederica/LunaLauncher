<template>
  <MelodyWindow kind="components" show-title @close="$emit('close')">
    <div class="melody-gallery">
      <MelodyGroupBox title="按钮"><div class="melody-gallery__row"><MelodyButton @click="count++">按钮 {{ count || '' }}</MelodyButton><MelodyButton variant="primary">主要按钮</MelodyButton><MelodyButton variant="link">链接</MelodyButton><MelodyButton disabled>禁用</MelodyButton></div></MelodyGroupBox>
      <MelodyGroupBox title="输入与选择"><div class="melody-gallery__row"><MelodyInput v-model="text" aria-label="输入示例" /><MelodySelect v-model="choice" :options="options" aria-label="下拉示例" /></div><div class="melody-gallery__row"><MelodyChoice v-model:checked="checked">复选框</MelodyChoice><MelodyRadioGroup v-model="choice" label="单选示例" :options="options" /></div></MelodyGroupBox>
      <MelodyGroupBox title="进度与列表"><MelodyProgress :model-value="42" label="示例进度" /><MelodyTable v-model="selected" label="示例列表" :columns="[{ key: 'name', label: '名称' }, { key: 'description', label: '简介' }]" :rows="[{ id: 'sample', name: '示例', description: '列表选中状态' }]" /></MelodyGroupBox>
    </div>
  </MelodyWindow>
</template>
<script setup lang="ts">
import { ref } from 'vue';
import MelodyWindow from '../../components/MelodyWindow.vue';
import MelodyButton from '../../components/MelodyButton.vue';
import MelodyInput from '../../components/MelodyInput.vue';
import MelodySelect from '../../components/MelodySelect.vue';
import MelodyChoice from '../../components/MelodyChoice.vue';
import MelodyRadioGroup from '../../components/MelodyRadioGroup.vue';
import MelodyGroupBox from '../../components/MelodyGroupBox.vue';
import MelodyProgress from '../../components/MelodyProgress.vue';
import MelodyTable from '../../components/MelodyTable.vue';
defineEmits<{ close: [] }>();
const count = ref(0), text = ref('忘却的旋律'), choice = ref('first'), checked = ref(true), selected = ref('');
const options = [{ value: 'first', label: '选项一' }, { value: 'second', label: '选项二' }];
</script>
