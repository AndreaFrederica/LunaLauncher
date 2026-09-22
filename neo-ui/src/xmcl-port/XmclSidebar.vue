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
defineProps<{instances:any[];selected:any;icons:Record<string,string>;page:string; pinned?:string[]}>();
const emit=defineEmits<{navigate:[page:string];select:[instance:any];launch:[instance:any];back:[];add:[]}>();
const dragging=ref('');
function onDrop(target:any){if(dragging.value && dragging.value!==target.id)emit('select',target);dragging.value='';}
</script>
<style scoped>
/* These dimensions and interaction states follow XMCL's AppSideBarClassic. */
.xmcl-sidebar{min-width:80px;width:80px;height:100%;display:flex;flex-direction:column;align-items:stretch;border-radius:var(--surface-menu-item-radius,10px);padding:8px 0;overflow:hidden;background:rgba(24,24,28,.68);backdrop-filter:blur(var(--blur-sidebar,18px));color:#eee}.xmcl-sidebar-section{display:flex;flex-direction:column;align-items:center;flex-shrink:0;padding:0 4px}.xmcl-sidebar-instances{flex:1 1 auto;min-height:0;overflow-y:auto;overflow-x:hidden;padding:0 4px;scrollbar-width:none}.xmcl-sidebar-instances::-webkit-scrollbar{display:none}.xmcl-divider{margin:6px 16px;height:2px;border-radius:1px;background:rgba(255,255,255,.12);flex-shrink:0}.xmcl-back{display:flex;align-items:center;justify-content:center;width:56px;height:32px;margin:4px auto 8px;padding:0;border:0;border-radius:8px;background:transparent;color:inherit;cursor:pointer}.xmcl-back:hover{background:rgba(255,255,255,.12)}.xmcl-sidebar-item{position:relative;display:block;width:48px;height:48px;padding:0;border:0;background:transparent;margin:4px auto;flex-shrink:0;text-decoration:none;color:inherit;--item-color:#4caf50}.xmcl-sidebar-content{position:relative;display:flex;align-items:center;justify-content:center;width:48px;height:48px;border-radius:50%;background:transparent;transition:border-radius .2s,background-color .2s,color .2s;overflow:hidden;cursor:pointer}.xmcl-sidebar-item:hover .xmcl-sidebar-content,.xmcl-sidebar-item.active .xmcl-sidebar-content{border-radius:16px;background:rgba(255,255,255,.18)}.xmcl-sidebar-item.active .xmcl-sidebar-content{background:var(--item-color);color:#fff}.xmcl-sidebar-item::before{content:'';position:absolute;left:-12px;top:50%;width:4px;height:0;border-radius:0 4px 4px 0;background:#fff;transform:translateY(-50%);transition:height .2s;opacity:.85}.xmcl-sidebar-item:hover::before{height:20px}.xmcl-sidebar-item.active::before{height:36px}.xmcl-add{--item-color:#4caf50}.xmcl-instance-wrapper{position:relative;width:100%;height:56px;display:flex;align-items:center;justify-content:center;flex-shrink:0}.xmcl-instance{position:relative;display:block;width:48px;height:48px;padding:0;border:0;background:transparent;color:inherit;cursor:pointer}.xmcl-instance-content{position:relative;display:flex;align-items:center;justify-content:center;width:48px;height:48px;border-radius:50%;overflow:hidden;background:transparent;transition:border-radius .2s,box-shadow .2s}.xmcl-instance:hover .xmcl-instance-content,.xmcl-instance.active .xmcl-instance-content{border-radius:16px}.xmcl-instance.active .xmcl-instance-content{box-shadow:0 0 0 2px #4caf50}.xmcl-instance-content img{width:100%;height:100%;object-fit:cover;user-select:none}.xmcl-instance-indicator{position:absolute;left:-12px;top:50%;width:4px;height:0;border-radius:0 4px 4px 0;background:#fff;transform:translateY(-50%);transition:height .2s;opacity:.85}.xmcl-instance:hover .xmcl-instance-indicator{height:20px}.xmcl-instance.active .xmcl-instance-indicator{height:36px}.xmcl-instance-status{position:absolute;right:2px;bottom:2px;width:20px;height:20px;display:flex;align-items:center;justify-content:center;border:2px solid #202024;border-radius:50%;background:#4caf50}.xmcl-pin{position:absolute;right:7px;top:0;width:16px;height:16px;display:flex;align-items:center;justify-content:center;border-radius:50%;background:#eab308;color:#fff;z-index:2}.dragging{opacity:.5}.xmcl-sidebar-bottom{margin-top:auto}
</style>
