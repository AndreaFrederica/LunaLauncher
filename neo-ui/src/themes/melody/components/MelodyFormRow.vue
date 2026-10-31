<template><label class="melody-form-row"><span>{{ label }}</span><slot /><span v-if="suffix">{{ suffix }}</span></label></template>
<script setup lang="ts">defineProps<{ label: string; suffix?: string }>();</script>
