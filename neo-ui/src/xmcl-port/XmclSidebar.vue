<template>
  <aside class="sidebar moveable" role="navigation" aria-label="XMCL sidebar">
    <div class="sidebar__section">
      <button class="sidebar-back-btn" aria-label="返回实例库" @click="emit('back')"><span class="material-icons">arrow_back</span></button>
      <button class="sidebar-item" :class="{ active: page === 'accounts' }" aria-label="账户" @click="emit('navigate', 'accounts')"><span class="sidebar-item__content"><span class="material-icons">person</span></span></button>
      <button class="sidebar-item" :class="{ active: page === 'store' }" aria-label="下载中心" @click="emit('navigate', 'store')"><span class="sidebar-item__content"><span class="material-icons">store</span></span></button>
    </div>
    <div class="sidebar__divider" />
    <div class="sidebar__instances" role="group" aria-label="实例">
      <div v-for="instance in instances" :key="instance.id" class="sidebar-instance" :class="{ dragging: dragging === instance.id }">
        <button class="sidebar-item sidebar-instance__button" :class="{ active: selected?.id === instance.id, running: instance.running }" :title="instance.name" draggable="true" @click="emit('select', instance)" @dblclick="emit('launch', instance)" @dragstart="dragging = instance.id" @dragend="dragging = ''" @drop.prevent="onDrop(instance)">
          <span class="sidebar-instance__indicator" />
          <span class="sidebar-instance__content"><img :src="icons[instance.id] || fallback" :alt="instance.name" draggable="false" /><span v-if="instance.running" class="sidebar-instance__status"><span class="material-icons">play_arrow</span></span></span>
        </button>
      </div>
      <button id="create-instance-button" class="sidebar-item sidebar-add" aria-label="添加实例" @click="emit('add')"><span class="sidebar-item__content"><span class="material-icons">add</span></span></button>
    </div>
    <div class="sidebar__divider" />
    <div class="sidebar__section sidebar__bottom">
      <button class="sidebar-item" :class="{ active: page === 'multiplayer' }" aria-label="多人游戏" @click="emit('navigate', 'multiplayer')"><span class="sidebar-item__content"><span class="material-icons">hub</span></span></button>
      <button class="sidebar-item" :class="{ active: page === 'settings' }" aria-label="设置" @click="emit('navigate', 'settings')"><span class="sidebar-item__content"><span class="material-icons">settings</span></span></button>
    </div>
  </aside>
</template>
<script setup lang="ts">
import { ref } from 'vue';
import fallback from './assets/minecraft.webp';
defineProps<{ instances: any[]; selected: any; icons: Record<string, string>; page: string }>();
const emit = defineEmits<{ navigate: [page: string]; select: [instance: any]; launch: [instance: any]; back: []; add: [] }>();
const dragging = ref('');
function onDrop(target: any) { if (dragging.value && dragging.value !== target.id) emit('select', target); dragging.value = ''; }
</script>
<style scoped>
.sidebar{position:fixed;top:38px;bottom:8px;left:8px;z-index:30;min-width:80px;width:80px;display:flex;flex-direction:column;align-items:stretch;border-radius:var(--surface-menu-item-radius,12px);padding:8px 0;overflow:hidden;background:rgba(24,24,28,.72);backdrop-filter:blur(18px);color:#eee;box-shadow:0 8px 32px rgba(0,0,0,.18)}
.sidebar__section{display:flex;flex-direction:column;align-items:center;flex-shrink:0;padding:0 4px}.sidebar__instances{flex:1 1 auto;min-height:0;overflow-y:auto;overflow-x:hidden;padding:0 4px;scrollbar-width:none}.sidebar__instances::-webkit-scrollbar{display:none}.sidebar__divider{margin:6px 16px;height:2px;border-radius:1px;background:rgba(255,255,255,.12);flex-shrink:0}.sidebar-back-btn{display:flex;align-items:center;justify-content:center;width:56px;height:32px;margin:4px auto 8px;padding:0;border:0;border-radius:8px;background:transparent;color:inherit;cursor:pointer}.sidebar-back-btn:hover{background:rgba(255,255,255,.12)}.sidebar-item{position:relative;display:block;width:48px;height:48px;padding:0;border:0;background:transparent;margin:4px auto;flex-shrink:0;color:inherit;cursor:pointer}.sidebar-item__content{position:relative;display:flex;align-items:center;justify-content:center;width:48px;height:48px;border-radius:50%;background:transparent;transition:border-radius .2s,background-color .2s,color .2s;overflow:hidden}.sidebar-item:hover .sidebar-item__content,.sidebar-item.active .sidebar-item__content{border-radius:16px;background:rgba(255,255,255,.16)}.sidebar-item.active .sidebar-item__content{background:#4caf50;color:#fff}.sidebar-item::before{content:'';position:absolute;left:-12px;top:50%;width:4px;height:0;border-radius:0 4px 4px 0;background:#fff;transform:translateY(-50%);transition:height .2s;opacity:.85}.sidebar-item:hover::before{height:20px}.sidebar-item.active::before{height:36px}.sidebar-instance{position:relative;width:100%;height:56px;display:flex;align-items:center;justify-content:center;flex-shrink:0}.sidebar-instance__button{position:relative;margin:0}.sidebar-instance__content{position:relative;display:flex;align-items:center;justify-content:center;width:48px;height:48px;border-radius:50%;overflow:hidden;background:transparent;transition:border-radius .2s,box-shadow .2s}.sidebar-instance__button:hover .sidebar-instance__content,.sidebar-instance__button.active .sidebar-instance__content{border-radius:16px}.sidebar-instance__button.active .sidebar-instance__content{box-shadow:0 0 0 2px #4caf50}.sidebar-instance__content img{width:100%;height:100%;object-fit:cover;user-select:none}.sidebar-instance__indicator{position:absolute;left:-12px;top:50%;width:4px;height:0;border-radius:0 4px 4px 0;background:#fff;transform:translateY(-50%);transition:height .2s;opacity:.85}.sidebar-instance__button:hover .sidebar-instance__indicator{height:20px}.sidebar-instance__button.active .sidebar-instance__indicator{height:36px}.sidebar-instance__status{position:absolute;right:2px;bottom:2px;width:20px;height:20px;display:flex;align-items:center;justify-content:center;border:2px solid #202024;border-radius:50%;background:#4caf50}.sidebar-instance__status .material-icons{font-size:14px}.sidebar-add{--item-color:#4caf50}.dragging{opacity:.5}.sidebar__bottom{margin-top:auto}.material-icons{font-size:23px;line-height:1}
</style>
