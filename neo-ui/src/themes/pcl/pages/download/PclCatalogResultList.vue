<template>
  <div class="pcl-result-list">
    <template v-for="item in items" :key="item.projectId">
    <button
      class="pcl-result-row"
      type="button"
      :disabled="busy"
      @click="emit('select', item)"
    >
      <span v-if="selectable" class="pcl-result-check" @click.stop>
        <PclCheckbox :model-value="Boolean(selected?.[item.projectId])" :disabled="busy" @update:model-value="emit('toggle', item, $event)" />
      </span>
      <img v-if="item.iconUrl" :src="item.iconUrl" alt="" />
      <span v-else class="pcl-result-icon"><PclIcon name="box" :size="24" /></span>
      <span class="pcl-result-copy">
        <strong>{{ item.name }}<span v-if="item.mcmod?.name" class="pcl-result-localized-name"> · {{ item.mcmod.name }}</span></strong>
        <small>{{ item.description || fallbackDescription }}</small>
        <em class="pcl-result-links">
          <template v-for="link in platformLinks(item)" :key="`${link.label}:${link.url}`">
            <span class="pcl-result-link" role="link" tabindex="0" @click.stop="openExternal(link.url)" @keydown.enter.stop="openExternal(link.url)">{{ link.label }}</span>
          </template>
          <span v-if="!platformLinks(item).length">{{ providerLabel(item.provider || provider) }}</span>
          <span v-if="item.projectId"> · {{ item.projectId }}</span>
          <span v-if="item.mcmod?.url || item.mcmod?.searchUrl" class="pcl-result-link" role="link" tabindex="0" @click.stop="openExternal(item.mcmod.url || item.mcmod.searchUrl)" @keydown.enter.stop="openExternal(item.mcmod.url || item.mcmod.searchUrl)">{{ t('resource.mcmodLink') }}</span>
          <span v-if="item.sourceUrl" class="pcl-result-link" role="link" tabindex="0" @click.stop="openExternal(item.sourceUrl)" @keydown.enter.stop="openExternal(item.sourceUrl)">{{ t('resource.sourceCode') }}</span>
          <span v-if="item.websiteUrl && item.websiteUrl !== providerUrl(item)" class="pcl-result-link" role="link" tabindex="0" @click.stop="openExternal(item.websiteUrl)" @keydown.enter.stop="openExternal(item.websiteUrl)">{{ t('resource.projectPage') }}</span>
        </em>
      </span>
      <PclIcon name="chevron-right" :size="16" />
    </button>
    <slot name="expanded" :item="item" />
    </template>
    <div v-if="searched && !items.length && !busy" class="pcl-catalog-empty">
      <PclIcon name="search" :size="30" />{{ emptyText }}
    </div>
  </div>
</template>

<script setup lang="ts">
import { useI18n } from 'vue-i18n';
import PclIcon from '../../components/PclIcon.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';

const { t } = useI18n();

const props = defineProps<{
  items: any[];
  busy: boolean;
  searched: boolean;
  execute: (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
  provider: string;
  providerLabel: (value: string) => string;
  fallbackDescription: string;
  emptyText: string;
  selectable?: boolean;
  selected?: Record<string, any>;
}>();

const emit = defineEmits<{ select: [item: any]; toggle: [item: any, value: boolean] }>();

function providerUrl(item: any) {
  if (item?.providerUrl) return String(item.providerUrl);
  const source = String(item?.provider || '');
  const id = String(item?.projectId || item?.slug || '').trim();
  if (!id) return '';
  if (source === 'modrinth') return `https://modrinth.com/mod/${item?.slug || id}`;
  if (source === 'curseforge') return `https://www.curseforge.com/projects/${id}`;
  return '';
}
function platformLinks(item: any) {
  const links: Array<{ label: string; url: string }> = [];
  const add = (label: string, url: unknown) => {
    const value = String(url || '').trim();
    if (value && !links.some(link => link.url === value)) links.push({ label, url: value });
  };
  const isOfficialProjectUrl = (source: string, url: string) => {
    const value = url.toLowerCase();
    return (source === 'modrinth' && value.includes('modrinth.com/')) || (source === 'curseforge' && value.includes('curseforge.com/'));
  };
  const metadata = item?.providerMeta || {};
  for (const [source, raw] of Object.entries(metadata)) {
    const data: any = typeof raw === 'string' ? { providerUrl: raw } : raw || {};
    const ownProjectUrl = String(data.projectUrl || '').trim();
    const canonicalUrl = String(data.providerUrl || '').trim();
    const platform = ownProjectUrl && isOfficialProjectUrl(source, ownProjectUrl) ? ownProjectUrl : (canonicalUrl || ownProjectUrl);
    add(props.providerLabel(source), platform);
    add(t('resource.sourceCode'), data.sourceUrl);
    if (ownProjectUrl && ownProjectUrl !== platform) add(t('resource.projectPage'), ownProjectUrl);
  }
  const source = String(item?.provider || props.provider || '');
  add(props.providerLabel(source), item?.providerUrl || providerUrl(item));
  const providerLinks = item?.providerLinks || {};
  add('Modrinth', typeof providerLinks.modrinth === 'string' ? providerLinks.modrinth : providerLinks.modrinth?.url);
  add('CurseForge', typeof providerLinks.curseforge === 'string' ? providerLinks.curseforge : providerLinks.curseforge?.url);
  add('Modrinth', item?.mcmod?.modrinthId && `https://modrinth.com/mod/${item.mcmod.modrinthId}`);
  add('CurseForge', item?.mcmod?.curseForgeId && `https://www.curseforge.com/projects/${item.mcmod.curseForgeId}`);
  return links;
}
async function openExternal(url: string) {
  const value = String(url || '').trim();
  if (!value) return;
  try {
    await props.execute('desktop.open-url', { url: value });
  } catch {
    // Keep browser preview usable when the native bridge is unavailable.
    window.open(value, '_blank', 'noopener,noreferrer');
  }
}
</script>

<style scoped>
.pcl-result-list {
  display: flex;
  flex-direction: column;
  border-top: 1px solid var(--pcl-border);
}

.pcl-result-row {
  display: flex;
  align-items: center;
  gap: 11px;
  min-height: 64px;
  padding: 9px 6px;
  border: 0;
  border-bottom: 1px solid var(--pcl-border);
  background: transparent;
  color: var(--pcl-text);
  text-align: left;
  cursor: pointer;
}

.pcl-result-row:hover { background: var(--pcl-hover); }
.pcl-result-row:disabled { cursor: default; opacity: .65; }
.pcl-result-check { display:flex; flex:0 0 auto; align-items:center; }

.pcl-catalog-inline-project {
  margin: 8px 0 12px;
}

.pcl-result-row > img,
.pcl-result-icon {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 46px;
  width: 46px;
  height: 46px;
  border-radius: 5px;
  background: var(--pcl-c6);
  color: var(--pcl-c2);
  object-fit: cover;
}

.pcl-result-copy {
  display: flex;
  flex-direction: column;
  min-width: 0;
  flex: 1;
}

.pcl-result-copy strong { color: var(--pcl-text); font-size: calc(12px * var(--pcl-font-scale, 1)); font-weight: 500; }
.pcl-result-localized-name { color: var(--pcl-muted); font-size: .92em; font-weight: 400; }
.pcl-result-copy small,
.pcl-result-copy em { overflow: hidden; color: var(--pcl-muted); font-size: calc(10px * var(--pcl-font-scale, 1)); font-style: normal; text-overflow: ellipsis; white-space: nowrap; }
.pcl-result-copy small { margin-top: 2px; }
.pcl-result-copy em { margin-top: 3px; color: var(--pcl-c5); font-size: calc(9px * var(--pcl-font-scale, 1)); }
.pcl-result-links { display: flex; flex-wrap: wrap; align-items: center; gap: 4px; margin-top: 3px; color: var(--pcl-c5); }
.pcl-result-link { display: inline-flex; align-items: center; min-height: 18px; padding: 1px 6px; border: 1px solid currentColor; border-radius: 4px; color: inherit; cursor: pointer; line-height: 1.2; }
.pcl-result-link:hover { background: color-mix(in srgb, currentColor 12%, transparent); text-decoration: none; }

.pcl-catalog-empty { display: flex; flex-direction: column; align-items: center; gap: 7px; padding: 45px 20px; color: var(--pcl-muted); font-size: calc(11px * var(--pcl-font-scale, 1)); }
.pcl-catalog-empty :deep(svg) { color: var(--pcl-c5); }
</style>
