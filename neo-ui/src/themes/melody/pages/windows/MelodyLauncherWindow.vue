<template>
  <MelodyWindow kind="launcher" @close="$emit('close')">
    <img class="melody-launcher__background" :src="background" :style="{ objectFit: backgroundFit }" alt="" draggable="false" />
    <div class="melody-launcher__controls">
      <MelodyButton variant="primary" class="melody-launcher__launch" :disabled="busy" @click="$emit('launch')">{{ busy ? '启动中…' : '启动游戏' }}</MelodyButton>
      <div class="melody-launcher__side">
        <MelodyButton @click="openWindow('games')">更新游戏</MelodyButton>
        <MelodyButton @click="openWindow('preferences')">设置</MelodyButton>
      </div>
      <div class="melody-launcher__bottom" :class="{ 'has-account-selector': accounts && accounts.length > 1 }">
        <MelodySelect :model-value="modelValue" :options="versions" aria-label="选择游戏版本" @update:model-value="$emit('update:modelValue', $event)" />
        <MelodySelect v-if="accounts && accounts.length > 1" class="melody-launcher__account-select" :model-value="accountId" :options="accounts" aria-label="选择账户" @update:model-value="$emit('account-change', $event)" />
        <MelodyButton variant="link" @click="$emit('folder')">打开目录</MelodyButton>
      </div>
    </div>
    <footer class="melody-launcher__footer"><span :style="footerStyle" :title="footer">{{ footer }}</span><MelodyButton variant="link" @click="openWindow('clients')">客户端管理</MelodyButton></footer>
  </MelodyWindow>
</template>
<script setup lang="ts">
import type { CSSProperties } from 'vue';
import MelodyButton from '../../components/MelodyButton.vue';
import MelodySelect from '../../components/MelodySelect.vue';
import MelodyWindow from '../../components/MelodyWindow.vue';
import builtinBackground from '../../assets/minecraft-water.jpg';
import type { MelodyOption } from '../../types';
const props = withDefaults(defineProps<{ background?: string; backgroundFit?: 'cover' | 'contain' | 'fill'; footer: string; footerStyle?: CSSProperties; versions?: MelodyOption[]; accounts?: MelodyOption[]; accountId?: string; modelValue?: string; busy?: boolean; onOpen?: (kind: 'games' | 'preferences' | 'clients') => void }>(), { background: builtinBackground, backgroundFit: 'fill', versions: () => [], accounts: () => [], accountId: '', modelValue: '' });
defineEmits<{ close: []; open: [kind: 'games' | 'preferences' | 'clients']; 'account-change': [value: string]; launch: []; folder: []; 'update:modelValue': [value: string] }>();
function openWindow(kind: 'games' | 'preferences' | 'clients') {
  props.onOpen?.(kind);
}
</script>
