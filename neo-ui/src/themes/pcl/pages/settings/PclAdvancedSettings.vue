<template>
  <div class="pcl-settings-section">
    <PclCard :title="t('pcl.auto2.k86e9bcfc62')">
      <div class="pcl-form-row pcl-form-row--top">
        <span class="pcl-form-row__label">{{ t('pcl.auto2.kb001801f13') }}</span>
        <div class="pcl-form-row__control pcl-form-row__control--column">
          <PclInput multiline :rows="4" :model-value="draftValue('JvmArgs', '')" :placeholder="t('pcl.auto2.k3496cb95b0')" @update:model-value="drafts.JvmArgs = $event" @blur="save('JvmArgs', drafts.JvmArgs)" />
        </div>
      </div>
      <div v-for="item in commandFields" :key="item.key" class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t(item.label) }}</span>
        <div class="pcl-form-row__control"><PclInput :model-value="draftValue(item.key, '')" :placeholder="t(item.placeholder)" @update:model-value="drafts[item.key] = $event" @blur="save(item.key, drafts[item.key])" /></div>
      </div>
      <div class="pcl-check-grid">
        <PclCheckbox v-for="item in checks" :key="item.key" :model-value="bool(item.key)" :label="t(item.label)" @update:model-value="save(item.key, $event)" />
      </div>
      <PclHint theme="blue" class="pcl-advanced-hint">{{ t('pcl.auto2.k49395925fa') }}</PclHint>
    </PclCard>
  </div>
</template>

<script setup lang="ts">
import PclCard from '../../components/PclCard.vue';
import PclInput from '../../components/PclInput.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclHint from '../../components/PclHint.vue';
type Translator = (key: string, params?: Record<string, unknown>) => string;
type Save = (key: string, value: unknown) => void | Promise<boolean>;
const props = defineProps<{ t: Translator; drafts: Record<string, any>; draftValue: (key: string, fallback?: string) => string; bool: (key: string) => boolean; save: Save }>();
const { t, drafts, draftValue, bool, save } = props;
const commandFields = [
  { key: 'PreLaunchCommand', label: 'pcl.auto2.k7e026da625', placeholder: 'pcl.auto2.k7296ad4897' },
  { key: 'WrapperCommand', label: 'pcl.auto2.k49a71399fa', placeholder: 'pcl.auto2.kc5c2ba325b' },
  { key: 'PostExitCommand', label: 'pcl.auto2.kfa4f7c58eb', placeholder: 'pcl.auto2.kc80ce0c4e5' },
];
const checks = [
  { key: 'UseNativeGLFW', label: 'pcl.auto2.kb6565ef62e' }, { key: 'UseNativeOpenAL', label: 'pcl.auto2.k7377d6632f' },
  { key: 'UseDiscreteGpu', label: 'pcl.auto2.k3ddf18f61b' }, { key: 'OnlineFixes', label: 'pcl.auto2.k29eb9b95d0' },
  { key: 'ShowGameTime', label: 'pcl.auto2.kff9e4486e9' }, { key: 'RecordGameTime', label: 'pcl.auto2.k19241e920e' },
];
</script>
