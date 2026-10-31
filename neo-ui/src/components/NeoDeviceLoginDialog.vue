<template>
  <q-dialog :model-value="modelValue" persistent @update:model-value="emit('update:modelValue', $event)">
    <q-card :class="isPclShell ? 'pcl-dialog pcl-device-dialog' : 'device-card'">
      <q-card-section>
        <div class="pcl-kicker">{{ t('app.microsoftLogin') }}</div>
        <h3>{{ t('app.browserLogin') }}</h3>
        <p class="pcl-muted">{{ t('app.deviceHint') }}</p>
      </q-card-section>
      <q-card-section v-if="device?.code" class="pcl-device-code">{{ device.code }}</q-card-section>
      <q-card-section>
        <code class="pcl-device-url">{{ device?.url || t('app.loadingLoginUrl') }}</code>
        <p v-if="device?.expiresIn" class="pcl-muted">{{ t('app.codeExpires', { minutes: Math.ceil(Number(device.expiresIn) / 60) }) }}</p>
      </q-card-section>
      <q-card-actions align="right">
        <PclButton icon="external-link" :disabled="!device?.url" @click="emit('open-browser')">{{ t('app.openBrowser') }}</PclButton>
        <PclButton icon="copy" :disabled="!device?.code" @click="emit('copy-code')">{{ t('app.copyCode') }}</PclButton>
        <PclButton variant="ghost" @click="emit('cancel')">{{ t('app.closeCancel') }}</PclButton>
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { useI18n } from 'vue-i18n';
import PclButton from '../themes/pcl/components/PclButton.vue';
defineProps<{ modelValue: boolean; isPclShell: boolean; device: any }>();
const emit = defineEmits(['update:modelValue', 'open-browser', 'copy-code', 'cancel']);
const { t } = useI18n();
</script>
