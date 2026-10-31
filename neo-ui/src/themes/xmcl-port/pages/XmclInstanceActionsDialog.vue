<template>
  <q-dialog v-model="moreShown">
    <q-card class="more-card text-white">
      <q-card-section><div class="text-h6">{{ t('xmcl.instance.actions') }}</div></q-card-section>
      <q-list>
        <q-item v-for="item in actions" :key="item.op" clickable v-close-popup :disable="busy" @click="action(item)"><q-item-section avatar><q-icon :name="item.icon" /></q-item-section><q-item-section>{{ item.label }}</q-item-section></q-item>
        <q-item clickable v-close-popup :disable="busy" @click="iconPickerShown = true"><q-item-section avatar><q-icon name="image" /></q-item-section><q-item-section>{{ t('xmcl.instance.changeIcon') }}</q-item-section></q-item>
        <q-item clickable v-close-popup :disable="busy" @click="emit('delete')"><q-item-section avatar><q-icon name="delete" color="negative" /></q-item-section><q-item-section class="text-negative">{{ t('xmcl.instance.delete') }}</q-item-section></q-item>
      </q-list>
    </q-card>
  </q-dialog>
  <PclIconPicker v-model="iconPickerShown" :selected-key="instance?.iconKey || instance?.icon || ''" :execute="execute" @select="emit('change-icon', $event)" @import="emit('change-icon')" />
</template>

<script setup lang="ts">
import { inject, ref } from 'vue';
import { XmclInstanceContextKey } from './XmclInstanceContext';
import PclIconPicker from '../../pcl/components/PclIconPicker.vue';
const context = inject(XmclInstanceContextKey);
if (!context) throw new Error('XmclInstanceContext is missing');
const { t, moreShown, actions, busy, action, instance, execute } = context;
const iconPickerShown = ref(false);
const emit = defineEmits<{ delete: []; 'change-icon': [iconKey?: string] }>();
</script>

