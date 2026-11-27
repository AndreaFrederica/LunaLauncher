<template>
  <InstanceComponentsPanel v-if="props.section === 'install' && props.instance && props.execute"
    :instance="props.instance" :components="props.components" :component-error="props.componentError"
    :busy="props.busy" :execute="props.execute" allow-browse
    @refresh="emit('refresh')" @browse="emit('browse', $event)"
    @install-start="emit('install-start', $event)" @install-progress="emit('install-progress', $event)" @install-done="emit('install-done', $event)" />
</template>
<script setup lang="ts">
import { inject } from 'vue';
import { PclInstanceSectionsContextKey } from './PclInstanceSectionsContext';
import InstanceComponentsPanel from '../../../../components/InstanceComponentsPanel.vue';
const context = inject(PclInstanceSectionsContextKey);
if (!context) throw new Error('PclInstanceSectionsContext is missing');
const { props, emit } = context;
</script>
