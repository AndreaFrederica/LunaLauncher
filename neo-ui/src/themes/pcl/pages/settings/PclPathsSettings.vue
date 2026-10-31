<template>
  <div class="pcl-settings-section">
    <PclCard :title="t('pcl.auto2.k0e567795b0')">
      <div v-for="item in pathSettings" :key="item.key" class="pcl-form-row"><span class="pcl-form-row__label">{{ item.label }}</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue(item.key, '')" :placeholder="item.placeholder || t('pcl.setup.pathPlaceholder')" @update:model-value="drafts[item.key] = $event" @blur="save(item.key, drafts[item.key])" /><PclIconButton icon="folder-open" :label="t('pcl.setup.chooseNamed', { name: item.label })" :size="28" :disabled="busy" @click="chooseDirectory(item.key, item.label)" /></div></div>
    </PclCard>
    <PclCard :title="t('pcl.auto2.k0c3031d3cd')"><div class="pcl-check-grid"><PclCheckbox :model-value="bool('DownloadsDirWatchRecursive')" :label="t('pcl.auto2.k74948c07d1')" @update:model-value="save('DownloadsDirWatchRecursive', $event)" /><PclCheckbox :model-value="bool('MoveModsFromDownloadsDir')" :label="t('pcl.auto2.k375d153246')" @update:model-value="save('MoveModsFromDownloadsDir', $event)" /></div></PclCard>
    <PclCard :title="t('pcl.auto2.k6e09f08f81')"><PclCheckbox :model-value="bool('CurseForgeExternalToolEnabled')" :label="t('pcl.auto2.kba5e83b7b2')" @update:model-value="save('CurseForgeExternalToolEnabled', $event)" /><div v-for="item in toolPathSettings" :key="item.key" class="pcl-form-row"><span class="pcl-form-row__label">{{ item.label }}</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue(item.key, '')" :placeholder="t('pcl.auto2.k53e32830a5')" @update:model-value="drafts[item.key] = $event" @blur="save(item.key, drafts[item.key])" /><PclIconButton icon="file" :label="t('pcl.setup.chooseNamed', { name: item.label })" :size="28" :disabled="busy" @click="chooseFilePath(item.key, item.label)" /></div></div></PclCard>
  </div>
</template>
<script setup lang="ts">
import PclCard from '../../components/PclCard.vue'; import PclInput from '../../components/PclInput.vue'; import PclIconButton from '../../components/PclIconButton.vue'; import PclCheckbox from '../../components/PclCheckbox.vue';
type Translator = (key: string, params?: Record<string, unknown>) => string; type Save = (key: string, value: unknown) => void | Promise<boolean>;
type PathItem = { key: string; label: string; placeholder?: string };
const props = defineProps<{ t: Translator; drafts: Record<string, any>; draftValue: (key: string, fallback?: string) => string; bool: (key: string) => boolean; save: Save; busy: boolean; pathSettings: readonly PathItem[]; toolPathSettings: readonly PathItem[]; chooseDirectory: (key: string, label: string) => void | Promise<void>; chooseFilePath: (key: string, label: string) => void | Promise<void> }>();
const { t, drafts, draftValue, bool, save, busy, pathSettings, toolPathSettings, chooseDirectory, chooseFilePath } = props;
</script>

