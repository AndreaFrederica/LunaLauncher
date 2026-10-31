<template>
<section class="pcl-section-card pcl-install-section">
      <div class="pcl-install-head">
        <div>
          <span class="pcl-kicker">{{ t('ui.instanceInstallation') }}</span>
          <h2>{{ t('instanceSections.install') }}</h2>
          <p>{{ t('instanceSections.installHint') }}</p>
        </div>
        <PclButton size="sm" icon="refresh-cw" :disabled="busy" @click="emit('refresh')">{{ t('common.refresh') }}</PclButton>
      </div>
      <PclHint v-if="componentError" theme="red" :closable="false">{{ t('instanceSections.componentError', { error: componentError }) }}</PclHint>
      <div class="pcl-install-list">
        <article v-for="item in installItems" :key="item.id" class="pcl-install-item" :class="{ expanded: expanded === item.id }">
          <button class="pcl-install-item__head" type="button" @click="expanded = expanded === item.id ? '' : item.id">
            <PclIcon :name="item.icon" :size="18" />
            <span class="pcl-install-item__name">{{ item.label }}</span>
            <span class="pcl-install-item__version">{{ componentFor(item.uid)?.version || t('instanceSections.notInstalled') }}</span>
            <PclIcon :name="expanded === item.id ? 'chevron-up' : 'chevron-down'" :size="15" />
          </button>
          <div v-if="expanded === item.id" class="pcl-install-item__body">
            <p>{{ item.description }}</p>
            <div v-if="componentFor(item.uid)" class="pcl-install-current">
              <span>{{ t('instanceSections.currentVersion', { version: componentFor(item.uid)?.version || t('instanceSections.unknown') }) }}</span>
              <PclCheckbox
                :model-value="componentFor(item.uid)?.enabled !== false"
                :label="t('instanceSections.enable')"
                :disabled="busy || !componentFor(item.uid)?.canDisable"
                @update:model-value="emit('component-enabled', componentFor(item.uid), $event)"
              />
            </div>
            <div class="pcl-install-actions">
              <PclInput v-model="versions[item.id]" :placeholder="t('instanceSections.versionPlaceholder')" />
              <PclButton variant="highlight" :disabled="busy || !versions[item.id]?.trim() || (componentFor(item.uid) && componentFor(item.uid)?.canChangeVersion === false)" @click="emit('component-version', componentFor(item.uid) || { id: item.uid, name: item.label }, versions[item.id].trim())" >{{ t('instanceSections.applyVersion') }}</PclButton>
              <PclButton variant="ghost" icon="search" @click="emit('browse', item.uid)" >{{ t('instanceSections.browseVersions') }}</PclButton>
            </div>
          </div>
        </article>
      </div>
      <PclHint theme="blue">{{ t('instanceSections.installFooter') }}</PclHint>
    </section>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclInstanceSectionsContextKey } from './PclInstanceSectionsContext';
import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclHint from '../../components/PclHint.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclInput from '../../components/PclInput.vue'; import PclIconButton from '../../components/PclIconButton.vue'; import PclSelect from '../../components/PclSelect.vue';
const context = inject(PclInstanceSectionsContextKey); if (!context) throw new Error('PclInstanceSectionsContext is missing');
const { t, props, emit, busy, section, components, componentError, settings, instance, execute, expanded, versions, exportName, exportVersion, exportOutput, exportFormat, exportSummary, exportAuthor, exportOptionalFiles, exportOverwrite, exportError, exporting, selectedExport, exportFormats, exportOutputPlaceholder, canExport, serverLoading, serverError, eulaAgreed, serverPort, serverMaxPlayers, serverGamemode, serverWhitelist, serverCommand, consoleText, installItems, exportItems, instanceSettings, javaSettings, componentFor, settingValue, save, toggleExport, chooseExportOutput, chooseJavaPath, startExport, loadServer, saveEula, saveProperty, startServer, sendServerCommand } = context;
</script>
