<template>
  <section class="xmcl-dashboard">
    <header class="dashboard-header">
      <div>
        <div class="eyebrow">{{ t('ui.launcherKicker') }}</div>
        <h1>{{ t('xmcl.dashboard.title') }}</h1>
        <p>{{ t('xmcl.dashboard.description') }}</p>
      </div>
      <div class="dashboard-actions">
        <q-btn flat round icon="refresh" :disable="busy" @click="emit('refresh')" />
        <q-btn v-if="selectedIds.length" flat icon="close" :label="t('xmcl.dashboard.clearSelection', { count: selectedIds.length })" @click="selectedIds=[]" />
        <q-btn v-if="selectedIds.length" color="positive" icon="play_arrow" :label="t('xmcl.dashboard.batchLaunch')" :disable="busy" @click="launchSelected" />
        <q-btn color="primary" icon="add" :label="t('xmcl.dashboard.add')" :disable="busy" @click="emit('add')" />
      </div>
    </header>
    <div class="dashboard-toolbar">
      <q-input v-model="query" dark borderless dense class="dashboard-search" :placeholder="t('xmcl.dashboard.search')">
        <template #prepend><q-icon name="search" /></template>
      </q-input>
      <q-btn-toggle v-model="filter" flat unelevated toggle-color="primary" :options="[{ label: t('xmcl.dashboard.all'), value: 'all' }, { label: t('xmcl.dashboard.running'), value: 'running' }, { label: t('xmcl.dashboard.recent'), value: 'recent' }]" />
      <span class="dashboard-count">{{ filtered.length }} / {{ instances.length }}</span>
    </div>
    <div v-if="!filtered.length" class="dashboard-empty">
      <span class="material-icons">sports_esports</span>
      <h3>{{ instances.length ? t('xmcl.dashboard.noMatch') : t('xmcl.dashboard.empty') }}</h3>
      <p>{{ instances.length ? t('xmcl.dashboard.noMatchHint') : t('xmcl.dashboard.emptyHint') }}</p>
      <q-btn color="primary" icon="add" :label="t('xmcl.dashboard.add')" @click="emit('add')" />
    </div>
    <div v-else class="dashboard-grid">
      <article v-for="instance in filtered" :key="instance.id" class="dashboard-card" :class="{ running: instance.running, selected: selectedIds.includes(instance.id) }" @click="emit('select', instance)">
        <div class="dashboard-art" :style="artStyle(instance)">
          <img :src="icons[instance.id] || fallback" :alt="instance.name" />
          <div class="dashboard-art-meta"><q-checkbox :model-value="selectedIds.includes(instance.id)" dark dense keep-color color="primary" @click.stop="toggleSelected(instance)" /><q-badge v-if="instance.running" color="positive">{{ t('xmcl.dashboard.runningStatus') }}</q-badge><q-badge v-if="instance.group" color="dark">{{ instance.group }}</q-badge></div>
        </div>
        <div class="dashboard-card-body">
          <div class="dashboard-card-title"><strong>{{ instance.name }}</strong><q-icon v-if="instance.managed" name="cloud_done" color="primary" /></div>
          <div class="dashboard-card-meta"><span>{{ instance.runtime?.minecraft || instance.minecraftVersion || 'Minecraft' }}</span><span>·</span><span>{{ instance.type || t('xmcl.dashboard.client') }}</span></div>
          <p v-if="instance.notes" class="dashboard-notes">{{ instance.notes }}</p>
          <div class="dashboard-card-actions"><q-btn flat dense icon="tune" :label="t('xmcl.dashboard.manage')" @click.stop="emit('select', instance)" /><q-btn flat dense icon="check_box" :label="selectedIds.includes(instance.id) ? t('xmcl.dashboard.selected') : t('xmcl.dashboard.select')" @click.stop="toggleSelected(instance)" /><q-btn color="primary" dense :icon="instance.running ? 'stop' : 'play_arrow'" :label="instance.running ? t('xmcl.dashboard.runningStatus') : t('xmcl.dashboard.launch')" :disable="instance.running || busy" @click.stop="emit('launch', instance)" /></div>
        </div>
      </article>
    </div>
  </section>
</template>
<script setup lang="ts">
import { computed, ref } from 'vue';
import { useI18n } from 'vue-i18n';
import fallback from './assets/minecraft.webp';
const props = defineProps<{ instances: any[]; icons: Record<string, string>; busy: boolean }>();
const { t } = useI18n();
const emit = defineEmits<{ select: [instance: any]; launch: [instance: any]; 'batch-launch': [instances: any[]]; add: []; refresh: [] }>();
const query = ref('');
const filter = ref('all');
const selectedIds = ref<string[]>([]);
const filtered = computed(() => {
  const q = query.value.trim().toLowerCase();
  return props.instances.filter((i) => {
    if (filter.value === 'running' && !i.running) return false;
    if (filter.value === 'recent' && !i.lastLaunch) return false;
    if (!q) return true;
    return `${i.name} ${i.id} ${i.group || ''} ${i.type || ''} ${i.runtime?.minecraft || ''}`.toLowerCase().includes(q);
  });
});
function artStyle(instance: any) {
  const key = String(instance.id || instance.name || 'minecraft').length;
  const palettes = ['#334155,#1d4ed8', '#312e81,#7c3aed', '#164e63,#0891b2', '#365314,#65a30d', '#7c2d12,#ea580c'];
  const palette = palettes[key % palettes.length].split(',');
  return { background: `linear-gradient(135deg, ${palette[0]}, ${palette[1]})` };
}
function toggleSelected(instance: any) {
  if (instance.running) return;
  selectedIds.value = selectedIds.value.includes(instance.id)
    ? selectedIds.value.filter((id) => id !== instance.id)
    : [...selectedIds.value, instance.id];
}
function launchSelected() {
  const selected = props.instances.filter((instance) => selectedIds.value.includes(instance.id) && !instance.running);
  if (selected.length) {
    emit('batch-launch', selected);
    selectedIds.value = [];
  }
}
</script>
<style scoped>
.xmcl-dashboard{max-width:1320px;margin:0 auto;padding:28px 24px 48px}.dashboard-header{display:flex;justify-content:space-between;align-items:flex-end;gap:24px;margin-bottom:28px}.dashboard-header h1{margin:4px 0 8px;font-size:36px;letter-spacing:-.04em}.dashboard-header p{margin:0;color:rgba(255,255,255,.58)}.eyebrow{font-size:11px;letter-spacing:.16em;color:#a5b4fc;font-weight:700}.dashboard-actions{display:flex;gap:8px}.dashboard-toolbar{display:flex;align-items:center;gap:16px;margin-bottom:22px;padding:7px 12px;border:1px solid rgba(255,255,255,.1);border-radius:14px;background:rgba(255,255,255,.045)}.dashboard-search{flex:1;min-width:180px}.dashboard-count{color:rgba(255,255,255,.48);font-size:12px}.dashboard-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:16px}.dashboard-card{overflow:hidden;border:1px solid rgba(255,255,255,.1);border-radius:18px;background:rgba(31,31,40,.78);cursor:pointer;transition:transform .18s,border-color .18s,box-shadow .18s}.dashboard-card:hover{transform:translateY(-3px);border-color:rgba(129,140,248,.65);box-shadow:0 16px 36px rgba(0,0,0,.25)}.dashboard-card.running{border-color:rgba(74,222,128,.65)}.dashboard-art{height:142px;display:flex;align-items:flex-end;justify-content:space-between;padding:14px}.dashboard-art img{width:58px;height:58px;border-radius:16px;object-fit:cover;box-shadow:0 6px 18px rgba(0,0,0,.28)}.dashboard-art-meta{display:flex;gap:5px;align-items:center}.dashboard-card-body{padding:16px}.dashboard-card-title{display:flex;align-items:center;gap:8px;font-size:17px}.dashboard-card-title strong{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.dashboard-card-meta{display:flex;gap:6px;margin-top:7px;color:rgba(255,255,255,.55);font-size:12px}.dashboard-notes{height:18px;margin:9px 0 0;color:rgba(255,255,255,.65);font-size:12px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.dashboard-card-actions{display:flex;justify-content:space-between;gap:8px;margin-top:16px}.dashboard-card-actions .q-btn:last-child{flex:1}.dashboard-empty{display:grid;place-items:center;text-align:center;min-height:440px;color:rgba(255,255,255,.6)}.dashboard-empty .material-icons{font-size:64px;color:#818cf8}.dashboard-empty h3{margin:12px 0 4px;color:#fff}.dashboard-empty p{margin:0 0 18px}@media(max-width:700px){.xmcl-dashboard{padding:20px 12px}.dashboard-header{display:block}.dashboard-actions{margin-top:16px}.dashboard-toolbar{flex-wrap:wrap}.dashboard-search{flex-basis:100%}}
.dashboard-card.selected{border-color:rgba(129,140,248,.95);box-shadow:0 0 0 2px rgba(129,140,248,.25)}
</style>
