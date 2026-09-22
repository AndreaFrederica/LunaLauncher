<template>
  <!-- Port of XMCL HomeHeader: sticky title row, route actions, and drop zone. -->
  <header class="xmcl-header" :class="{compact}">
    <div class="xmcl-header-content">
      <div class="xmcl-header-primary">
        <span class="xmcl-home-title">{{ selected?.name || 'Minecraft' }}<small v-if="selected?.runtime?.minecraft"> {{ selected.runtime.minecraft }}</small></span>
        <div class="xmcl-header-spacer" />
        <q-btn flat round icon="folder_open" aria-label="打开实例目录" @click="emit('open-folder')" />
        <q-btn flat round icon="tune" aria-label="实例设置" @click="emit('settings')" />
        <q-btn unelevated color="primary" icon="play_arrow" label="启动" :disable="!selected || selected.running" @click="emit('launch')" />
      </div>
      <div class="xmcl-header-extension"><q-tabs :model-value="tab" dense align="left" outside-arrows mobile-arrows @update:model-value="v=>emit('tab',String(v))"><q-tab name="home" label="概览"/><q-tab name="mods" label="模组"/><q-tab name="resourcepacks" label="资源包"/><q-tab name="shaderpacks" label="光影"/><q-tab name="save" label="世界"/></q-tabs></div>
    </div>
    <div v-if="dragover" class="xmcl-drop-zone" @dragenter="overcount++" @dragleave="overcount--" @drop.prevent="drop"><div :class="{hot:overcount>0}"><q-icon name="save_alt" size="32px"/><span>将整合包拖放到这里导入</span></div></div>
  </header>
</template>
<script setup lang="ts">
import { ref } from 'vue';
defineProps<{selected:any;tab:string;compact?:boolean;dragover?:boolean}>();
const emit=defineEmits<{settings:[];launch:[];'open-folder':[];tab:[tab:string];drop:[files:FileList|null]}>();
const overcount=ref(0);
function drop(e:DragEvent){overcount.value=0;emit('drop',e.dataTransfer?.files||null);}
</script>
<style scoped>
.xmcl-header{position:sticky;top:0;z-index:20;max-width:100%;padding:2.5rem .5rem 0;user-select:none}.xmcl-header::before{content:'';position:absolute;inset:0;bottom:-70px;z-index:-1;pointer-events:none;background:linear-gradient(var(--app-bar-color,rgba(20,20,25,.88)),transparent);backdrop-filter:blur(var(--app-bar-blur,18px));-webkit-mask-image:linear-gradient(#000 40%,transparent);mask-image:linear-gradient(#000 40%,transparent)}.xmcl-header.compact{padding-top:.2rem;padding-bottom:.5rem}.xmcl-header-content{display:flex;flex-direction:column;max-width:1200px;margin:auto}.xmcl-header-primary{display:flex;align-items:baseline;min-height:60px;gap:4px;padding:0 1.5rem}.xmcl-home-title{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:2.425rem;letter-spacing:-.04em;font-weight:500}.xmcl-home-title small{font-size:.55em;color:rgba(255,255,255,.58)}.xmcl-header-spacer{flex:1}.xmcl-header-extension{padding:0 1rem;margin-top:1rem}.xmcl-header-extension :deep(.q-tab){text-transform:none}.xmcl-drop-zone{position:absolute;inset:0;top:0;padding:20px;display:flex}.xmcl-drop-zone>div{width:100%;height:100%;display:flex;align-items:center;justify-content:center;gap:10px;border:2px dashed #fbc02d;border-radius:16px;background:rgba(251,192,45,.15);transition:transform .2s}.xmcl-drop-zone>div.hot{transform:scale(1.0125);background:rgba(251,192,45,.28)}@media(max-width:900px){.xmcl-header{padding-left:.25rem!important;padding-right:.25rem!important}.xmcl-header-primary{padding-left:.75rem!important}.xmcl-home-title{font-size:1.8rem}}
</style>
