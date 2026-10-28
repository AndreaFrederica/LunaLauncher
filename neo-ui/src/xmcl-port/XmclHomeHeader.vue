<template>
  <header class="header sticky" :class="{ compact }">
    <div class="header-content">
      <div class="header-primary-row">
        <span class="home-title">{{ selected?.name || 'Minecraft' }}<small v-if="selected?.runtime?.minecraft"> {{ selected.runtime.minecraft }}</small></span>
        <div class="header-spacer" />
        <button class="header-action" :aria-label="t('xmcl.header.openFolder')" @click="emit('open-folder')"><span class="material-icons">folder_open</span></button>
        <button class="header-action" :aria-label="t('xmcl.header.settings')" @click="emit('settings')"><span class="material-icons">tune</span></button>
        <button id="launch-button" class="header-launch" :disabled="!selected || selected.running" @click="emit('launch')"><span class="material-icons">play_arrow</span>{{ t('xmcl.header.launch') }}</button>
      </div>
      <div class="header-extension" role="tablist">
        <button v-for="item in tabs" :key="item.id" class="header-tab" :class="{ active: tab === item.id }" role="tab" :aria-selected="tab === item.id" @click="emit('tab', item.id)">{{ item.label }}</button>
      </div>
    </div>
    <div v-if="dragover" class="drop-zone" @dragenter="overcount++" @dragleave="overcount--" @drop.prevent="drop"><div :class="{ hot: overcount > 0 }"><span class="material-icons">save_alt</span><span>{{ t('xmcl.header.dropPack') }}</span></div></div>
  </header>
</template>
<script setup lang="ts">
import { ref } from 'vue';
import { useI18n } from 'vue-i18n';
defineProps<{ selected: any; tab: string; compact?: boolean; dragover?: boolean }>();
const emit = defineEmits<{ settings: []; launch: []; 'open-folder': []; tab: [tab: string]; drop: [files: FileList | null] }>();
const { t } = useI18n();
const tabs = [{ id: 'home', label: t('xmcl.header.overview') }, { id: 'mods', label: t('xmcl.header.mods') }, { id: 'resourcepacks', label: t('xmcl.header.resourcepacks') }, { id: 'shaderpacks', label: t('xmcl.header.shaders') }, { id: 'save', label: t('xmcl.header.worlds') }];
const overcount = ref(0);
function drop(e: DragEvent) { overcount.value = 0; emit('drop', e.dataTransfer?.files || null); }
</script>
<style scoped>
.header{position:sticky;top:30px;z-index:20;max-width:100%;padding:2.5rem .5rem 0;user-select:none}.header::before{content:'';position:absolute;inset:0;bottom:-70px;z-index:-1;pointer-events:none;background-image:linear-gradient(var(--app-bar-color,rgba(16,18,28,.92)),transparent);backdrop-filter:blur(var(--app-bar-blur,18px));-webkit-mask-image:linear-gradient(#000 40%,transparent);mask-image:linear-gradient(#000 40%,transparent)}.header.compact{padding-top:.2rem;padding-bottom:.5rem}.header-content{display:flex;flex-direction:column;max-width:1200px;margin:auto}.header-primary-row{display:flex;align-items:baseline;min-height:60px;gap:4px;padding:0 1.5rem}.home-title{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:2.425rem;letter-spacing:-.04em;font-weight:500}.home-title small{font-size:.55em;color:rgba(255,255,255,.58)}.header-spacer{flex:1}.header-action,.header-launch{display:inline-flex;align-items:center;justify-content:center;border:0;border-radius:10px;color:rgba(255,255,255,.78);background:transparent;cursor:pointer}.header-action{width:42px;height:42px}.header-action:hover{background:rgba(255,255,255,.12);color:#fff}.header-launch{height:42px;padding:0 16px;gap:7px;background:#4caf50;color:#fff;font-weight:600;margin-left:7px}.header-launch:hover{background:#66bb6a}.header-launch:disabled{opacity:.45;cursor:not-allowed}.header-extension{display:flex;align-items:center;gap:4px;padding:0 1rem;margin-top:1rem;border-bottom:1px solid rgba(255,255,255,.1)}.header-tab{border:0;border-bottom:2px solid transparent;background:transparent;color:rgba(255,255,255,.6);padding:12px 16px;cursor:pointer;font:inherit}.header-tab:hover,.header-tab.active{color:#fff}.header-tab.active{border-bottom-color:#4caf50}.drop-zone{position:absolute;inset:0;top:0;padding:20px;display:flex}.drop-zone>div{width:100%;height:100%;display:flex;align-items:center;justify-content:center;gap:10px;border:2px dashed #fbc02d;border-radius:16px;background:rgba(251,192,45,.15);transition:transform .2s}.drop-zone>div.hot{transform:scale(1.0125);background:rgba(251,192,45,.28)}@media(max-width:900px){.header{padding-left:.25rem!important;padding-right:.25rem!important}.header-primary-row{padding-left:.75rem!important}.home-title{font-size:1.8rem}}
.material-icons{font-size:22px;line-height:1}
</style>
