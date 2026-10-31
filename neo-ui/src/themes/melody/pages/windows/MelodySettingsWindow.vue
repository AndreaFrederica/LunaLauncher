<template>
  <MelodyWindow kind="settings" @close="$emit('close')">
    <form class="melody-settings" @submit.prevent="$emit('save', { ...form })">
      <div class="melody-settings__columns">
        <MelodyGroupBox title="游戏设置">
          <div class="melody-settings__game-form">
            <div class="melody-settings__identity">
              <label :for="id + '-name'">游戏名字</label><MelodyInput :id="id + '-name'" v-model="form.name" />
              <label :for="id + '-password'">密码</label><MelodyInput :id="id + '-password'" v-model="form.password" type="password" autocomplete="off" />
              <MelodyChoice v-model:checked="form.official">正版登录</MelodyChoice>
            </div>
            <div class="melody-settings__memory">
              <span>最大内存</span>
              <MelodyRadioGroup v-model="form.memoryMode" label="内存设置" :options="[{ value: 'auto', label: '自动设置' }, { value: 'manual', label: '手动' }]" />
              <MelodyInput v-model="form.memory" aria-label="最大内存 MB" inputmode="numeric" /><span>MB</span>
            </div>
            <div class="melody-settings__skin">
              <span>启动器皮肤</span>
              <MelodyRadioGroup v-model="form.skinMode" label="启动器皮肤" :options="[{ value: 'random', label: '随机皮肤' }, { value: 'default', label: '默认皮肤' }]" />
            </div>
            <div class="melody-settings__java">
              <label :for="id + '-java'">java路径</label><MelodyInput :id="id + '-java'" v-model="form.javaPath" />
              <MelodyButton @click="browse">浏览</MelodyButton>
            </div>
          </div>
        </MelodyGroupBox>
        <MelodyGroupBox title="游戏管理" class="melody-settings__manage">
          <div class="melody-settings__manage-form">
            <div class="melody-settings__version"><span>游戏列表</span><MelodySelect v-model="form.game" :options="games" aria-label="游戏列表" /></div>
            <p>游戏目录(Forge mods,材质包,存档的目录)</p>
            <MelodyRadioGroup v-model="form.directory" vertical label="游戏目录" :options="directoryOptions" />
            <div class="melody-settings__note"><span>只在1.6以上的版本有效</span><MelodyButton @click="$emit('action', 'rename', form)">重命名</MelodyButton></div>
          </div>
        </MelodyGroupBox>
      </div>
      <footer class="melody-settings__footer">
        <div class="melody-settings__footer-left">
          <MelodyChoice v-model:checked="form.debug">调试模式</MelodyChoice>
          <MelodyButton @click="$emit('action', 'find-java', form)">查找java</MelodyButton>
          <MelodyButton @click="$emit('action', 'login-test', form)">正版登录测试</MelodyButton>
          <MelodyButton @click="$emit('action', 'delete-login', form)">删除正版信息</MelodyButton>
        </div>
        <div class="melody-settings__footer-right">
          <MelodyButton @click="$emit('action', 'delete-game', form)">删除选中游戏</MelodyButton>
          <MelodyButton type="submit" :disabled="busy">保存设置</MelodyButton>
        </div>
      </footer>
    </form>
  </MelodyWindow>
</template>
<script setup lang="ts">
import { reactive, useId } from 'vue';
import { isTauri } from '@tauri-apps/api/core';
import { open } from '@tauri-apps/plugin-dialog';
import MelodyButton from '../../components/MelodyButton.vue';
import MelodyChoice from '../../components/MelodyChoice.vue';
import MelodyRadioGroup from '../../components/MelodyRadioGroup.vue';
import MelodyGroupBox from '../../components/MelodyGroupBox.vue';
import MelodyInput from '../../components/MelodyInput.vue';
import MelodySelect from '../../components/MelodySelect.vue';
import MelodyWindow from '../../components/MelodyWindow.vue';
import { defaultSettings, type MelodySettings, type MelodyOption } from '../../types';
const props = withDefaults(defineProps<{ modelValue?: MelodySettings; games?: MelodyOption[]; busy?: boolean }>(), { games: () => [] });
const emit = defineEmits<{ close: []; message: [text: string]; save: [value: MelodySettings]; action: [action: string, value: MelodySettings] }>();
const id = useId();
const form = reactive({ ...defaultSettings, ...props.modelValue });
const directoryOptions = [{ value: 'shared', label: '保存到 .minecraft' }, { value: 'version', label: '保存到 .minecraft\\versions\\(版本名称)' }];
async function browse() {
  if (!isTauri()) { emit('message', '请在 java路径 中填写 javaw.exe 的完整路径。'); return; }
  try { const path = await open({ multiple: false, directory: false, title: '选择 Java' }); if (typeof path === 'string') form.javaPath = path; }
  catch (error) { emit('message', String(error)); }
}
</script>
