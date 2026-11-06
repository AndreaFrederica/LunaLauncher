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

const props = defineProps<{ instance: any }>();
const kind = computed(() => {
  // `format` and `vanillaCompatible` are the C++ source of truth.  Keep the
  // provenance fallback for older API snapshots so a recognized external
  // instance never silently loses its source badge while its detail object is
  // being refreshed.
  const provenance = props.instance?.provenance || {};
  const external = props.instance?.format === 'vanilla-compatible' || props.instance?.vanillaCompatible === true || Boolean(provenance.badgeId || provenance.primaryProvider || provenance.providers?.length);
  if (!external) return '';
  const provider = Array.isArray(provenance.providers) ? provenance.providers[0] : undefined;
  const badge = String(provenance.badgeId || provenance.primaryProvider || provider?.providerId || '').toLowerCase();
  if (badge === 'mixed' || provenance.mixed === true) return 'mixed';
  const dialect = String(props.instance?.dialect || badge || '').toLowerCase();
  return dialect === 'hmcl' || dialect === 'pcl' || dialect === 'luna' ? dialect : dialect && dialect !== 'mojang' && dialect !== 'vanilla' ? 'mixed' : 'vanilla';
});
const label = computed(() => kind.value === 'hmcl' ? 'HMCL 实例' : kind.value === 'pcl' ? 'PCL 实例' : kind.value === 'luna' ? 'Luna 创建的原版兼容实例' : kind.value === 'mixed' ? '多启动器修改的外部实例' : kind.value ? '原版格式实例' : '');
</script>

<style scoped>
.instance-format-badge{position:absolute;right:3px;bottom:3px;z-index:2;display:inline-flex;align-items:center;justify-content:center;width:22px;height:22px;overflow:hidden;pointer-events:none}
.instance-format-badge__icon{display:block;width:100%;height:100%;object-fit:contain}
</style>
