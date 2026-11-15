<template>
<section class="pcl-section-card">
      <div v-if="props.settings.some((item: any) => item.key === 'VanillaGameDirectoryMode')" class="setting-line">
        <div><strong>原版目录模式</strong><small>新实例默认隔离。切换不会移动存档、模组或设置。</small></div>
        <PclSelect :model-value="String(settingValue('VanillaGameDirectoryMode') || 'auto')" :options="[{label:'自动识别已有设置',value:'auto'},{label:'隔离 · 版本目录',value:'isolated'},{label:'非隔离 · 共享 .minecraft',value:'shared'}]" :disabled="busy" @update:model-value="save('VanillaGameDirectoryMode', $event)" />
      </div>
      <div class="section-heading"><div><span class="pcl-kicker">{{ t('ui.instanceSettings') }}</span><h2>{{ t('instanceSections.settings') }}</h2><p>{{ t('instanceSections.settingsHint') }}</p></div></div>
      <div class="setting-grid"><div v-for="item in instanceSettings" :key="item.key" class="setting-line"><div><strong>{{ item.label }}</strong><small>{{ item.description }}</small></div><q-toggle v-if="item.type === 'boolean'" :model-value="settingValue(item.key)" :label="settingValue(item.key) ? t('instanceSections.enable') : t('instanceSections.disable')" :disable="busy" @update:model-value="save(item.key, $event)"/><q-input v-else outlined dense :model-value="settingValue(item.key)" :label="item.label" :disable="busy" @update:model-value="save(item.key, $event)"/></div></div>
      <q-banner class="pcl-hint"><span class="material-icons">tune</span>{{ t('instanceSections.settingsNotice') }}</q-banner>
    </section>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclInstanceSectionsContextKey } from './PclInstanceSectionsContext';
import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclHint from '../../components/PclHint.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclInput from '../../components/PclInput.vue'; import PclIconButton from '../../components/PclIconButton.vue'; import PclSelect from '../../components/PclSelect.vue';
const context = inject(PclInstanceSectionsContextKey); if (!context) throw new Error('PclInstanceSectionsContext is missing');
const { t, props, emit, busy, section, components, componentError, settings, instance, execute, expanded, versions, exportName, exportVersion, exportOutput, exportFormat, exportSummary, exportAuthor, exportOptionalFiles, exportOverwrite, exportError, exporting, selectedExport, exportFormats, exportOutputPlaceholder, canExport, serverLoading, serverError, eulaAgreed, serverPort, serverMaxPlayers, serverGamemode, serverWhitelist, serverCommand, consoleText, installItems, exportItems, instanceSettings, javaSettings, componentFor, settingValue, save, toggleExport, chooseExportOutput, chooseJavaPath, startExport, loadServer, saveEula, saveProperty, startServer, sendServerCommand } = context;
</script>
