<template><div class="melody-progress" role="progressbar" :aria-label="label" :aria-valuenow="value" aria-valuemin="0" aria-valuemax="100"><span :style="{ width: `${value}%` }" /></div></template>
<script setup lang="ts">
import { computed } from 'vue';
const props = defineProps<{ modelValue: number; label?: string }>();
const value = computed(() => Number.isFinite(props.modelValue) ? Math.max(0, Math.min(100, props.modelValue)) : 0);
</script>
