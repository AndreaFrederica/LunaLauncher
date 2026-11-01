<template>
  <PclCard v-if="selectedAccount" :title="t('account.authConfig')" class="pcl-account-auth-config">
    <template #actions>
      <span class="pcl-auth-account">{{ selectedAccount.profileName || selectedAccount.displayName || t('account.unnamed') }}</span>
    </template>
    <PclHint theme="blue">{{ t('account.authHint') }}</PclHint>
    <div v-if="selectedAccount.type === 'yggdrasil'" class="pcl-config-grid">
      <label class="pcl-field"><span>{{ t('account.authUrl') }}</span><PclInput v-model="config.authUrl" placeholder="https://..." /></label>
      <label class="pcl-field"><span>{{ t('account.sessionUrl') }}</span><PclInput v-model="config.sessionUrl" :placeholder="t('account.displayName')" /></label>
      <label class="pcl-field"><span>{{ t('account.serviceName') }}</span><PclInput v-model="config.sourceName" :placeholder="t('account.displayName')" /></label>
      <label class="pcl-field"><span>{{ t('account.tokenType') }}</span><PclSelect v-model="config.tokenType" :options="tokenTypeOptions" /></label>
      <label class="pcl-field"><span>{{ t('ui.authenticateEndpoint') }}</span><PclInput v-model="config.authenticateEndpoint" /></label>
      <label class="pcl-field"><span>{{ t('ui.refreshEndpoint') }}</span><PclInput v-model="config.refreshEndpoint" /></label>
      <label class="pcl-field"><span>{{ t('ui.validateEndpoint') }}</span><PclInput v-model="config.validateEndpoint" /></label>
      <label class="pcl-field"><span>{{ t('ui.profileEndpoint') }}</span><PclInput v-model="config.profileEndpoint" /></label>
      <label class="pcl-field"><span>{{ t('ui.oauthTokenEndpoint') }}</span><PclInput v-model="config.oauthTokenEndpoint" /></label>
    </div>
    <div v-else-if="selectedAccount.type === 'unified-pass'" class="pcl-config-grid">
      <label class="pcl-field"><span>{{ t('ui.unifiedPassServerId') }}</span><PclInput v-model="config.serverId" /></label>
      <label class="pcl-field"><span>{{ t('ui.apiBaseUrl') }}</span><PclInput v-model="config.baseUrl" /></label>
    </div>
    <PclHint v-else theme="yellow">{{ t('account.authConfigUnavailable') }}</PclHint>
    <div v-if="canConfigure" class="pcl-actions">
      <PclButton variant="highlight" icon="save" :disabled="busy" @click="saveConfig">{{ t('account.saveAuth') }}</PclButton>
    </div>
  </PclCard>
  <PclCard v-else :title="t('account.authConfig')" class="pcl-account-auth-config">
    <PclHint theme="yellow">{{ t('account.selectAccountForAuth') }}</PclHint>
  </PclCard>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclHint from '../../components/PclHint.vue';
import PclInput from '../../components/PclInput.vue';
import PclSelect from '../../components/PclSelect.vue';
import { useLauncherSessionStore } from '../../../../common/launcher-session';
import { PclAccountContextKey } from './PclAccountContext';
import { inject } from 'vue';

const context = inject(PclAccountContextKey);
if (!context) throw new Error('PclAccountContext is missing');
const { t, accounts, busy, execute, emit } = context;
const launcherSession = useLauncherSessionStore();
const config = ref<any>({
  authUrl: '', sessionUrl: '', sourceName: '', tokenType: 'Standard',
  authenticateEndpoint: '', refreshEndpoint: '', validateEndpoint: '', profileEndpoint: '', oauthTokenEndpoint: '',
  serverId: '', baseUrl: '',
});
const tokenTypeOptions = [{ label: 'Standard', value: 'Standard' }, { label: 'OAuth', value: 'OAuth' }];
const selectedAccount = computed(() => {
  const current = String(launcherSession.selectedAccountId || '');
  return (accounts.value || []).find((account: any) => String(account.internalId || account.id || account.profileName || '') === current)
    || (accounts.value || []).find((account: any) => account.default)
    || (accounts.value || [])[0]
    || null;
});
const canConfigure = computed(() => selectedAccount.value?.type === 'yggdrasil' || selectedAccount.value?.type === 'unified-pass');

function accountKey(account: any) { return String(account?.internalId || account?.id || account?.profileName || ''); }
function loadConfig(account: any) {
  config.value = {
    authUrl: account?.yggdrasil?.authUrl || '', sessionUrl: account?.yggdrasil?.sessionUrl || '',
    sourceName: account?.yggdrasil?.sourceName || '', tokenType: account?.yggdrasil?.tokenType || 'Standard',
    authenticateEndpoint: account?.yggdrasil?.authenticateEndpoint || '', refreshEndpoint: account?.yggdrasil?.refreshEndpoint || '',
    validateEndpoint: account?.yggdrasil?.validateEndpoint || '', profileEndpoint: account?.yggdrasil?.profileEndpoint || '',
    oauthTokenEndpoint: account?.yggdrasil?.oauthTokenEndpoint || '',
    serverId: account?.unifiedPass?.serverId || '', baseUrl: account?.unifiedPass?.baseUrl || '',
  };
}
watch(selectedAccount, account => loadConfig(account), { immediate: true });

async function saveConfig() {
  const account = selectedAccount.value;
  if (!account || !canConfigure.value) return;
  const payload: any = { account: accountKey(account) };
  if (account.type === 'yggdrasil') {
    Object.assign(payload, {
      authUrl: config.value.authUrl, sessionUrl: config.value.sessionUrl, sourceName: config.value.sourceName,
      tokenType: config.value.tokenType, authenticateEndpoint: config.value.authenticateEndpoint,
      refreshEndpoint: config.value.refreshEndpoint, validateEndpoint: config.value.validateEndpoint,
      profileEndpoint: config.value.profileEndpoint, oauthTokenEndpoint: config.value.oauthTokenEndpoint,
    });
  } else {
    Object.assign(payload, { serverId: config.value.serverId, baseUrl: config.value.baseUrl });
  }
  try {
    await execute('account.configure', payload);
    emit('changed');
  } catch (exception) {
    // Let the page-level error banner handle configuration failures.
    const message = exception instanceof Error ? exception.message : String(exception);
    context.accountError.value = message;
  }
}
</script>

<style scoped>
.pcl-auth-account {
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-config-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 10px 14px;
  margin: 12px 0;
}

.pcl-field {
  display: flex;
  flex-direction: column;
  gap: 4px;
  min-width: 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  margin-top: 12px;
}

@media (max-width: 900px) {
  .pcl-config-grid {
    grid-template-columns: 1fr;
  }
}
</style>
