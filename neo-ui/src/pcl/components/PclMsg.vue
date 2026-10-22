<template>
  <q-dialog :model-value="modelValue" persistent @update:model-value="value => !value && emit('finish', false)">
    <q-card class="pcl-dialog pcl-msg">
      <q-card-section>
        <div class="pcl-kicker">提示</div>
        <h3>{{ title || '请确认' }}</h3>
        <p v-if="description">{{ description }}</p>
      </q-card-section>
      <q-card-section v-if="choices" class="pcl-msg__field">
        <PclSelect :model-value="answer" :options="choices" placeholder="请选择" @update:model-value="emit('update:answer', $event)" />
      </q-card-section>
      <q-card-section v-else-if="!confirmation" class="pcl-msg__field">
        <PclInput
          :model-value="answer"
          :type="secret ? 'password' : 'text'"
          :multiline="!secret"
          :rows="3"
          placeholder="请输入"
          @update:model-value="emit('update:answer', $event)"
        />
      </q-card-section>
      <q-card-actions align="right">
        <PclButton variant="ghost" @click="emit('finish', false)">取消</PclButton>
        <PclButton variant="highlight" @click="emit('finish', true)">确认</PclButton>
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import PclButton from './PclButton.vue';
import PclInput from './PclInput.vue';
import PclSelect from './PclSelect.vue';

withDefaults(defineProps<{
  modelValue: boolean;
  title?: string;
  description?: string;
  answer?: any;
  choices?: any[] | null;
  confirmation?: boolean;
  secret?: boolean;
}>(), {
  title: '',
  description: '',
  answer: '',
  choices: null,
  confirmation: false,
  secret: false,
});

const emit = defineEmits<{
  'update:modelValue': [value: boolean];
  'update:answer': [value: any];
  finish: [confirmed: boolean];
}>();
</script>

<style scoped>
.pcl-msg {
  width: 460px;
  max-width: 92vw;
}

.pcl-msg h3 {
  margin: 4px 0 0;
}

.pcl-msg p {
  margin: 7px 0 0;
  color: var(--pcl-muted);
  font-size: 12px;
  line-height: 1.7;
}

.pcl-msg__field {
  padding-top: 0;
}
</style>
