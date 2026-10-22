<template>
  <XmclSystemBar :connected="connected" :busy="busy" :page="page" :instance-name="instanceName" @refresh="$emit('refresh')" @tasks="$emit('tasks')" @search="$emit('search')" @navigate="$emit('navigate', $event)" />
  <XmclSidebar v-if="!isPclShell" :instances="instances" :selected="selected" :icons="icons" :page="page" @navigate="$emit('navigate', $event)" @select="$emit('select', $event)" @launch="$emit('launch', $event)" @back="$emit('back')" @add="$emit('add')" />
  <main class="xmcl-main"><slot /></main>
</template>
<script setup lang="ts">
import XmclSidebar from './XmclSidebar.vue';
import XmclSystemBar from './XmclSystemBar.vue';
defineProps<{ connected:boolean; busy:boolean; page:string; instanceName?:string; isPclShell:boolean; instances:any[]; selected:any; icons:Record<string,string> }>();
defineEmits<{ refresh:[]; tasks:[]; search:[]; navigate:[string]; select:[any]; launch:[any]; back:[]; add:[] }>();
</script>
