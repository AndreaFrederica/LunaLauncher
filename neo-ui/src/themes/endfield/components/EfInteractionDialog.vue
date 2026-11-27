<template>
  <q-dialog :model-value="shown" persistent>
    <q-card class="ef-dialog" :class="{ 'ef-dialog--profiles': choosingProfile }">
      <q-card-section>
        <h3 class="ef-dialog__title">{{ choosingProfile ? t('importProfiles.title') : confirming ? t('neo.auto2.k9deb52e20d') : interaction?.choices ? '选择' : '输入' }}</h3>
        <p v-if="!choosingProfile">{{ interaction?.prompt }}</p>
        <p v-if="interaction?.error" class="ef-dialog__error">{{ interaction.error }}</p>
      </q-card-section>
      <q-card-section v-if="choosingProfile" class="ef-dialog__profiles">
        <ImportProfileChoices v-model="answer" :choices="interaction?.choices || []" />
      </q-card-section>
      <q-card-section v-else-if="!confirming">
        <EfSelect v-if="interaction?.choices" v-model="answer" :options="choiceOptions" />
        <EfInput v-else v-model="answer" :type="interaction?.secret ? 'password' : 'text'" />
      </q-card-section>
      <q-card-actions align="right">
        <EfButton variant="ghost" @click="cancel">{{ choosingProfile && !hasValidProfile ? t('importProfiles.close') : t('neo.auto2.k4d0b4688c7') }}</EfButton>
        <EfButton v-if="!choosingProfile || hasValidProfile" variant="accent" :disabled="choosingProfile && !validProfileSelected"
          @click="emit('submit', confirming ? 1 : answer)">{{ choosingProfile ? t('importProfiles.confirm') : t(confirming ? 'neo.auto2.k1fc1afc5c5' : 'neo.auto2.kb56d9ac6c5') }}</EfButton>
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { isContinueConfirmation } from '../../../common/continueConfirmation';
import ImportProfileChoices from '../../../components/ImportProfileChoices.vue';
import EfButton from './EfButton.vue';
import EfInput from './EfInput.vue';
import EfSelect from './EfSelect.vue';

const props = defineProps<{ shown: boolean; request: any }>();
const emit = defineEmits<{ submit: [value: any]; cancel: [] }>();

const { t } = useI18n();
const answer = ref<string | number>('');
const interaction = computed(() => props.request?.request ?? props.request);
const confirming = computed(() => isContinueConfirmation(interaction.value?.choices));
const choosingProfile = computed(() => interaction.value?.choices?.[0]?.kind === 'import-profile');
const hasValidProfile = computed(() => interaction.value?.choices?.some((choice: any) => !choice.disabled));
const validProfileSelected = computed(() => {
  const choice = interaction.value?.choices?.[Number(answer.value)];
  return typeof answer.value === 'number' && choice && !choice.disabled;
});
const choiceOptions = computed(() => (interaction.value?.choices || []).map((x: any, n: number) => ({ label: typeof x === 'string' ? x : (x.name || x.label || JSON.stringify(x)), value: n })));

watch(() => interaction.value?.interactionId, () => { answer.value = ''; });

function cancel() {
  if (confirming.value) emit('submit', 0);
  else emit('cancel');
}
</script>

<style scoped>
.ef-dialog--profiles { width: 720px; max-width: 94vw; }
.ef-dialog__profiles { max-height: 58vh; overflow: auto; }
.ef-dialog__error { color: var(--ef-danger); font-size: 12px; }
</style>
