<template>
  <!-- Port of XMCL Setting.vue's navigation-sidebar/content split. -->
  <div class="xmcl-setting-page">
    <nav v-if="!hideNavigation" class="xmcl-setting-sidebar" :aria-label="t('xmcl.settings.navigation')"><div class="xmcl-setting-card"><div class="xmcl-setting-label">{{ isInstance ? t('xmcl.settings.instance') : t('xmcl.settings.launcher') }}</div><button v-for="item in sections" :key="item.id" :class="{active:section===item.id}" @click="section=item.id"><q-icon :name="item.icon" size="18px"/><span>{{ item.label }}</span><small v-if="item.count !== undefined">{{ item.count }}</small></button></div></nav>
    <main class="xmcl-setting-content">
      <section v-if="section==='all'" class="xmcl-setting-section"><header><q-icon name="tune"/><div><h5>{{ isInstance ? t('xmcl.settings.instance') : t('xmcl.settings.launcher') }}</h5><p>{{ t('xmcl.settings.allHint') }}</p></div></header><InstanceSettingsPanel v-if="isInstance" :settings="settings" :busy="busy" :scope="scope" :execute="execute" @changed="emit('changed')"/><LauncherSettingsPanel v-else :settings="settings" :busy="busy" :scope="scope" :execute="execute" @changed="emit('changed')"/></section>
      <section v-else-if="isCategorySection" class="xmcl-setting-section"><header><q-icon :name="categoryIcon"/><div><h5>{{ categoryLabel }}</h5><p>{{ categoryDescription }}</p></div></header><SettingsPanel v-model:category="category" :settings="settings" :scope="isInstance ? 'instance' : 'launcher'" :busy="busy" @save="(key,value)=>execute('settings.set',{...scope,key,value})" @reset="key=>execute('settings.reset',{...scope,key})"/></section>
      <section v-else-if="section==='runtime'" class="xmcl-setting-section"><header><q-icon name="code"/><div><h5>{{ t('xmcl.settings.runtime') }}</h5><p>{{ t('xmcl.settings.runtimeHint') }}</p></div></header><JavaPanel :scope="scope" :settings="settings" :busy="busy" :execute="execute" @changed="emit('changed')"/></section>
      <section v-else-if="section==='tools'" class="xmcl-setting-section"><header><q-icon name="palette"/><div><h5>{{ t('xmcl.settings.appearance') }}</h5><p>{{ t('xmcl.settings.appearanceHint') }}</p></div></header><NeoUiAppearancePanel /></section>
      <section v-else-if="section==='qt'" class="xmcl-setting-section"><header><q-icon name="desktop_windows"/><div><h5>{{ t('xmcl.settings.qt') }}</h5><p>{{ t('xmcl.settings.qtHint') }}</p></div></header><LauncherControlPanel :busy="busy" :execute="execute"/><SettingsPanel class="q-mt-xl" :settings="settings" category="qt-ui" :scope="isInstance ? 'instance' : 'launcher'" :busy="busy" @save="(key,value)=>execute('settings.set',{...scope,key,value})" @reset="key=>execute('settings.reset',{...scope,key})"/></section>
      <section v-else-if="section==='about'" class="xmcl-setting-section"><header><q-icon name="info"/><div><h5>{{ t('xmcl.settings.about') }}</h5><p>{{ t('xmcl.settings.aboutHint') }}</p></div></header><NeoUiAboutPanel :execute="execute"/></section>
    </main>
  </div>
</template>
<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import JavaPanel from '../components/JavaPanel.vue';
import InstanceSettingsPanel from '../components/InstanceSettingsPanel.vue';
import LauncherSettingsPanel from '../components/LauncherSettingsPanel.vue';
import SettingsPanel from '../components/SettingsPanel.vue';
import LauncherControlPanel from '../components/LauncherControlPanel.vue';
import NeoUiAppearancePanel from '../components/NeoUiAppearancePanel.vue';
import NeoUiAboutPanel from '../components/NeoUiAboutPanel.vue';
import { settingCategoryIcons, settingCategoryOrder } from '../settings-categories';
import { settingMetadata } from '../settings-schema';
const props=defineProps<{settings:any[];busy:boolean;scope:Record<string,unknown>;execute:(op:string,p?:Record<string,unknown>)=>Promise<any>;hideNavigation?:boolean;initialSection?:string}>();
const { t } = useI18n();
const emit=defineEmits<{changed:[]}>();
const section=ref(props.initialSection || 'all');
const category=ref('all');
const isInstance=computed(()=>props.scope.scope==='instance');
const legacyGroups: Record<string, string> = { '\u7a97\u53e3\u4e0e\u542f\u52a8': 'launch', '\u6e38\u620f\u4e0e\u542f\u52a8': 'launch', 'Java \u4e0e\u5185\u5b58': 'java', '\u547d\u4ee4\u4e0e\u73af\u5883': 'launch', '\u76ee\u5f55\u4e0e\u6587\u4ef6': 'paths', '\u7f51\u7edc': 'network', Proxy: 'network', '\u4e0b\u8f7d\u4e0e\u8d44\u6e90': 'downloads', '\u6027\u80fd\u4e0e\u517c\u5bb9': 'compatibility', '\u63a7\u5236\u53f0': 'console', '\u6e38\u620f\u65f6\u95f4': 'playtime', Launcher: 'qt-ui', ExternalTools: 'advanced', API: 'advanced' };
function settingCategory(setting: any) {
  if (setting.surface === 'qt') return 'qt-ui';
  const metadata = settingMetadata[setting.key];
  return setting.category || legacyGroups[metadata?.group || ''] || metadata?.group || 'advanced';
}
const presentCategories=computed(()=>new Set(props.settings.map(settingCategory)));
// Qt compatibility settings have their own dedicated page so they are not
// mixed into the Neo UI appearance navigation.
const categorySections=computed(()=>settingCategoryOrder.filter(id=>id !== 'qt-ui' && presentCategories.value.has(id)).map(id=>({id,label:t(`settings.categories.${id}`),icon:settingCategoryIcons[id],count:props.settings.filter(setting=>settingCategory(setting)===id).length})));
const sections=computed(()=>[
  {id:'all',label:isInstance.value ? t('xmcl.settings.allInstance') : t('xmcl.settings.allLauncher'),icon:'tune',count:props.settings.length},
  ...categorySections.value,
  {id:'runtime',label:t('xmcl.settings.runtime'),icon:'code',count:0},
  ...(!isInstance.value ? [{id:'tools',label:t('xmcl.settings.appearance'),icon:'palette',count:0},{id:'qt',label:t('xmcl.settings.qt'),icon:'desktop_windows',count:0},{id:'about',label:t('xmcl.settings.about'),icon:'info',count:0}] : []),
]);
const isCategorySection=computed(()=>section.value !== 'all' && section.value !== 'runtime' && section.value !== 'tools' && section.value !== 'qt' && section.value !== 'about');
const categoryLabel=computed(()=>t(`settings.categories.${section.value}`));
const categoryIcon=computed(()=>settingCategoryIcons[section.value] || 'tune');
const categoryDescription=computed(()=>section.value === 'qt-ui' ? t('xmcl.settings.qtCategoryHint') : t('xmcl.settings.categoryHint'));
watch(isInstance, () => { section.value = 'all'; category.value = 'all'; });
watch(() => props.initialSection, value => { if (value) section.value = value; });
watch(presentCategories, () => { if (isCategorySection.value && !presentCategories.value.has(section.value)) section.value = 'all'; });
watch(section, value => { if (isCategorySection.value) category.value = value; else if (value === 'all') category.value = 'all'; });
watch(category, value => { if (value !== 'all' && presentCategories.value.has(value)) section.value = value; else if (value === 'all' && isCategorySection.value) section.value = 'all'; });
</script>
<style scoped>
.xmcl-setting-page{display:flex;min-height:680px;max-width:1250px;margin:0 auto}.xmcl-setting-sidebar{width:250px;min-width:250px;padding:24px 12px 24px 20px}.xmcl-setting-card{position:sticky;top:24px;padding:12px;border-radius:16px;background:rgba(35,35,42,.76);border:1px solid rgba(255,255,255,.1);backdrop-filter:blur(20px);max-height:calc(100vh - 48px);overflow-y:auto}.xmcl-setting-label{padding:8px 12px 12px;color:rgba(255,255,255,.55);font-size:12px;text-transform:uppercase;font-weight:700;letter-spacing:.08em}.xmcl-setting-card button{width:100%;display:flex;align-items:center;gap:10px;border:0;background:transparent;color:rgba(255,255,255,.72);padding:10px 12px;border-radius:10px;text-align:left;cursor:pointer;margin:2px 0}.xmcl-setting-card button span{flex:1;min-width:0}.xmcl-setting-card button small{color:rgba(255,255,255,.4);font-size:11px}.xmcl-setting-card button:hover{background:rgba(255,255,255,.08);color:#fff}.xmcl-setting-card button.active{background:rgba(129,140,248,.22);color:#a5b4fc}.xmcl-setting-content{flex:1;min-width:0;padding:24px 28px;overflow-y:auto}.xmcl-setting-section>header{display:flex;align-items:flex-start;gap:12px;margin:0 0 22px}.xmcl-setting-section>header>.q-icon{margin-top:4px;color:#a5b4fc}.xmcl-setting-section h5{margin:0 0 4px;font-size:24px}.xmcl-setting-section p{margin:0;color:rgba(255,255,255,.55)}@media(max-width:800px){.xmcl-setting-page{display:block}.xmcl-setting-sidebar{width:auto;min-width:0;padding:8px 12px}.xmcl-setting-card{position:static;display:flex;overflow:auto;gap:4px;max-height:none}.xmcl-setting-label{display:none}.xmcl-setting-card button{width:auto;white-space:nowrap}.xmcl-setting-content{padding:18px 12px}}
</style>
