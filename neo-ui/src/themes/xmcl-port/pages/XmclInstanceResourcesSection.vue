<template>
<template><div class="resource-switcher"><q-tabs v-model="resourceKind" align="left" outside-arrows><q-tab v-for="item in resourceKinds" :key="item.value" :name="item.value" :label="item.label" /></q-tabs></div><XmclResourceWorkspace :instance="instance.id" :kind="resourceKind" :minecraft-version="details?.minecraftVersion" :busy="busy" :execute="execute" /><ResourceUpdatesPanel :instance="instance.id" :kind="resourceKind" :busy="busy" :execute="execute" /><InstanceExtras :instance="instance.id" :busy="busy" :execute="execute" @changed="emit('refresh')" /></template>
</template>
<script setup lang="ts">
import { inject } from 'vue'; import { XmclInstanceContextKey } from './XmclInstanceContext';
import XmclHomeGrid from './XmclHomeGrid.vue'; import XmclResourceWorkspace from './XmclResourceWorkspace.vue'; import ResourceUpdatesPanel from '../../../components/ResourceUpdatesPanel.vue'; import InstanceExtras from '../../../components/InstanceExtras.vue'; import InstanceLogViewer from '../../../components/InstanceLogViewer.vue'; import LogUploadButton from '../../../components/LogUploadButton.vue';
const context = inject(XmclInstanceContextKey); if (!context) throw new Error('XmclInstanceContext is missing');
const { t, props, emit, instance, details, icon, fallback, components, componentError, notes, busy, execute, consoleLines, consoleState, logFiles, fileLog, initialSection, initialResourceKind, hideToolbar, section, resourceKind, localNotes, follow, moreShown, versionShown, editing, version, resourceKinds, actions, goResource, action, editComponent, saveVersion } = context;
</script>
