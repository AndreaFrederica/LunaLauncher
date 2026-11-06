<template>
  <span v-if="kind" class="instance-format-badge" :class="`is-${kind}`" :title="label" :aria-label="label">
    <img v-if="kind === 'vanilla'" class="instance-format-badge__icon" :src="grassBadge" alt="" aria-hidden="true" />
    <img v-else-if="kind === 'hmcl'" class="instance-format-badge__icon" :src="hmclBadge" alt="" aria-hidden="true" />
    <img v-else-if="kind === 'pcl'" class="instance-format-badge__icon" :src="pclBadge" alt="" aria-hidden="true" />
    <img v-else-if="kind === 'luna'" class="instance-format-badge__icon" :src="lunaBadge" alt="" aria-hidden="true" />
    <img v-else-if="kind === 'mixed'" class="instance-format-badge__icon" :src="mixedBadge" alt="" aria-hidden="true" />
  </span>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import grassBadge from '../assets/instance-format-grass.png';
import hmclBadge from '../assets/instance-format-hmcl.png';
import pclBadge from '../assets/instance-format-pcl.png';
import lunaBadge from '../assets/instance-format-luna.png';
import mixedBadge from '../assets/instance-format-mixed.png';
import { neoUiPreferences } from '../neo-ui-preferences';

const props = defineProps<{ instance: any }>();
const kind = computed(() => {
  // `format` and `vanillaCompatible` are the C++ source of truth.  Keep the
  // provenance fallback for older API snapshots so a recognized external
  // instance never silently loses its source badge while its detail object is
  // being refreshed.
  const provenance = props.instance?.provenance || {};
  const external = props.instance?.format === 'vanilla-compatible' || props.instance?.vanillaCompatible === true || Boolean(provenance.badgeId || provenance.primaryProvider || provenance.providers?.length);
  if (!external) return '';
  const providers = Array.isArray(provenance.providers) ? provenance.providers : [];
  const provider = providers[0];
  const badge = String(provenance.badgeId || provenance.primaryProvider || provider?.providerId || '').toLowerCase();
  const changes = props.instance?.lunaChanges;
  const substantial = changes?.substantial === true
    || changes?.actions?.includes?.('loader') === true;
  const hasLunaProvider = badge === 'luna' || providers.some((item: any) => String(item?.providerId || '').toLowerCase() === 'luna');
  // Remove Luna's compatibility marker from the visual result when it only
  // represents the launcher sidecar. Keep any HMCL/PCL evidence visible.
  if (hasLunaProvider && !neoUiPreferences.pclShowLunaBadge && !substantial) {
    const remaining = providers.filter((item: any) => String(item?.providerId || '').toLowerCase() !== 'luna');
    if (!remaining.length) return 'vanilla';
    if (remaining.length > 1) return 'mixed';
    const remainingBadge = String(remaining[0]?.providerId || '').toLowerCase();
    return remainingBadge === 'hmcl' || remainingBadge === 'pcl' ? remainingBadge : 'vanilla';
  }
  if (badge === 'mixed' || provenance.mixed === true) return 'mixed';
  const dialect = String(props.instance?.dialect || badge || '').toLowerCase();
  // A Luna sidecar can contain only launcher-owned metadata (name, icon,
  // notes, group). Keep that implementation detail hidden by default. A
  // recorded loader change is a real game modification and remains visible.
  return dialect === 'hmcl' || dialect === 'pcl' || dialect === 'luna' ? dialect : dialect && dialect !== 'mojang' && dialect !== 'vanilla' ? 'mixed' : 'vanilla';
});
const label = computed(() => kind.value === 'hmcl' ? 'HMCL 实例' : kind.value === 'pcl' ? 'PCL 实例' : kind.value === 'luna' ? 'Luna 创建的原版兼容实例' : kind.value === 'mixed' ? '多启动器修改的外部实例' : kind.value ? '原版格式实例' : '');
</script>

<style scoped>
.instance-format-badge{position:absolute;right:3px;bottom:3px;z-index:2;display:inline-flex;align-items:center;justify-content:center;width:22px;height:22px;overflow:hidden;pointer-events:none}
.instance-format-badge__icon{display:block;width:100%;height:100%;object-fit:contain}
</style>
