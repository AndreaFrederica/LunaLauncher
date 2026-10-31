<template><slot /></template>
<script setup lang="ts">
import { provide, toRef } from 'vue';
import { melodyWindowKey } from '../services/window-context';
import type { MelodyWindowEntry } from '../types';
const props = defineProps<{ entry: MelodyWindowEntry; native: boolean }>();
const emit = defineEmits<{ activate: []; close: []; move: [x: number, y: number] }>();
provide(melodyWindowKey, { entry: toRef(props, 'entry'), native: props.native, activate: () => emit('activate'), close: () => emit('close'), move: (x, y) => emit('move', x, y) });
</script>
