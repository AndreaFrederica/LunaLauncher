<template>
  <MelodyModernWindow kind="clients" @close="$emit('close')">
    <div class="melody-modern-column melody-client-list">
      <div class="melody-toolbar">
        <MelodyInput v-model="search" placeholder="搜索客户端" aria-label="搜索客户端" />
        <MelodyButton :disabled="loading" @click="refresh">刷新</MelodyButton>
        <MelodyButton @click="props.onGames?.()">下载游戏</MelodyButton>
      </div>
      <p v-if="error" class="melody-error" role="alert">{{ error }}</p>
      <div class="melody-client-list__items">
        <MelodyGroupBox v-for="item in filtered" :key="item.id" :title="item.name">
          <div class="melody-client-row">
            <div><strong>{{ item.runtime?.minecraftVersion || item.type }}</strong><p>{{ item.group || '未分组' }} · {{ item.running ? '运行中' : '未运行' }}</p></div>
            <MelodyButton @click="props.onEdit?.(item.id)">管理客户端</MelodyButton>
            <MelodyButton @click="select(item.id)">设为当前游戏</MelodyButton>
          </div>
        </MelodyGroupBox>
        <p v-if="!loading && !filtered.length" class="melody-empty">{{ search ? '没有匹配的客户端' : '还没有客户端，点击“下载游戏”添加。' }}</p>
      </div>
      <p v-if="notice" role="status">{{ notice }}</p>
    </div>
  </MelodyModernWindow>
</template>
<script setup lang="ts">
import { computed, ref } from 'vue';
import { storeToRefs } from 'pinia';
import MelodyModernWindow from '../../components/MelodyModernWindow.vue';
import MelodyInput from '../../components/MelodyInput.vue';
import MelodyButton from '../../components/MelodyButton.vue';
import MelodyGroupBox from '../../components/MelodyGroupBox.vue';
import { useLauncherSessionStore } from '../../../../common/launcher-session';
const props = defineProps<{ onGames?: () => void; onEdit?: (id: string) => void }>();
defineEmits<{ close: []; games: []; edit: [id: string] }>();
const session = useLauncherSessionStore();
const { instances, loading, error } = storeToRefs(session);
const refresh = session.refresh;
const search = ref(''), notice = ref('');
const filtered = computed(() => instances.value.filter(item => item.name.toLowerCase().includes(search.value.toLowerCase())));
function select(id: string) { session.selectInstance(id); notice.value = '已设为当前游戏'; }
</script>
