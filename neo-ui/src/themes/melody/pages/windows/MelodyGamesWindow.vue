<template>
  <MelodyModernWindow kind="games" @close="$emit('close')">
    <div class="melody-modern-column">
      <div class="melody-toolbar melody-toolbar--header">
        <label>资源安装到</label>
        <MelodySelect v-model="selected" :options="options" aria-label="资源安装到" />
        <MelodyButton @click="$emit('clients')">客户端管理</MelodyButton>
      </div>
      <p v-if="error" class="melody-error" role="alert">{{ error }}</p>
      <PclDownloadPage :busy="loading" :execute="melodyExecute" :selected="target" :instances="instances" :initial-component-uid="componentUid?.startsWith('resource:') ? undefined : componentUid" :initial-section="resourceSection" ignore-feature-visibility @refresh="refresh" @launch="$emit('clients')" @installed="installed" @install-done="installed" />
      <MelodyTaskPanel />
    </div>
  </MelodyModernWindow>
</template>
<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { storeToRefs } from 'pinia';
import PclDownloadPage from '../../../pcl/pages/download/PclDownloadPage.vue';
import MelodyModernWindow from '../../components/MelodyModernWindow.vue';
import MelodyButton from '../../components/MelodyButton.vue';
import MelodySelect from '../../components/MelodySelect.vue';
import MelodyTaskPanel from '../../components/MelodyTaskPanel.vue';
import { melodyExecute, notifyMelodyInstancesChanged } from '../../services/backend';
import { useLauncherSessionStore } from '../../../../common/launcher-session';
const props = defineProps<{ instanceId?: string; componentUid?: string }>();
defineEmits<{ close: []; clients: [] }>();
const session = useLauncherSessionStore();
const { instances, loading, error, selectedInstanceId } = storeToRefs(session);
const refresh = session.refresh;
const resourceSection = computed(() => props.componentUid?.startsWith('resource:') ? props.componentUid.slice(9).replace('shaderpacks', 'shaders') : undefined);
const selected = computed({ get: () => props.instanceId || selectedInstanceId.value, set: value => session.selectInstance(value) });
const options = computed(() => [{ value: '', label: '请选择客户端' }, ...instances.value.map(item => ({ value: item.id, label: item.name }))]);
const target = computed(() => instances.value.find(item => item.id === selected.value));
watch(instances, values => { if (!values.some(item => item.id === selected.value)) session.selectInstance(''); });
function installed() { notifyMelodyInstancesChanged(); }
</script>
