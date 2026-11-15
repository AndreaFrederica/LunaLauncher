<template>
  <q-dialog :model-value="shown" persistent>
    <q-card class="ef-dialog">
      <q-card-section>
        <h3 class="ef-dialog__title">{{ request?.request?.choices ? '选择' : '输入' }}</h3>
        <p>{{ request?.request?.prompt }}</p>
        <p v-if="request?.request?.error" class="ef-dialog__error">{{ request.request.error }}</p>
      </q-card-section>
      <q-card-section>
        <EfSelect v-if="request?.request?.choices" v-model="answer" :options="choiceOptions" />
        <EfInput v-else v-model="answer" :type="request?.request?.secret ? 'password' : 'text'" />
      </q-card-section>
      <q-card-actions align="right">
        <EfButton variant="ghost" @click="emit('cancel')">取消</EfButton>
        <EfButton variant="accent" @click="emit('submit', answer)">确定</EfButton>
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import EfButton from './EfButton.vue';
import EfInput from './EfInput.vue';
import EfSelect from './EfSelect.vue';

const props = defineProps<{ shown: boolean; request: any }>();
const emit = defineEmits<{ submit: [value: any]; cancel: [] }>();

const answer = ref('');
const choiceOptions = computed(() => (props.request?.request?.choices || []).map((x: any, n: number) => ({ label: typeof x === 'string' ? x : (x.name || x.label || JSON.stringify(x)), value: n })));

watch(() => props.shown, (v) => { if (v) answer.value = ''; });
</script>

<style scoped>
.ef-dialog__error { color: var(--ef-danger); font-size: 12px; }
</style>
