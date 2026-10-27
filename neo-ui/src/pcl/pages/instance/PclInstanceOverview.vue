<template>
  <div class="pcl-overview">
    <PclCard>
      <div class="pcl-overview-head">
        <img :src="icon || fallback" :alt="instance.name" />
        <div class="pcl-overview-copy">
          <div class="pcl-kicker">{{ details?.minecraftVersion || instance.runtime?.minecraft || 'MINECRAFT' }}</div>
          <h2>{{ instance.name }}</h2>
          <p>{{ instance.type || t('instance.client') }}</p>
        </div>
        <div class="pcl-overview-actions">
          <PclButton size="sm" icon="folder-open" :disabled="busy" @click="emit('open-folder')">{{ t('instance.openFolder') }}</PclButton>
          <PclButton variant="highlight" icon="play" :disabled="busy || instance.running" @click="emit('launch')">{{ t('instance.launch') }}</PclButton>
        </div>
      </div>
    </PclCard>

    <PclCard :title="t('instance.info')">
      <div class="pcl-info-grid">
        <div><span>{{ t('instance.instanceDir') }}</span><code>{{ details?.root || t('about.loadingRuntime') }}</code></div>
        <div><span>{{ t('instance.gameDir') }}</span><code>{{ details?.gameRoot || t('about.loadingRuntime') }}</code></div>
        <div><span>{{ t('instance.modsDir') }}</span><code>{{ details?.modsRoot || t('about.loadingRuntime') }}</code></div>
        <div><span>{{ t('instance.status') }}</span><strong>{{ instance.running ? t('instance.running') : t('instance.stopped') }}</strong></div>
      </div>
    </PclCard>

    <PclCard :title="t('instance.customize')">
      <div class="pcl-form-row">
        <span class="pcl-form-row__label">{{ t('instance.icon') }}</span>
        <div class="pcl-form-row__control">
          <PclButton icon="image" :disabled="busy" @click="emit('change-icon')">{{ t('instance.changeIcon') }}</PclButton>
        </div>
      </div>
      <div class="pcl-form-row pcl-form-row--top">
        <span class="pcl-form-row__label">{{ t('instance.notes') }}</span>
        <div class="pcl-form-row__control pcl-form-row__control--column">
          <PclInput v-model="localNotes" multiline :rows="5" :placeholder="t('instance.notesInput')" />
        </div>
      </div>
      <div class="pcl-actions">
        <PclButton variant="highlight" icon="save" :disabled="busy" @click="emit('save-notes', localNotes)">{{ t('instance.saveNotes') }}</PclButton>
        <PclButton icon="pencil" :disabled="busy" @click="emit('action', 'rename')">{{ t('instance.rename') }}</PclButton>
        <PclButton icon="heart" :disabled="busy" @click="emit('action', 'favorite')">{{ t('instance.favorite') }}</PclButton>
      </div>
    </PclCard>

    <PclCard :title="t('instance.quickActions')">
      <div class="pcl-actions">
        <PclButton icon="folder-open" :disabled="busy" @click="emit('open-folder')">{{ t('instance.instanceDir') }}</PclButton>
        <PclButton icon="earth" :disabled="busy" @click="emit('action', 'open-saves')">{{ t('instance.savesDir') }}</PclButton>
        <PclButton icon="puzzle" :disabled="busy" @click="emit('action', 'open-mods')">{{ t('instance.modsDir') }}</PclButton>
        <PclButton icon="shield-check" :disabled="busy" @click="emit('action', 'verify')">{{ t('instance.verify') }}</PclButton>
        <PclButton icon="refresh-cw" :disabled="busy" @click="emit('action', 'update')">{{ t('instance.update') }}</PclButton>
        <PclButton variant="red" icon="trash-2" :disabled="busy" @click="emit('delete')">{{ t('instance.delete') }}</PclButton>
      </div>
    </PclCard>
  </div>
</template>

<script setup lang="ts">
import { ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclInput from '../../components/PclInput.vue';
import fallback from '../../../xmcl-port/assets/minecraft.webp';

const props = defineProps<{ instance: any; details: any; icon?: string; notes: string; busy: boolean }>();
const { t } = useI18n();
const emit = defineEmits<{ launch: []; 'open-folder': []; settings: []; 'change-icon': []; 'save-notes': [notes: string]; delete: []; action: [op: string] }>();
const localNotes = ref(props.notes);
watch(() => props.notes, value => { localNotes.value = value; });
</script>

<style scoped>
.pcl-overview {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.pcl-overview-head {
  display: flex;
  align-items: center;
  gap: 13px;
  min-width: 0;
}

.pcl-overview-head > img {
  flex: 0 0 64px;
  width: 64px;
  height: 64px;
  border-radius: 4px;
  object-fit: cover;
  image-rendering: pixelated;
}

.pcl-overview-copy {
  min-width: 0;
  flex: 1;
}

.pcl-overview-copy h2 {
  margin: 3px 0;
  overflow-wrap: anywhere;
  color: var(--pcl-text);
  font-size: calc(22px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-overview-copy p {
  margin: 0;
  color: var(--pcl-muted);
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-overview-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}

.pcl-info-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 12px 22px;
}

.pcl-info-grid div {
  min-width: 0;
}

.pcl-info-grid span {
  display: block;
  margin-bottom: 3px;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-info-grid code,
.pcl-info-grid strong {
  display: block;
  overflow: hidden;
  color: var(--pcl-text);
  font: calc(12px * var(--pcl-font-scale, 1))/1.5 ui-monospace, Consolas, monospace;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-info-grid strong {
  font-family: var(--pcl-font);
}

@media (max-width: 900px) {
  .pcl-overview-head {
    flex-wrap: wrap;
  }

  .pcl-overview-actions {
    flex-basis: 100%;
  }

  .pcl-info-grid {
    grid-template-columns: 1fr;
  }
}
</style>
