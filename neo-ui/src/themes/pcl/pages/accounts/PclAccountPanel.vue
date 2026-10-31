<template>
  <div class="pcl-account-panel">
    <div class="pcl-account-list">
      <div
        v-for="account in accounts"
        :key="accountRef(account)"
        class="pcl-account-row"
        :class="{ active: selected === accountRef(account) }"
        @click="select(account)"
      >
        <div class="pcl-account-avatar">
          <PclAccountAvatar v-if="account.profile?.skin?.url" :src="account.profile.skin.url" :name="account.profileName || account.displayName" />
          <span v-else>{{ initial(account) }}</span>
        </div>
        <div class="pcl-account-copy">
          <strong>
            {{ account.profileName || account.displayName || t('account.unnamed') }}
            <em v-if="account.default">{{ t('account.default') }}</em>
            <em v-if="account.cacheAvailable" class="cached">{{ t('account.cached') }}</em>
            <em v-else-if="account.cacheState === 'not-cached'">{{ t('account.notCached') }}</em>
          </strong>
          <small>{{ accountTypeLabel(account) }} · {{ accountStateLabel(account) }} · {{ t('account.cache') }}: {{ cacheStateLabel(account) }}</small>
        </div>
        <div class="pcl-account-actions" @click.stop>
          <PclIconButton icon="move-up" :label="t('account.moveUp')" :size="24" :disabled="busy" @click="move(account, -1)" />
          <PclIconButton icon="move-down" :label="t('account.moveDown')" :size="24" :disabled="busy" @click="move(account, 1)" />
          <PclIconButton icon="circle-check" :label="t('account.setDefault')" :size="24" :disabled="busy || account.default" @click="setDefault(account)" />
          <PclIconButton icon="trash-2" :label="t('account.removeAccount')" :size="24" danger :disabled="busy" @click="remove(account)" />
        </div>
      </div>
    </div>

    <PclCard v-if="profile" :title="profile.name" class="pcl-profile-card">
      <template #actions>
        <span class="pcl-profile-state">{{ profile.state }} · {{ profile.canPlayMinecraft ? t('account.canPlay') : t('account.cannotPlay') }}</span>
      </template>
      <div class="pcl-profile-grid">
        <div class="pcl-profile-skin">
          <PclSkinPreview v-if="profile.skin?.url" :src="profile.skin.url" />
          <div v-else class="pcl-skin-empty">{{ t('account.skinEquipped') }}</div>

          <label class="pcl-field">
            <span>{{ t('account.skinPath') }}</span>
            <div class="pcl-field-row">
              <PclInput v-model="skinPath" placeholder="D:\skins\skin.png" />
              <PclIconButton icon="folder-open" :label="t('account.chooseSkin')" :size="28" :disabled="busy" @click="chooseSkinFile" />
              <PclButton icon="file-input" :disabled="busy || !skinPath" @click="installSkin">{{ t('account.import') }}</PclButton>
            </div>
          </label>

          <div class="pcl-field-row">
            <label class="pcl-field">
              <span>{{ t('account.localSkin') }}</span>
              <PclSelect v-model="skinName" :options="skinNames" :placeholder="t('account.chooseSkinName')" />
            </label>
            <label class="pcl-field pcl-field--narrow">
              <span>{{ t('account.model') }}</span>
              <PclSelect v-model="variant" :options="variantOptions" />
            </label>
          </div>
          <div class="pcl-actions">
            <PclButton variant="highlight" icon="upload" :disabled="busy || !skinName" @click="uploadSkin">{{ t('account.uploadEquip') }}</PclButton>
            <PclButton variant="red" icon="rotate-ccw" :disabled="busy" @click="resetSkin">{{ t('account.resetRemote') }}</PclButton>
          </div>
        </div>

        <div class="pcl-profile-capes">
          <h3>{{ t('account.cape') }}</h3>
          <PclListItem
            v-for="cape in profile.capes || []"
            :key="cape.id"
            :title="cape.alias || cape.id"
            :selected="cape.current"
            @click="selectCape(cape)"
          >
            <template #icon><PclIcon name="flag" :size="16" /></template>
            <template #actions>
              <PclIcon :name="cape.current ? 'circle-check' : 'minus'" :size="15" :class="{ 'pcl-cape-current': cape.current }" />
            </template>
          </PclListItem>
          <div v-if="!profile.capes?.length" class="pcl-empty pcl-empty--compact">{{ t('account.noCapes') }}</div>
          <PclButton variant="ghost" icon="x" :disabled="busy" @click="chooseCape('')">{{ t('account.removeCape') }}</PclButton>

          <h3>{{ t('account.skinLibrary') }}</h3>
          <PclListItem v-for="skin in skins" :key="skin.name || skin.path" :title="skin.name || skin.path">
            <template #icon><PclIcon name="image" :size="16" /></template>
            <template #actions>
              <PclIconButton icon="trash-2" :label="t('account.deleteSkin')" :size="24" danger :disabled="busy" @click.stop="deleteSkin(skin)" />
            </template>
          </PclListItem>
          <div v-if="!skins.length" class="pcl-empty pcl-empty--compact">{{ t('account.emptySkinLibrary') }}</div>
        </div>
      </div>
    </PclCard>

    <PclCard v-if="selectedAccount" :title="t('account.authConfig')">
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
      <div class="pcl-actions">
        <PclButton variant="highlight" icon="save" :disabled="busy" @click="saveConfig">{{ t('account.saveAuth') }}</PclButton>
      </div>
    </PclCard>

    <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>
  </div>
</template>

<script setup lang="ts">
import { ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import PclAccountAvatar from '../../components/PclAccountAvatar.vue';
import PclButton from '../../components/PclButton.vue';
import PclCard from '../../components/PclCard.vue';
import PclHint from '../../components/PclHint.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclSelect from '../../components/PclSelect.vue';
import PclSkinPreview from './PclSkinPreview.vue';
import { pickPclFile } from '../../composables/usePclFileDialog';
import { useLauncherSessionStore } from '../../../../common/launcher-session';

const props = defineProps<{ accounts: any[]; busy: boolean; execute: (op: string, p?: Record<string, unknown>) => Promise<any> }>();
const { t } = useI18n();
const emit = defineEmits<{ changed: [] }>();
const launcherSession = useLauncherSessionStore();

const selected = ref<any>(''), selectedAccount = ref<any>(null), profile = ref<any>(null), skins = ref<any[]>([]);
const skinPath = ref<any>(''), skinName = ref<any>(''), variant = ref<any>('classic'), error = ref<any>('');
const skinNames = ref<string[]>([]);
const variantOptions = [{ label: 'Classic', value: 'classic' }, { label: 'Slim', value: 'slim' }];
const tokenTypeOptions = [{ label: 'Standard', value: 'Standard' }, { label: 'OAuth', value: 'OAuth' }];
const config = ref<any>({
  authUrl: '', sessionUrl: '', sourceName: '', tokenType: 'Standard',
  authenticateEndpoint: '', refreshEndpoint: '', validateEndpoint: '', profileEndpoint: '', oauthTokenEndpoint: '',
  serverId: '', baseUrl: '',
});

function accountRef(account: any) { return String(account?.internalId || account?.id || account?.profileName || ''); }
function resolveAccount(referenceOrAccount: any) {
  const reference = accountRef(referenceOrAccount);
  return props.accounts.find(account => accountRef(account) === reference) || (typeof referenceOrAccount === 'object' ? referenceOrAccount : null);
}
function accountSnapshotKey(account: any) {
  if (!account) return '';
  return JSON.stringify({
    id: accountRef(account),
    type: account.type,
    stateName: account.stateName,
    cacheState: account.cacheState,
    cacheAvailable: account.cacheAvailable,
    profileName: account.profileName,
    displayName: account.displayName,
    profile: account.profile ? { name: account.profile.name, state: account.profile.state, skin: account.profile.skin, capes: account.profile.capes } : null,
    yggdrasil: account.yggdrasil || null,
    unifiedPass: account.unifiedPass || null,
  });
}
function initial(account: any) { return String(account?.profileName || account?.displayName || '?').slice(0, 1).toUpperCase(); }
function accountTypeLabel(account: any) {
  const type = String(account?.type || '').toLowerCase();
  if (type.includes('msa') || type.includes('microsoft')) return t('account.microsoftAccount');
  if (type.includes('ygg')) {
    const server = account?.yggdrasil?.displayName || account?.yggdrasil?.sourceName || account?.authServer?.displayName || account?.authServerName || t('account.thirdParty');
    return `${t('account.thirdParty')} · ${server}`;
  }
  if (type.includes('unified')) {
    const server = account?.unifiedPass?.displayName || account?.unifiedPass?.serverName || account?.unifiedPass?.serverId || account?.authServer?.displayName || account?.authServerName || 'UnifiedPass';
    return `UnifiedPass account · ${server}`;
  }
  return t('account.offlineAccountLabel');
}
function accountStateLabel(account: any) {
  const state = String(account?.stateName || '').toLowerCase();
  return ({ online: t('account.online'), working: t('account.loggingIn'), offline: t('account.offline'), expired: t('account.expired'), errored: t('account.error'), disabled: t('account.disabledStatus'), gone: t('account.gone'), unchecked: t('account.unchecked') } as any)[state] || (account?.stateName || t('account.unchecked'));
}
function cacheStateLabel(account: any) {
  const state = String(account?.cacheState || 'unknown');
  return ({ cached: t('account.cached'), 'not-cached': t('account.notCached'), unknown: t('account.checking') } as any)[state] || t('account.unknown');
}

async function attempt(action: () => Promise<any>) {
  error.value = '';
  try { return await action(); }
  catch (exception) { error.value = exception instanceof Error ? exception.message : String(exception); }
}

let selectionGeneration = 0;
let selectedSnapshotKey = '';
async function select(account: any) {
  const resolved = resolveAccount(account);
  const reference = accountRef(resolved || account);
  const generation = ++selectionGeneration;
  selectedSnapshotKey = accountSnapshotKey(resolved || account);
  selected.value = reference;
  launcherSession.selectAccount(reference);
  selectedAccount.value = resolved || account;
  profile.value = null;
  skins.value = [];
  skinNames.value = [];
  config.value = {
    authUrl: resolved?.yggdrasil?.authUrl || '', sessionUrl: resolved?.yggdrasil?.sessionUrl || '',
    sourceName: resolved?.yggdrasil?.sourceName || '', tokenType: resolved?.yggdrasil?.tokenType || 'Standard',
    authenticateEndpoint: resolved?.yggdrasil?.authenticateEndpoint || '', refreshEndpoint: resolved?.yggdrasil?.refreshEndpoint || '',
    validateEndpoint: resolved?.yggdrasil?.validateEndpoint || '', profileEndpoint: resolved?.yggdrasil?.profileEndpoint || '',
    oauthTokenEndpoint: resolved?.yggdrasil?.oauthTokenEndpoint || '',
    serverId: resolved?.unifiedPass?.serverId || '', baseUrl: resolved?.unifiedPass?.baseUrl || '',
  };
  error.value = '';
  try {
    const [nextProfile, nextSkins] = await Promise.all([
      props.execute('account.profile', { account: reference }),
      props.execute('account.skin.list', { account: reference }),
    ]);
    if (generation !== selectionGeneration || selected.value !== reference) return;
    profile.value = nextProfile;
    skins.value = Array.isArray(nextSkins) ? nextSkins : [];
    skinNames.value = skins.value.map((skin: any) => skin.name || skin.path);
  } catch (exception) {
    if (generation === selectionGeneration && selected.value === reference) error.value = exception instanceof Error ? exception.message : String(exception);
  }
}

// Account details are theme-local UI, but the selected account itself belongs
// to the shared runtime. Rehydrate the panel from Pinia after a theme switch
// or an account list refresh instead of falling back to the first row.
watch(() => {
  const preferred = props.accounts.find(account => accountRef(account) === launcherSession.selectedAccountId)
    || props.accounts.find(account => account.default)
    || props.accounts[0];
  return `${launcherSession.selectedAccountId}|${accountSnapshotKey(preferred)}`;
}, () => {
  const preferred = props.accounts.find(account => accountRef(account) === launcherSession.selectedAccountId)
    || props.accounts.find(account => account.default)
    || props.accounts[0];
  const preferredRef = accountRef(preferred);
  const preferredSnapshotKey = accountSnapshotKey(preferred);
  if (preferred && (selected.value !== preferredRef || selectedSnapshotKey !== preferredSnapshotKey)) void select(preferred);
  else if (!preferred) {
    selectionGeneration += 1;
    selectedSnapshotKey = '';
    selected.value = '';
    selectedAccount.value = null;
    profile.value = null;
    skins.value = [];
    skinNames.value = [];
  }
}, { immediate: true });

async function move(account: any, delta: number) { await attempt(async () => { await props.execute('account.move', { account: accountRef(account), delta }); emit('changed'); }); }
async function setDefault(account: any) { await attempt(async () => { await props.execute('account.set-default', { account: accountRef(account) }); launcherSession.selectAccount(accountRef(account)); emit('changed'); }); }
async function remove(account: any) { await attempt(async () => { await props.execute('account.remove', { account: accountRef(account), confirm: true }); selectionGeneration += 1; selectedSnapshotKey = ''; profile.value = null; skins.value = []; skinNames.value = []; selected.value = ''; selectedAccount.value = null; if (launcherSession.selectedAccountId === accountRef(account)) launcherSession.selectAccount(''); emit('changed'); }); }
async function chooseSkinFile() {
  const selected = await pickPclFile({ title: t('account.chooseSkin'), filters: [{ name: t('account.pngImage'), extensions: ['png'] }] });
  if (selected) skinPath.value = selected;
}

async function selectByReference(reference: string) {
  const account = resolveAccount(reference);
  if (account) await select(account);
}
async function installSkin() { await attempt(async () => { const reference = selected.value; await props.execute('account.skin.install', { account: reference, path: skinPath.value }); skinPath.value = ''; await selectByReference(reference); }); }
async function uploadSkin() { await attempt(async () => { const reference = selected.value; await props.execute('account.skin.upload', { account: reference, skin: skinName.value, variant: variant.value }); await selectByReference(reference); }); }
async function resetSkin() { await attempt(async () => { const reference = selected.value; await props.execute('account.skin.reset', { account: reference, confirm: true }); await selectByReference(reference); }); }
async function deleteSkin(skin: any) { await attempt(async () => { const reference = selected.value; await props.execute('account.skin.delete', { account: reference, skin: skin.name || skin.path, confirm: true }); await selectByReference(reference); }); }
async function chooseCape(cape: string) { await attempt(async () => { const reference = selected.value; await props.execute('account.cape.select', { account: reference, cape }); await selectByReference(reference); }); }
function selectCape(cape: any) { return chooseCape(cape.id); }
async function saveConfig() {
  await attempt(async () => {
    const payload: any = { account: selected.value };
    if (selectedAccount.value?.type === 'yggdrasil') {
      Object.assign(payload, {
        authUrl: config.value.authUrl, sessionUrl: config.value.sessionUrl, sourceName: config.value.sourceName,
        tokenType: config.value.tokenType, authenticateEndpoint: config.value.authenticateEndpoint,
        refreshEndpoint: config.value.refreshEndpoint, validateEndpoint: config.value.validateEndpoint,
        profileEndpoint: config.value.profileEndpoint, oauthTokenEndpoint: config.value.oauthTokenEndpoint,
      });
    } else {
      Object.assign(payload, { serverId: config.value.serverId, baseUrl: config.value.baseUrl });
    }
    await props.execute('account.configure', payload);
    emit('changed');
  });
}
</script>

<style scoped>
.pcl-account-list {
  border-top: 1px solid var(--pcl-border);
}

.pcl-account-row {
  display: flex;
  align-items: center;
  gap: 10px;
  min-height: 54px;
  padding: 5px 2px;
  border-bottom: 1px solid var(--pcl-border);
  cursor: pointer;
  transition: background .12s ease;
}

.pcl-account-row:hover,
.pcl-account-row.active {
  background: var(--pcl-c7);
}

.pcl-account-avatar {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 36px;
  width: 36px;
  height: 36px;
  overflow: hidden;
  border-radius: 4px;
  background: var(--pcl-c6);
  color: var(--pcl-c2);
  font-size: calc(14px * var(--pcl-font-scale, 1));
}


.pcl-account-copy {
  display: flex;
  flex-direction: column;
  min-width: 0;
  flex: 1;
}

.pcl-account-copy strong {
  overflow: hidden;
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  font-weight: 400;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-account-copy strong em {
  margin-left: 4px;
  padding: 1px 4px;
  border-radius: 3px;
  background: var(--pcl-c5);
  color: var(--pcl-c2);
  font-size: calc(10px * var(--pcl-font-scale, 1));
  font-style: normal;
}

.pcl-account-copy strong em.cached {
  background: rgba(52, 211, 153, .16);
  color: #34d399;
}

.pcl-account-copy small {
  margin-top: 2px;
  overflow: hidden;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-account-actions {
  display: flex;
  gap: 1px;
  opacity: .35;
  transition: opacity .12s ease;
}

.pcl-account-row:hover .pcl-account-actions,
.pcl-account-row.active .pcl-account-actions {
  opacity: 1;
}

.pcl-profile-card {
  margin-top: 14px;
}

.pcl-profile-state {
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-profile-grid {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
  gap: 18px;
}

.pcl-profile-skin {
  min-width: 0;
}

.pcl-skin-empty {
  display: grid;
  place-items: center;
  width: 100%;
  max-width: 220px;
  aspect-ratio: 1;
  margin: 2px 0 12px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 5px;
  background: var(--pcl-c8);
  color: var(--pcl-muted);
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-field {
  display: flex;
  flex-direction: column;
  gap: 4px;
  min-width: 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-field-row {
  display: flex;
  align-items: flex-end;
  gap: 7px;
}

.pcl-field-row .pcl-field {
  flex: 1;
}

.pcl-field--narrow {
  flex: 0 0 110px !important;
}

.pcl-profile-capes h3 {
  margin: 2px 0 7px;
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-profile-capes h3:not(:first-child) {
  margin-top: 16px;
}

.pcl-cape-current {
  color: #34d399;
}

.pcl-config-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 10px 14px;
  margin: 12px 0;
}

.pcl-empty--compact {
  min-height: 44px;
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

@media (max-width: 900px) {
  .pcl-profile-grid,
  .pcl-config-grid {
    grid-template-columns: 1fr;
  }
}
</style>
