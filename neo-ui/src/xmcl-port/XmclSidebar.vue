<template>
  <!-- Port of XMCL AppSideBarClassic/AppSideBarInstances/AppSideBarItem. -->
  <aside class="xmcl-sidebar moveable" role="navigation" aria-label="Sidebar navigation">
    <div class="xmcl-sidebar-section">
      <button class="xmcl-back non-moveable" aria-label="返回" @click="emit('back')"><q-icon name="arrow_back" size="18px" /></button>
      <button class="xmcl-sidebar-item non-moveable" :class="{active: page==='accounts'}" aria-label="账户" @click="emit('navigate','accounts')"><span class="xmcl-sidebar-content"><q-icon name="person" size="23px" /></span></button>
      <button class="xmcl-sidebar-item non-moveable" :class="{active: page==='store'}" aria-label="下载中心" @click="emit('navigate','store')"><span class="xmcl-sidebar-content"><q-icon name="store" size="26px" /></span></button>
    </div>
    <div class="xmcl-divider" />
    <div class="xmcl-sidebar-instances" @dragover.prevent>
      <div v-for="instance in instances" :key="instance.id" class="xmcl-instance-wrapper" :class="{dragging: dragging===instance.id}">
        <button class="xmcl-instance non-moveable" :class="{active:selected?.id===instance.id,running:instance.running}" :title="instance.name" draggable="true" @click="emit('select',instance)" @dblclick="emit('launch',instance)" @dragstart="dragging=instance.id" @dragend="dragging=''" @drop.prevent="onDrop(instance)">
          <span class="xmcl-instance-indicator" /><span class="xmcl-instance-content"><img :src="icons[instance.id] || fallback" :alt="instance.name" draggable="false"/><span v-if="instance.running" class="xmcl-instance-status"><q-icon name="play_arrow" size="14px" /></span></span>
        </button>
        <span v-if="pinned.includes(instance.id)" class="xmcl-pin"><q-icon name="push_pin" size="10px" /></span>
      </div>
      <button id="create-instance-button" class="xmcl-sidebar-item xmcl-add non-moveable" aria-label="添加实例" @click="emit('add')"><span class="xmcl-sidebar-content"><q-icon name="add" size="25px" /></span></button>
    </div>
    <div class="xmcl-divider" />
    <div class="xmcl-sidebar-section xmcl-sidebar-bottom">
      <button class="xmcl-sidebar-item non-moveable" :class="{active:page==='multiplayer'}" aria-label="多人游戏" @click="emit('navigate','multiplayer')"><span class="xmcl-sidebar-content"><q-icon name="hub" size="23px" /></span></button>
      <button class="xmcl-sidebar-item non-moveable" :class="{active:page==='settings'}" aria-label="设置" @click="emit('navigate','settings')"><span class="xmcl-sidebar-content"><q-icon name="settings" size="23px" /></span></button>
    </div>
  </aside>
</template>
<script setup lang="ts">
import { ref } from 'vue';
import fallback from './assets/minecraft.webp';
withDefaults(defineProps<{instances:any[];selected:any;icons:Record<string,string>;page:string; pinned?:string[]}>(), { pinned: () => [] });
const emit=defineEmits<{navigate:[page:string];select:[instance:any];launch:[instance:any];back:[];add:[]}>();
const dragging=ref('');
function onDrop(target:any){if(dragging.value && dragging.value!==target.id)emit('select',target);dragging.value='';}
</script>
<style scoped>
/* These dimensions and interaction states follow XMCL's AppSideBarClassic. */
.xmcl-sidebar{position:fixed;top:42px;bottom:8px;left:8px;z-index:30;height:auto}
</style>
