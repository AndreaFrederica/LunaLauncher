<template>
  <!-- Port of XMCL Store/StoreEntry/MarketProject visual hierarchy. -->
  <section class="xmcl-store"><header class="xmcl-store-header"><div><div class="eyebrow">XMCL STORE</div><h3>{{ t('xmcl.store.title') }}</h3><p>{{ t('xmcl.store.description') }}</p></div><q-btn flat round icon="refresh" :disable="busy" :aria-label="t('common.refresh')" @click="refresh"/></header><div class="xmcl-provider-cards"><button v-for="item in providers" :key="item.id" :class="{active:provider===item.id}" @click="provider=item.id"><q-icon :name="item.icon" size="26px"/><span>{{ item.name }}</span><small>{{ item.description }}</small></button></div><q-card class="xmcl-store-card"><CatalogBrowser :key="catalogKey" packs :default-provider="provider" :busy="busy" :execute="execute" @installed="emit('installed')"/></q-card></section>
</template>
<script setup lang="ts">
import CatalogBrowser from '../components/CatalogBrowser.vue';
import { ref } from 'vue';
import { useI18n } from 'vue-i18n';
defineProps<{busy:boolean;execute:(op:string,p?:Record<string,unknown>)=>Promise<any>}>();
const emit=defineEmits<{installed:[]}>();const { t } = useI18n();const provider=ref('modrinth'),catalogKey=ref(0);const providers=[{id:'modrinth',name:'Modrinth',icon:'public',description:t('xmcl.store.modrinth')},{id:'curseforge',name:'CurseForge',icon:'extension',description:t('xmcl.store.curseforge')},{id:'ftb',name:'Feed The Beast',icon:'inventory_2',description:t('xmcl.store.ftb')},{id:'atlauncher',name:'ATLauncher',icon:'rocket_launch',description:t('xmcl.store.atlauncher')}];
function refresh(){catalogKey.value++;}
</script>
<style scoped>.xmcl-store{max-width:1200px;margin:0 auto;padding:28px}.xmcl-store-header{display:flex;align-items:flex-end;justify-content:space-between}.xmcl-store-header h3{margin:4px 0;font-size:30px}.xmcl-store-header p{margin:0;color:rgba(255,255,255,.56)}.eyebrow{font-size:11px;letter-spacing:.14em;color:#a5b4fc;font-weight:700}.xmcl-provider-cards{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin:28px 0 18px}.xmcl-provider-cards button{display:flex;flex-direction:column;align-items:flex-start;gap:7px;padding:16px;border:1px solid rgba(255,255,255,.1);border-radius:14px;background:rgba(255,255,255,.05);color:#fff;text-align:left;cursor:pointer}.xmcl-provider-cards button:hover,.xmcl-provider-cards button.active{background:rgba(129,140,248,.2);border-color:rgba(129,140,248,.6)}.xmcl-provider-cards small{color:rgba(255,255,255,.55)}.xmcl-store-card{padding:14px;background:rgba(35,35,42,.76)!important;border:1px solid rgba(255,255,255,.1)}@media(max-width:800px){.xmcl-store{padding:16px 12px}.xmcl-provider-cards{grid-template-columns:repeat(2,1fr)}}
</style>
