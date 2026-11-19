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
          <label v-if="exportFormat !== 'zip'" class="pcl-export__field">
            <span>{{ t('instanceSections.modExportMode') }}</span>
            <PclSelect v-model="exportModMode" :options="exportModModes" />
          </label>
          <label v-if="exportFormat === 'curseforge'" class="pcl-export__field">
            <span>{{ t('instanceSections.recommendedRAM') }}</span>
            <PclInput v-model="exportRecommendedRAM" type="number" min="0" max="1048576" :placeholder="t('instanceSections.recommendedRAMPlaceholder')" />
          </label>
        </div>
        <PclHint v-if="exportFormat !== 'zip'" theme="blue">{{ t('instanceSections.modModeHint') }}</PclHint>

        <div class="pcl-export__section-title">{{ t('instanceSections.packageContent') }}</div>
        <div class="pcl-export__options">
          <div v-for="item in visibleExportItems" :key="item.key" class="pcl-export__option" @click="toggleExport(item.key, !selectedExport.includes(item.key))">
            <PclCheckbox :model-value="selectedExport.includes(item.key)" @update:model-value="toggleExport(item.key, $event)" @click.stop />
            <span><strong>{{ item.label }}</strong><small>{{ item.description }}</small></span>
          </div>
        </div>

        <div class="pcl-export__section-title">{{ t('instanceSections.exportFiles') }}</div>
        <p>{{ t('instanceSections.exportFilesHint') }}</p>
        <PclHint v-if="fileError" theme="red">{{ fileError }}</PclHint>
        <div class="pcl-export__checks">
          <PclButton :disabled="fileLoading || busy" @click="exportFiles.load()">{{ t('instanceSections.reloadExportFiles') }}</PclButton>
        </div>
        <div v-if="fileLoading" role="status">{{ t('instanceSections.loadingExportFiles') }}</div>
        <div v-else class="pcl-export__files" :aria-label="t('instanceSections.exportFiles')">
          <div v-for="row in exportRows" :key="row.path" class="pcl-export__file" :style="{ paddingInlineStart: `${12 + row.depth * 22}px` }">
            <button v-if="row.isDir" type="button" class="pcl-export__expand" :aria-expanded="fileExpanded.includes(row.path)" :aria-label="row.path" :disabled="filePending.includes(row.path)" @click="exportFiles.expand(row)">
              <PclIcon :name="fileExpanded.includes(row.path) ? 'chevron-down' : 'chevron-right'" :size="16" />
            </button>
            <span v-else class="pcl-export__expand" />
            <label class="pcl-export__file-label" :title="row.path">
              <input type="checkbox" :checked="exportFiles.checked(row.path)" :indeterminate="exportFiles.partial(row.path)" :disabled="busy" @change="exportFiles.toggle(row.path, ($event.target as HTMLInputElement).checked)" />
              <PclIcon :name="row.isDir ? 'folder' : 'file'" :size="16" />
              <span>{{ row.path.includes('/') && row.depth === 0 ? row.path : row.name }}</span>
            </label>
          </div>
        </div>

        <div class="pcl-export__checks">
          <PclCheckbox v-if="exportFormat !== 'zip'" v-model="exportOptionalFiles" :label="t('instanceSections.markOptional')" />
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
import { computed, inject } from 'vue'; import { PclInstanceSectionsContextKey } from './PclInstanceSectionsContext';
import PclButton from '../../components/PclButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue'; import PclHint from '../../components/PclHint.vue'; import PclIcon from '../../components/PclIcon.vue'; import PclInput from '../../components/PclInput.vue'; import PclIconButton from '../../components/PclIconButton.vue'; import PclSelect from '../../components/PclSelect.vue';
const context = inject(PclInstanceSectionsContextKey); if (!context) throw new Error('PclInstanceSectionsContext is missing');
const { t, busy, exportName, exportVersion, exportOutput, exportFormat, exportSummary, exportAuthor, exportModMode, exportModModes, exportRecommendedRAM, exportFiles, exportOptionalFiles, exportOverwrite, exportError, exporting, selectedExport, exportFormats, exportOutputPlaceholder, canExport, exportItems, toggleExport, chooseExportOutput, startExport } = context;
const visibleExportItems = computed(() => exportFormat.value === 'zip' ? exportItems : exportItems.filter((item: { key: string }) => item.key !== 'game'));
const { rows: exportRows, loading: fileLoading, error: fileError, expanded: fileExpanded, pending: filePending } = exportFiles;
</script>
<style scoped>
.pcl-export__files { max-height: 360px; overflow: auto; border: 1px solid var(--pcl-border, #80808040); border-radius: 5px; margin-block: 12px; }
.pcl-export__file { display: flex; align-items: center; gap: 6px; min-height: 34px; padding-inline-end: 12px; }
.pcl-export__file:hover { background: #80808012; }
.pcl-export__expand { display: inline-flex; align-items: center; justify-content: center; width: 24px; height: 28px; padding: 0; flex-shrink: 0; background: transparent; border: 0; color: inherit; }
button.pcl-export__expand { cursor: pointer; }
.pcl-export__file-label { display: flex; align-items: center; gap: 8px; cursor: pointer; min-width: 0; }
.pcl-export__file-label input { accent-color: var(--pcl-accent, #d65599); }
.pcl-export__file-label span { overflow-wrap: anywhere; }
</style>
