<template>
  <!-- Port of XMCL Setting.vue's navigation-sidebar/content split. -->
  <div class="xmcl-setting-page">
    <nav class="xmcl-setting-sidebar" aria-label="设置导航"><div class="xmcl-setting-card"><div class="xmcl-setting-label">{{ isInstance ? '客户端设置' : '启动器设置' }}</div><button v-for="item in sections" :key="item.id" :class="{active:section===item.id}" @click="section=item.id"><q-icon :name="item.icon" size="18px"/><span>{{ item.label }}</span><small v-if="item.count !== undefined">{{ item.count }}</small></button></div></nav>
    <main class="xmcl-setting-content">
      <section v-if="section==='all'" class="xmcl-setting-section"><header><q-icon name="tune"/><div><h5>{{ isInstance ? '客户端设置' : '启动器设置' }}</h5><p>设置项由 Launcher API 返回，按功能和 UI 作用范围分类。</p></div></header><SettingsPanel v-model:category="category" :settings="settings" :scope="isInstance ? 'instance' : 'launcher'" :busy="busy" @save="(key,value)=>execute('settings.set',{...scope,key,value})" @reset="key=>execute('settings.reset',{...scope,key})"/></section>
      <section v-else-if="isCategorySection" class="xmcl-setting-section"><header><q-icon :name="categoryIcon"/><div><h5>{{ categoryLabel }}</h5><p>{{ categoryDescription }}</p></div></header><SettingsPanel v-model:category="category" :settings="settings" :scope="isInstance ? 'instance' : 'launcher'" :busy="busy" @save="(key,value)=>execute('settings.set',{...scope,key,value})" @reset="key=>execute('settings.reset',{...scope,key})"/></section>
      <section v-else-if="section==='runtime'" class="xmcl-setting-section"><header><q-icon name="code"/><div><h5>Java 与运行环境</h5><p>检测 Java、诊断兼容性并选择运行时。</p></div></header><JavaPanel :scope="scope" :settings="settings" :busy="busy" :execute="execute" @changed="emit('changed')"/></section>
      <section v-else-if="section==='tools'" class="xmcl-setting-section"><header><q-icon name="tune"/><div><h5>外观、网络与更新</h5><p>这些能力通过 API 操作接口管理，不混入设置值列表。</p></div></header><LauncherControlPanel :busy="busy" :execute="execute"/></section>
      <section v-else class="xmcl-setting-section"><header><q-icon name="info"/><div><h5>关于与维护</h5><p>导入设置、清理缓存和启动器新闻。</p></div></header><LauncherControlPanel :busy="busy" :execute="execute"/></section>
    </main>
  </div>
</template>
<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import JavaPanel from '../components/JavaPanel.vue';
import SettingsPanel from '../components/SettingsPanel.vue';
import LauncherControlPanel from '../components/LauncherControlPanel.vue';
import { settingCategoryIcons, settingCategoryLabels, settingCategoryOrder } from '../settings-categories';
import { settingMetadata } from '../settings-schema';
const props=defineProps<{settings:any[];busy:boolean;scope:Record<string,unknown>;execute:(op:string,p?:Record<string,unknown>)=>Promise<any>}>();
const emit=defineEmits<{changed:[]}>();
const section=ref('all');
const category=ref('all');
const isInstance=computed(()=>props.scope.scope==='instance');
const legacyGroups: Record<string, string> = { '窗口与启动': 'launch', '游戏与启动': 'launch', 'Java 与内存': 'java', '命令与环境': 'launch', '目录与文件': 'paths', '网络': 'network', Proxy: 'network', '下载与资源': 'downloads', '性能与兼容': 'compatibility', 控制台: 'console', '游戏时间': 'playtime', Launcher: 'qt-ui', ExternalTools: 'advanced', API: 'advanced' };
function settingCategory(setting: any) {
  if (setting.surface === 'qt') return 'qt-ui';
  const metadata = settingMetadata[setting.key];
  return setting.category || legacyGroups[metadata?.group || ''] || metadata?.group || 'advanced';
}
const presentCategories=computed(()=>new Set(props.settings.map(settingCategory)));
const categorySections=computed(()=>settingCategoryOrder.filter(id=>presentCategories.value.has(id)).map(id=>({id,label:settingCategoryLabels[id],icon:settingCategoryIcons[id],count:props.settings.filter(setting=>settingCategory(setting)===id).length})));
const sections=computed(()=>[
  {id:'all',label:isInstance.value ? '全部客户端设置' : '全部启动器设置',icon:'tune',count:props.settings.length},
  ...categorySections.value,
  {id:'runtime',label:'Java 与运行环境',icon:'code',count:0},
  ...(!isInstance.value ? [{id:'tools',label:'外观、网络与更新',icon:'palette',count:0},{id:'about',label:'关于与维护',icon:'info',count:0}] : []),
]);
const isCategorySection=computed(()=>section.value !== 'all' && section.value !== 'runtime' && section.value !== 'tools' && section.value !== 'about');
const categoryLabel=computed(()=>settingCategoryLabels[section.value] || section.value);
const categoryIcon=computed(()=>settingCategoryIcons[section.value] || 'tune');
const categoryDescription=computed(()=>section.value === 'qt-ui' ? '这些选项只影响 Qt 旧界面的布局、工具栏或外部集成。' : '按 API 分类显示设置项，修改后立即写回当前配置对象。');
watch(isInstance, () => { section.value = 'all'; category.value = 'all'; });
watch(presentCategories, () => { if (isCategorySection.value && !presentCategories.value.has(section.value)) section.value = 'all'; });
watch(section, value => { if (isCategorySection.value) category.value = value; else if (value === 'all') category.value = 'all'; });
watch(category, value => { if (value !== 'all' && presentCategories.value.has(value)) section.value = value; else if (value === 'all' && isCategorySection.value) section.value = 'all'; });
</script>
<style scoped>
.xmcl-setting-page{display:flex;min-height:680px;max-width:1250px;margin:0 auto}.xmcl-setting-sidebar{width:250px;min-width:250px;padding:24px 12px 24px 20px}.xmcl-setting-card{position:sticky;top:24px;padding:12px;border-radius:16px;background:rgba(35,35,42,.76);border:1px solid rgba(255,255,255,.1);backdrop-filter:blur(20px);max-height:calc(100vh - 48px);overflow-y:auto}.xmcl-setting-label{padding:8px 12px 12px;color:rgba(255,255,255,.55);font-size:12px;text-transform:uppercase;font-weight:700;letter-spacing:.08em}.xmcl-setting-card button{width:100%;display:flex;align-items:center;gap:10px;border:0;background:transparent;color:rgba(255,255,255,.72);padding:10px 12px;border-radius:10px;text-align:left;cursor:pointer;margin:2px 0}.xmcl-setting-card button span{flex:1;min-width:0}.xmcl-setting-card button small{color:rgba(255,255,255,.4);font-size:11px}.xmcl-setting-card button:hover{background:rgba(255,255,255,.08);color:#fff}.xmcl-setting-card button.active{background:rgba(129,140,248,.22);color:#a5b4fc}.xmcl-setting-content{flex:1;min-width:0;padding:24px 28px;overflow-y:auto}.xmcl-setting-section>header{display:flex;align-items:flex-start;gap:12px;margin:0 0 22px}.xmcl-setting-section>header>.q-icon{margin-top:4px;color:#a5b4fc}.xmcl-setting-section h5{margin:0 0 4px;font-size:24px}.xmcl-setting-section p{margin:0;color:rgba(255,255,255,.55)}@media(max-width:800px){.xmcl-setting-page{display:block}.xmcl-setting-sidebar{width:auto;min-width:0;padding:8px 12px}.xmcl-setting-card{position:static;display:flex;overflow:auto;gap:4px;max-height:none}.xmcl-setting-label{display:none}.xmcl-setting-card button{width:auto;white-space:nowrap}.xmcl-setting-content{padding:18px 12px}}
</style>
