<!-- SPDX-License-Identifier: MPL-2.0 -->
<template>
  <div class="ef-home">
    <div class="ef-home__stats">
      <EfStat label="实例" :value="instances.length" note="已安装" />
      <EfStat label="运行中" :value="runningCount" :alert="runningCount > 0" note="当前会话" />
      <EfStat label="账户" :value="accounts.length" note="可用登录" />
    </div>

    <div class="ef-home__grid">
      <EfCard title="实例" class="ef-home__instances">
        <template #actions>
          <EfSearchBox v-model="query" placeholder="筛选实例" />
        </template>
        <div v-if="filtered.length" class="ef-home__instance-list">
          <EfListItem
            v-for="(item, index) in filtered"
            :key="item.id"
            :title="item.name"
            :info="item.version || item.runtime?.minecraft || 'Minecraft'"
            :index="index + 1"
            :selected="item.id === selected?.id"
            :tags="item.running ? ['运行中'] : []"
            @click="emit('select', item)"
          >
            <template #actions>
              <EfButton v-if="item.id === selected?.id" size="sm" variant="accent" icon="play_arrow" :disabled="busy || item.running" @click.stop="emit('launch')">启动</EfButton>
              <EfButton v-else size="sm" variant="ghost" icon="chevron_right" @click.stop="emit('select', item)">查看</EfButton>
            </template>
          </EfListItem>
        </div>
        <EfEmpty v-else :title="query ? '没有匹配的实例' : '还没有实例'" :description="query ? '换一个关键词。' : '先安装一个 Minecraft，再回来启动。'" icon="inventory_2">
          <EfButton variant="accent" @click="emit('refresh')">刷新</EfButton>
        </EfEmpty>
      </EfCard>

      <div class="ef-home__side">
        <EfCard v-if="selected" title="当前实例">
          <div class="ef-home__instance-hero">
            <div class="ef-home__instance-icon">
              <img v-if="icon" :src="icon" alt="" />
              <span v-else class="material-icons">inventory_2</span>
            </div>
            <div class="ef-home__instance-copy">
              <h2>{{ selected.name }}</h2>
              <p>{{ selected.version || selected.runtime?.minecraft || 'Minecraft' }}</p>
              <div class="ef-home__instance-meta">
                <EfTag v-if="selected.running">运行中</EfTag>
                <EfTag v-if="selected.group">{{ selected.group }}</EfTag>
              </div>
            </div>
          </div>
          <div class="ef-home__instance-actions">
            <EfButton variant="accent" icon="play_arrow" :disabled="busy || selected.running" @click="emit('launch')">启动</EfButton>
            <EfButton icon="folder_open" :disabled="busy" @click="emit('folder')">文件夹</EfButton>
            <EfButton icon="tune" :disabled="busy" @click="emit('settings')">详情</EfButton>
          </div>
        </EfCard>
        <EfCard v-else title="当前实例">
          <EfEmpty title="未选择实例" description="从左侧选择一个实例。" icon="touch_app" />
        </EfCard>

        <EfCard title="账户">
          <EfSelect :model-value="accountId" :options="accountOptions" placeholder="选择账户" @update:model-value="emit('account', $event)" />
          <p v-if="!accounts.length" class="ef-home__muted">还没有账户。<button type="button" @click="emit('accounts')">去账户页添加</button></p>
        </EfCard>

        <EfCard v-if="consoleLines?.length" title="控制台">
          <div class="ef-home__log">
            <div v-for="(line, index) in consoleLines.slice(-60)" :key="index">{{ line }}</div>
          </div>
        </EfCard>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from 'vue';
import EfButton from '../components/EfButton.vue';
import EfCard from '../components/EfCard.vue';
import EfSelect from '../components/EfSelect.vue';
import EfTag from '../components/EfTag.vue';
import EfListItem from '../components/EfListItem.vue';
import EfSearchBox from '../components/controls/EfSearchBox.vue';
import EfStat from '../components/data/EfStat.vue';
import EfEmpty from '../components/feedback/EfEmpty.vue';

const props = defineProps<{
  instances: any[];
  selected: any;
  accounts: any[];
  accountId?: string;
  busy: boolean;
  consoleLines?: string[];
  icon?: string;
}>();
const emit = defineEmits<{
  select: [instance: any];
  launch: [];
  settings: [];
  folder: [];
  refresh: [];
  account: [id: string];
  accounts: [];
}>();

const query = ref('');
const accountOptions = computed(() => props.accounts.map((account: any) => ({
  label: account.profileName || account.displayName || '未命名',
  value: String(account.internalId || account.id || account.profileName),
})));
const runningCount = computed(() => props.instances.filter((item: any) => item.running).length);
const filtered = computed(() => {
  const text = query.value.trim().toLowerCase();
  if (!text) return props.instances;
  return props.instances.filter((item: any) => `${item.name} ${item.group || ''} ${item.version || ''}`.toLowerCase().includes(text));
});
</script>

<style scoped>
.ef-home { width: 100%; }
.ef-home__stats {
  display: flex;
  gap: 8px;
  margin-bottom: 14px;
  overflow: auto;
}
.ef-home__grid {
  display: grid;
  grid-template-columns: minmax(280px, 1fr) 350px;
  gap: 14px;
}
.ef-home__instance-list { display: flex; flex-direction: column; }
.ef-home__instance-hero { display: flex; gap: 12px; align-items: flex-start; margin-bottom: 12px; }
.ef-home__instance-icon {
  flex: 0 0 64px;
  width: 64px;
  height: 64px;
  border: 1px solid var(--ef-rule);
  background: var(--ef-chip);
  display: grid;
  place-items: center;
}
.ef-home__instance-icon img { width: 100%; height: 100%; object-fit: cover; }
.ef-home__instance-icon .material-icons { font-size: 26px; color: var(--ef-ink-2); }
.ef-home__instance-copy { min-width: 0; }
.ef-home__instance-copy h2 {
  margin: 0 0 3px;
  font-family: var(--ef-font-ui);
  font-size: 17px;
  font-weight: 800;
}
.ef-home__instance-copy p { margin: 0 0 7px; color: var(--ef-ink-2); font-size: 11px; }
.ef-home__instance-meta { display: flex; gap: 4px; flex-wrap: wrap; }
.ef-home__instance-actions { display: flex; gap: 6px; flex-wrap: wrap; }
.ef-home__log {
  max-height: 220px;
  overflow: auto;
  font-family: var(--ef-font-mono);
  font-size: 10px;
  line-height: 1.55;
  color: var(--ef-ink-2);
  white-space: pre-wrap;
  word-break: break-all;
}
.ef-home__muted { margin: 8px 0 0; color: var(--ef-ink-2); font-size: 11px; }
.ef-home__muted button {
  padding: 0;
  border: 0;
  background: transparent;
  color: var(--ef-ink);
  text-decoration: underline;
  cursor: pointer;
}
@media (max-width: 1000px) {
  .ef-home__grid { grid-template-columns: 1fr; }
}
</style>
