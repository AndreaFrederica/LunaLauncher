<template>
<section class="pcl-section-card pcl-export">
      <div class="pcl-export__head">
        <div>
          <span class="pcl-kicker">{{ t('ui.instanceExport') }}</span>
          <h2>{{ t('instanceSections.export') }}</h2>
          <p>{{ t('instanceSections.exportHint') }}</p>
        </div>
      </div>
      <div class="pcl-export__body">
        <PclHint v-if="exportError" theme="red" closable @close="exportError = ''">{{ exportError }}</PclHint>
        <div class="pcl-export__grid">
          <label class="pcl-export__field">
            <span>{{ t('instanceSections.packName') }}</span>
            <PclInput v-model="exportName" :placeholder="t('instanceSections.packNamePlaceholder')" />
          </label>
          <label class="pcl-export__field">
            <span>{{ t('instanceSections.packVersion') }}</span>
            <PclInput v-model="exportVersion" :placeholder="t('instanceSections.packVersionPlaceholder')" />
          </label>
          <label class="pcl-export__field">
            <span>{{ t('instanceSections.format') }}</span>
            <PclSelect v-model="exportFormat" :options="exportFormats" />
          </label>
          <label class="pcl-export__field">
            <span>{{ t('instanceSections.outputPath') }}</span>
            <div class="pcl-path-row">
              <PclInput v-model="exportOutput" :placeholder="exportOutputPlaceholder" />
              <PclIconButton icon="folder-open" :label="t('instanceSections.chooseOutput')" :size="28" :disabled="busy" @click="chooseExportOutput" />
            </div>
          </label>
          <label v-if="exportFormat === 'modrinth'" class="pcl-export__field pcl-export__field--wide">
            <span>{{ t('instanceSections.modrinthSummary') }}</span>
            <PclInput v-model="exportSummary" :placeholder="t('instanceSections.summaryPlaceholder')" />
          </label>
          <label v-if="exportFormat === 'curseforge'" class="pcl-export__field pcl-export__field--wide">
            <span>{{ t('instanceSections.curseforgeAuthor') }}</span>
            <PclInput v-model="exportAuthor" :placeholder="t('instanceSections.authorPlaceholder')" />
          </label>
        </div>

        <div class="pcl-export__section-title">{{ t('instanceSections.packageContent') }}</div>
        <div class="pcl-export__options">
          <div v-for="item in exportItems" :key="item.key" class="pcl-export__option" @click="toggleExport(item.key, !selectedExport.includes(item.key))">
            <PclCheckbox :model-value="selectedExport.includes(item.key)" @update:model-value="toggleExport(item.key, $event)" @click.stop />
            <span><strong>{{ item.label }}</strong><small>{{ item.description }}</small></span>
          </div>
        </div>

        <div class="pcl-export__checks">
          <PclCheckbox v-model="exportOptionalFiles" :label="t('instanceSections.markOptional')" />
          <PclCheckbox v-model="exportOverwrite" :label="t('instanceSections.overwrite')" />
        </div>

        <PclHint theme="blue">
          {{ t('instanceSections.formatHint') }}
        </PclHint>

        <div class="pcl-export__actions">
          <PclButton variant="highlight" icon="archive" :disabled="busy || exporting || !canExport" @click="startExport">
            {{ exporting ? t('instanceSections.exporting') : t('instanceSections.startExport') }}
          </PclButton>
        </div>
      </div>
    </section>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { PclInstanceSectionsContextKey } from './PclInstanceSectionsContext';
import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclHint from '../../components/PclHint.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclInput from '../../components/PclInput.vue'; import PclIconButton from '../../components/PclIconButton.vue'; import PclSelect from '../../components/PclSelect.vue';
const context = inject(PclInstanceSectionsContextKey); if (!context) throw new Error('PclInstanceSectionsContext is missing');
const { t, props, emit, busy, section, components, componentError, settings, instance, execute, expanded, versions, exportName, exportVersion, exportOutput, exportFormat, exportSummary, exportAuthor, exportOptionalFiles, exportOverwrite, exportError, exporting, selectedExport, exportFormats, exportOutputPlaceholder, canExport, serverLoading, serverError, eulaAgreed, serverPort, serverMaxPlayers, serverGamemode, serverWhitelist, serverCommand, consoleText, installItems, exportItems, instanceSettings, javaSettings, componentFor, settingValue, save, toggleExport, chooseExportOutput, chooseJavaPath, startExport, loadServer, saveEula, saveProperty, startServer, sendServerCommand } = context;
</script>
