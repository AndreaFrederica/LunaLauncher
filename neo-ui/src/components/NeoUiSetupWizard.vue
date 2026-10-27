<template>
  <q-dialog :model-value="true" persistent>
    <q-card class="neo-setup-card" :class="{ 'pcl-dialog': isPcl }" :data-setup-stage="stage">
      <q-card-section class="neo-setup-header">
        <div class="neo-setup-brand">
          <img :src="launcherLogo" alt="Luna Launcher" width="44" height="44" />
          <div>
            <div class="neo-setup-kicker">{{ t('ui.launcherKicker') }}</div>
            <h2>{{ t('neo.auto2.k69b10cf40d') }}</h2>
          </div>
        </div>
        <p>{{ t('neo.auto2.k966a3f4e3a') }}</p>
        <div class="neo-setup-progress" role="progressbar" :aria-label="t('neo.auto2.k442a1dd661')" :aria-valuenow="progress" aria-valuemin="0" aria-valuemax="100"><span :style="{ width: `${progress}%` }" /></div>
      </q-card-section>

      <q-card-section ref="body" class="neo-setup-body">
        <div v-if="working && working !== 'login'" class="setup-working">
          <PclLoading v-if="isPcl" inline :text="workingText" />
          <template v-else><q-spinner color="primary" size="22px" /><span>{{ workingText }}</span></template>
        </div>
        <div v-if="stage === 'welcome'">
          <h3>{{ t('neo.auto2.k5cf79a6153') }}</h3>
          <p class="muted">{{ t('neo.auto2.kf371671607') }}</p>
          <component :is="controls.hint" v-if="status?.dataRoot" v-bind="hintProps()" class="setup-banner">{{ t('neo.auto2.k8f4d395bcf') }}<code>{{ status.dataRoot }}</code>
          </component>
          <component :is="controls.hint" v-if="status?.portable" v-bind="hintProps()" class="setup-banner">{{ t('neo.auto2.k9439e91bb0') }}</component>
        </div>

        <div v-else-if="stage === 'migration'">
          <h3>{{ t('neo.auto2.kbf13dd0c07') }}</h3>
          <p class="muted">{{ t('neo.auto2.keb006543fb') }}</p>
          <div class="migration-list">
            <div v-for="item in migrations" :key="item.name" class="migration-item">
              <div class="migration-details">
                <strong>{{ item.name }}</strong>
                <div class="muted migration-path">{{ item.source }}</div>
                <div v-if="item.currentDataExists" class="setup-warning">{{ t('neo.auto2.k1c1702cbea') }}</div>
              </div>
              <div class="migration-actions">
                <component :is="controls.button" v-bind="actionProps(true, Boolean(working) || item.currentDataExists)" @click="migrate(item)">{{ t('neo.auto2.kbdf1cab1ec') }}</component>
                <component :is="controls.button" v-bind="actionProps()" @click="dismiss(item)">{{ t('neo.auto2.k31a98593f1') }}</component>
              </div>
            </div>
          </div>
          <component :is="controls.hint" v-if="!migrations.length" v-bind="hintProps()" class="setup-banner">{{ t('neo.auto2.kfbdad15688') }}</component>
        </div>

        <div v-else-if="stage === 'preferences'">
          <h3>{{ t('neo.auto2.ke6cffd0771') }}</h3>
          <p class="muted">{{ t('neo.auto2.k77b8fc36ba') }}</p>
          <fieldset class="setup-group">
            <legend>{{ t('neo.auto2.kdeb52f209a') }}</legend>
            <div class="setup-grid">
              <div class="setup-field">
                <span>{{ t('neo.auto2.kd5d8edd12c') }}</span>
                <component :is="controls.select" v-model="language" v-bind="selectProps" :options="languageOptions" :aria-label="t('neo.auto2.kd5d8edd12c')" />
              </div>
              <component :is="controls.checkbox" v-model="useSystemLocale" v-bind="checkboxProps" :label="t('neo.auto2.k726bce844f')" />
              <div class="setup-field" data-setup-field="preset">
                <span>{{ t('neo.auto2.k1ab7d5469d') }}</span>
                <component :is="controls.select" :model-value="preferences.pclStylePreset" v-bind="selectProps" :options="presetOptions" :aria-label="t('neo.auto2.k1ab7d5469d')" @update:model-value="selectPreset" />
              </div>
              <div class="setup-field" data-setup-field="color">
                <span>{{ t('neo.auto2.k24c9bd1492') }}</span>
                <component :is="controls.select" v-model="preferences.theme" v-bind="selectProps" :options="colorOptions" :aria-label="t('neo.auto2.k24c9bd1492')" />
              </div>
              <div class="setup-field">
                <span>{{ t('neo.auto2.k7a468db44b') }}</span>
                <component :is="controls.select" v-model="theme" v-bind="selectProps" :options="themeOptions" :aria-label="t('neo.auto2.k7a468db44b')" />
              </div>
              <div class="setup-field">
                <span>{{ t('neo.auto2.k052f629b3c') }}</span>
                <component :is="controls.select" v-model="icons" v-bind="selectProps" :options="iconOptions" :aria-label="t('neo.auto2.k052f629b3c')" />
              </div>
            </div>
          </fieldset>
          <fieldset class="setup-group">
            <legend>{{ t('neo.auto2.kcffec64a65') }}</legend>
            <div class="setup-grid">
              <component :is="controls.checkbox" v-model="automaticJavaSwitch" v-bind="checkboxProps" :label="t('neo.auto2.ke73929398f')" />
              <component :is="controls.checkbox" v-model="automaticJavaDownload" v-bind="checkboxProps" :disabled="Boolean(working) || status?.javaDownloaderEnabled === false" :disable="Boolean(working) || status?.javaDownloaderEnabled === false" :label="t('neo.auto2.k6422ab08e1')" />
              <div v-if="!automaticJavaDownload" class="setup-wide setup-java">
                <div class="setup-field">
                  <label for="setup-java-path">{{ t('neo.auto2.k638752651a') }}</label>
                  <div class="setup-java-path">
                    <component :is="controls.input" v-model="javaPath" v-bind="inputFieldProps('setup-java-path')" :placeholder="t('neo.auto2.kea1beb7760')" />
                    <component :is="controls.button" v-bind="actionProps()" @click="browseJava">{{ t('neo.auto2.kef23ea67b0') }}</component>
                    <component :is="controls.button" v-bind="actionProps()" @click="scanJava">{{ t('neo.auto2.k4b15abac06') }}</component>
                  </div>
                </div>
                <div v-if="javaOptions.length" class="setup-field">
                  <span>{{ t('neo.auto2.k3e720e6837') }}</span>
                  <component :is="controls.select" v-model="javaPath" v-bind="selectProps" :options="javaOptions" :aria-label="t('neo.auto2.k3e720e6837')" />
                </div>
                <component :is="controls.checkbox" v-model="ignoreJavaWizard" v-bind="checkboxProps" :label="t('neo.auto2.kb40b63cb74')" />
              </div>
              <div class="setup-field">
                <label for="setup-min-memory">{{ t('neo.auto2.ka6551c0898') }}</label>
                <component :is="controls.input" v-model.number="minMemory" v-bind="inputFieldProps('setup-min-memory')" type="number" min="128" step="128" />
              </div>
              <div class="setup-field">
                <label for="setup-max-memory">{{ t('neo.auto2.k14469bb314') }}</label>
                <component :is="controls.input" v-model.number="maxMemory" v-bind="inputFieldProps('setup-max-memory')" type="number" min="128" step="128" />
              </div>
            </div>
          </fieldset>
          <fieldset class="setup-group">
            <legend>{{ t('neo.auto2.k6be63dbd9f') }}</legend>
            <div class="setup-grid">
              <div class="setup-field">
                <span>{{ t('neo.auto2.k6c7edf8feb') }}</span>
                <component :is="controls.select" v-model="pasteType" v-bind="selectProps" :options="pasteOptions" :aria-label="t('neo.auto2.k6c7edf8feb')" />
              </div>
              <div v-if="pasteType === 0" class="setup-field">
                <label for="setup-paste-base">{{ t('neo.auto2.k0bb7028717') }}</label>
                <component :is="controls.input" v-model="pasteBase" v-bind="inputFieldProps('setup-paste-base')" :placeholder="t('neo.auto2.kf63c4935ca')" />
              </div>
            </div>
          </fieldset>
          <component :is="controls.hint" v-bind="hintProps()" class="setup-banner">{{ t('neo.auto2.kf23046e691') }}</component>
        </div>

        <div v-else-if="stage === 'account'">
          <h3>{{ t('neo.auto2.kc771c7d8e2') }}</h3>
          <p v-if="hasAccounts" class="muted">{{ t('neo.auto2.ka86cea3781') }}</p>
          <p v-else class="muted">{{ t('neo.auto2.ke909d953ac') }}</p>
          <component :is="controls.button" v-if="loginAvailable" class="setup-login-button" v-bind="actionProps(true)" @click="login">{{ t('neo.auto2.kc50b0a9ff0') }}</component>
          <component :is="controls.hint" v-else v-bind="hintProps()" class="setup-banner">{{ t('neo.auto2.kacb4c7e164') }}</component>
          <component :is="controls.hint" v-if="loginTask" v-bind="hintProps()" class="setup-banner">
            <div>{{ loginTask.status || t('neo.auto2.k4e2209e354') }}</div>
            <div v-if="loginTask.deviceCode" class="setup-device-code">{{ loginTask.deviceCode }}</div>
            <div v-if="loginTask.deviceUrl" class="setup-login-actions">
              <component :is="controls.button" v-bind="actionProps(false, false)" @click="openLoginUrl">{{ t('neo.auto2.k90fffc297f') }}</component>
              <component :is="controls.button" v-if="loginTask.deviceCode" v-bind="actionProps(false, false)" @click="copyLoginCode">{{ t('neo.auto2.k3b1fde4d23') }}</component>
            </div>
            <component :is="controls.button" v-bind="actionProps(false, false)" @click="cancelLogin">{{ t('neo.auto2.kf21bbc4ead') }}</component>
          </component>
        </div>

        <div v-else>
          <h3>{{ t('neo.auto2.kd3fc9c330f') }}</h3>
          <p class="muted">{{ t('neo.auto2.k21effdff86') }}</p>
          <div class="finish-icon"><PclIcon v-if="isPcl" name="circle-check" :size="56" /><q-icon v-else name="check_circle" size="56px" /></div>
        </div>

        <component :is="controls.hint" v-if="error" v-bind="hintProps('red')" class="setup-banner setup-error" role="alert">
          {{ error }}
          <component :is="controls.button" v-if="!preferencesLoaded" v-bind="actionProps()" @click="reloadPreferences">{{ t('neo.auto2.k26d6bbe5e4') }}</component>
        </component>
      </q-card-section>

      <q-card-actions align="between" class="neo-setup-actions">
        <component :is="controls.button" v-if="stage !== 'welcome'" v-bind="actionProps()" @click="back">{{ t('neo.auto2.k75ef1241c0') }}</component>
        <span v-else />
        <component :is="controls.button" v-if="stage !== 'finish'" data-setup-next v-bind="actionProps(true, Boolean(working) || !preferencesLoaded || (stage === 'migration' && migrations.length > 0))" @click="next">{{ nextLabel }}</component>
        <component :is="controls.button" v-else data-setup-next v-bind="actionProps(true)" @click="complete">{{ t('neo.auto2.k60ff133549') }}</component>
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>
<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { QBanner, QBtn, QInput, QSelect, QToggle } from 'quasar';
import { open } from '@tauri-apps/plugin-dialog';
import { applyPclStylePreset, neoUiIsDark, neoUiPreferences } from '../neo-ui-preferences';
import launcherLogo from '../assets/luna-launcher.png';
import PclButton from '../pcl/components/PclButton.vue';
import PclCheckbox from '../pcl/components/PclCheckbox.vue';
import PclHint from '../pcl/components/PclHint.vue';
import PclIcon from '../pcl/components/PclIcon.vue';
import PclInput from '../pcl/components/PclInput.vue';
import PclLoading from '../pcl/components/PclLoading.vue';
import PclSelect from '../pcl/components/PclSelect.vue';
import { setNeoLocale } from '../i18n';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
const props = defineProps<{ status: any; execute: Execute }>();
const emit = defineEmits<{ complete: []; changed: [] }>();

const stage = ref<'welcome' | 'migration' | 'preferences' | 'account' | 'finish'>('welcome');
const migrations = ref<any[]>([]);
const working = ref('loading');
const error = ref('');
const preferencesLoaded = ref(false);
const language = ref('en_US');
const useSystemLocale = ref(false);
const automaticJavaSwitch = ref(true);
const automaticJavaDownload = ref(true);
const javaPath = ref('');
const ignoreJavaWizard = ref(false);
const javaOptions = ref<any[]>([]);
const minMemory = ref(512);
const maxMemory = ref(4096);
const theme = ref('');
const icons = ref('');
const pasteType = ref(3);
const pasteBase = ref('');
const hasAccounts = ref(Boolean(props.status?.hasAccounts));
const loginAvailable = ref(Boolean(props.status?.loginAvailable));
const loginTask = ref<any>(null);
let disposed = false;
const preferences = neoUiPreferences;
const body = ref<{ $el: HTMLElement } | null>(null);
const isPcl = computed(() => preferences.shell === 'pcl');
const controls = computed(() => isPcl.value
  ? { button: PclButton, input: PclInput, select: PclSelect, checkbox: PclCheckbox, hint: PclHint }
  : { button: QBtn, input: QInput, select: QSelect, checkbox: QToggle, hint: QBanner });
const inputProps = computed(() => isPcl.value
  ? { disabled: Boolean(working.value) }
  : { disable: Boolean(working.value), dark: neoUiIsDark.value, outlined: true, dense: true });
const selectProps = computed(() => isPcl.value ? inputProps.value : { ...inputProps.value, emitValue: true, mapOptions: true });
const checkboxProps = computed(() => isPcl.value
  ? { disabled: Boolean(working.value) }
  : { disable: Boolean(working.value), dark: neoUiIsDark.value });
const workingText = computed(() => ({ loading: t('neo.auto2.k6303ccb47a'), saving: t('neo.auto2.k8adf00b5dd'), java: t('neo.auto2.k4ff1ce9568'), complete: t('neo.auto2.kc457158b2e') }[working.value] || t('neo.auto2.kc83917a9e7')));
function actionProps(primary = false, disabled = Boolean(working.value)) {
  return isPcl.value ? { variant: primary ? 'highlight' : 'ghost', disabled }
    : { color: primary ? 'primary' : undefined, unelevated: primary, flat: !primary, disable: disabled, noCaps: true };
}
function inputFieldProps(id: string) {
  return { ...inputProps.value, ...(isPcl.value ? { id } : { for: id }) };
}
function hintProps(theme: 'blue' | 'red' = 'blue') {
  return isPcl.value ? { theme } : { rounded: true };
}
function selectPreset(value: string | number) {
  if (value === 'luna-neo' || value === 'pcl') applyPclStylePreset(value);
}
watch(stage, async () => {
  await nextTick();
  body.value?.$el.scrollTo({ top: 0 });
});
watch(error, async value => {
  if (!value) return;
  await nextTick();
  body.value?.$el.querySelector('.setup-error')?.scrollIntoView({ block: 'nearest' });
});
const presetOptions = [{ label: t('neo.auto2.ka6dc467052'), value: 'luna-neo' }, { label: 'PCL', value: 'pcl' }];
const colorOptions = [{ label: t('neo.auto2.k30b2c979ac'), value: 'dark' }, { label: t('neo.auto2.k80ec9e2b1b'), value: 'light' }, { label: t('neo.auto2.kf4bbd91f79'), value: 'system' }];
const pasteOptions = [
  { label: '0x0.st', value: 0 },
  { label: 'Hastebin', value: 1 },
  { label: 'paste.gg', value: 2 },
  { label: 'mclo.gs', value: 3 },
];
const languageOptions = ref<any[]>([]);
const themeOptions = ref<any[]>([]);
const iconOptions = ref<any[]>([]);

const progress = computed(() => ({ welcome: 20, migration: 40, preferences: 60, account: 80, finish: 100 }[stage.value]));
const nextLabel = computed(() => stage.value === 'welcome' ? (migrations.value.length ? t('neo.auto2.k804f8384da') : t('neo.auto2.k279497bbd2')) : t('neo.auto2.k1fc1afc5c5'));

async function loadPreferences() {
    preferencesLoaded.value = false;
    const [settings, languageResult, appearance] = await Promise.all([
      props.execute('settings.list', { scope: 'launcher' }),
      props.execute('language.list'),
      props.execute('appearance.catalog'),
    ]);
    const setting = (key: string) => settings.find((item: any) => item.key === key)?.value;
    languageOptions.value = (languageResult?.languages || []).map((item: any) => ({ label: item.name || item.id, value: item.id }));
    language.value = languageResult?.selected || String(setting('Language') || 'en_US');
    useSystemLocale.value = Boolean(languageResult?.useSystemLocale ?? setting('UseSystemLocale'));
    themeOptions.value = (appearance?.themes || []).map((item: any) => ({ label: item.name || item.id, value: item.id }));
    iconOptions.value = (appearance?.icons || []).map((item: any) => ({ label: item.name || item.id, value: item.id }));
    const validSelection = (options: any[], value: string, fallback: string) =>
      options.find(item => item.value === value)?.value || options.find(item => item.value === fallback)?.value || options[0]?.value || '';
    theme.value = validSelection(themeOptions.value, appearance?.selected?.theme || String(setting('ApplicationTheme') || ''), 'dark');
    icons.value = validSelection(iconOptions.value, appearance?.selected?.icons || String(setting('IconTheme') || ''), 'pe_colored');
  automaticJavaSwitch.value = Boolean(setting('AutomaticJavaSwitch') ?? true);
  automaticJavaDownload.value = props.status?.javaDownloaderEnabled !== false && Boolean(setting('AutomaticJavaDownload') ?? true);
  javaPath.value = String(setting('JavaPath') || '');
  ignoreJavaWizard.value = Boolean(setting('IgnoreJavaWizard'));
  minMemory.value = Number(setting('MinMemAlloc') || 512);
  maxMemory.value = Number(setting('MaxMemAlloc') || 4096);
  pasteType.value = Number(setting('PastebinType') ?? 3);
  pasteBase.value = String(setting('PastebinCustomAPIBase') || '');
  // Classic's paste-service page upgrades the old URL setting too.
  const oldPasteUrl = String(setting('PastebinURL') || '');
  if (oldPasteUrl && oldPasteUrl !== 'https://0x0.st') { pasteType.value = 0; pasteBase.value = oldPasteUrl; }
  preferencesLoaded.value = true;
}

async function load() {
  migrations.value = Array.isArray(props.status?.migrations) ? [...props.status.migrations] : [];
  await reloadPreferences();
}

async function reloadPreferences() {
  working.value = 'loading'; error.value = '';
  try { await loadPreferences(); }
  catch (exception) { error.value = String(exception); }
  finally { working.value = ''; }
}

async function migrate(item: any) {
  working.value = item.name;
  error.value = '';
  try {
    const result = await props.execute('startup.migrate', { name: item.name });
    migrations.value = Array.isArray(result?.status?.migrations)
      ? result.status.migrations
      : migrations.value.filter(candidate => candidate.name !== item.name);
    hasAccounts.value = Boolean(result?.status?.hasAccounts);
    loginAvailable.value = Boolean(result?.status?.loginAvailable);
    await loadPreferences();
    emit('changed');
  } catch (exception) {
    error.value = String(exception);
  } finally {
    working.value = '';
  }
}

async function dismiss(item: any) {
  working.value = item.name;
  error.value = '';
  try {
    const result = await props.execute('startup.dismiss-migration', { name: item.name });
    migrations.value = Array.isArray(result?.status?.migrations)
      ? result.status.migrations
      : migrations.value.filter(candidate => candidate.name !== item.name);
    emit('changed');
  } catch (exception) {
    error.value = String(exception);
  } finally {
    working.value = '';
  }
}

async function saveSetting(key: string, value: unknown) {
  await props.execute('settings.set', { scope: 'launcher', key, value });
}

async function savePreferences() {
    const minimum = Number(minMemory.value), maximum = Number(maxMemory.value);
    if (!Number.isInteger(minimum) || !Number.isInteger(maximum) || minimum < 128 || maximum < minimum)
      throw new Error(t('neo.extra.setupMemory'));
    if (!automaticJavaDownload.value && !ignoreJavaWizard.value) {
      if (!javaPath.value.trim()) throw new Error(t('neo.extra.chooseJava'));
      const diagnostic = await props.execute('java.diagnose', { scope: 'launcher', path: javaPath.value.trim() });
      if (!diagnostic?.usable) throw new Error(t('neo.extra.javaUnavailable'));
      await props.execute('java.select', { scope: 'launcher', path: javaPath.value.trim() });
    }
    await saveSetting('AutomaticJavaSwitch', automaticJavaSwitch.value);
    await saveSetting('AutomaticJavaDownload', automaticJavaDownload.value);
    await saveSetting('IgnoreJavaWizard', ignoreJavaWizard.value);
    await saveSetting('UserAskedAboutAutomaticJavaDownload', true);
    await saveSetting('MinMemAlloc', minimum);
    await saveSetting('MaxMemAlloc', maximum);
    await saveSetting('PastebinType', Number(pasteType.value));
    await saveSetting('PastebinCustomAPIBase', pasteBase.value.trim());
    await props.execute('settings.reset', { scope: 'launcher', key: 'PastebinURL' });
    await props.execute('language.select', { language: language.value, useSystemLocale: useSystemLocale.value });
    setNeoLocale(useSystemLocale.value ? navigator.language : language.value);
    if (theme.value || icons.value) await props.execute('appearance.select', { ...(theme.value ? { theme: theme.value } : {}), ...(icons.value ? { icons: icons.value } : {}) });
    emit('changed');
}

async function next() {
  error.value = '';
  if (stage.value === 'welcome') {
    stage.value = migrations.value.length ? 'migration' : 'preferences';
  } else if (stage.value === 'migration') {
    if (migrations.value.length) return;
    stage.value = 'preferences';
  } else if (stage.value === 'preferences') {
    working.value = 'saving';
    try { await savePreferences(); stage.value = 'account'; }
    catch (exception) { error.value = String(exception); }
    finally { working.value = ''; }
  } else if (stage.value === 'account') {
    stage.value = 'finish';
  }
}

function back() {
  if (stage.value === 'migration') stage.value = 'welcome';
  else if (stage.value === 'preferences') stage.value = migrations.value.length ? 'migration' : 'welcome';
  else if (stage.value === 'account') stage.value = 'preferences';
  else if (stage.value === 'finish') stage.value = 'account';
}

async function browseJava() {
  try {
    const path = await open({ title: t('neo.extra.chooseJavaExecutable'), multiple: false, directory: false });
    if (typeof path === 'string') javaPath.value = path;
  } catch (exception) { error.value = String(exception); }
}

async function scanJava() {
  working.value = 'java'; error.value = '';
  try {
    const installed = await props.execute('java.refresh');
    javaOptions.value = installed.map((item: any) => ({ label: `${item.version || 'Java'} · ${item.path}`, value: item.path }));
    if (!javaPath.value && javaOptions.value.length) javaPath.value = javaOptions.value[0].value;
    if (!javaOptions.value.length) error.value = t('neo.extra.noJavaDetected');
  } catch (exception) { error.value = String(exception); }
  finally { working.value = ''; }
}

async function login() {
  working.value = 'login'; error.value = '';
  try {
    loginTask.value = await props.execute('neo.task.start', { operation: 'account.login', priority: 'high', parameters: { type: 'microsoft', useDeviceCode: true } });
    if (disposed) { await cancelLogin(); return; }
    while (!disposed && loginTask.value && !loginTask.value.finished) {
      await new Promise(resolve => setTimeout(resolve, 400));
      if (!disposed) loginTask.value = await props.execute('neo.task.status', { taskId: loginTask.value.taskId });
    }
    if (disposed) return;
    if (loginTask.value?.state !== 'succeeded') throw new Error(loginTask.value?.error || t('neo.extra.loginFailed'));
    hasAccounts.value = true;
    emit('changed');
  } catch (exception) { error.value = String(exception); }
  finally { loginTask.value = null; working.value = ''; }
}

async function cancelLogin() {
  try { if (loginTask.value?.taskId) await props.execute('neo.task.cancel', { taskId: loginTask.value.taskId }); }
  catch (exception) { error.value = String(exception); }
}
async function openLoginUrl() {
  try { if (loginTask.value?.deviceUrl) await props.execute('desktop.open-url', { url: loginTask.value.deviceUrl }); }
  catch (exception) { error.value = String(exception); }
}
async function copyLoginCode() {
  try { await navigator.clipboard.writeText(loginTask.value?.deviceCode || ''); }
  catch (exception) { error.value = String(exception); }
}

async function complete() {
  working.value = 'complete';
  error.value = '';
  try {
    await props.execute('startup.complete');
    emit('complete');
  } catch (exception) {
    error.value = String(exception);
  } finally {
    working.value = '';
  }
}

onMounted(load);
onBeforeUnmount(() => { disposed = true; void cancelLogin(); });
</script>

<style scoped>
.neo-setup-card {
  --setup-bg: var(--neo-panel);
  --setup-text: var(--neo-text);
  --setup-muted: var(--neo-muted);
  --setup-border: var(--neo-border);
  --setup-accent: var(--neo-primary);
  display: flex;
  flex-direction: column;
  width: 760px;
  max-width: 94vw;
  max-height: 94vh;
  overflow: hidden;
  color: var(--setup-text);
  background: var(--setup-bg);
  border: 1px solid var(--setup-border);
  border-radius: 18px;
}
.neo-setup-card.pcl-dialog {
  --setup-bg: var(--pcl-white);
  --setup-text: var(--pcl-text);
  --setup-muted: var(--pcl-muted);
  --setup-border: var(--pcl-border);
  --setup-accent: var(--pcl-c3);
  font-family: var(--pcl-font);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  border-radius: var(--pcl-radius);
  box-shadow: var(--pcl-shadow);
  overflow: hidden;
}
.neo-setup-header { padding: 24px 28px 20px; background: linear-gradient(135deg, color-mix(in srgb, var(--setup-accent) 12%, transparent), transparent); border-bottom: 1px solid var(--setup-border); }
.neo-setup-brand { display: flex; align-items: center; gap: 14px; margin-bottom: 12px; }
.neo-setup-brand img { flex-shrink: 0; object-fit: contain; }
.neo-setup-kicker { color: var(--setup-accent); font-size: .8em; letter-spacing: .14em; font-weight: 700; }
.neo-setup-header h2 { margin: 4px 0 0; color: var(--setup-text); font-size: 24px; font-weight: 600; line-height: 1.4; }
.neo-setup-header p, .muted { margin: 0; color: var(--setup-muted); line-height: 1.7; }
.neo-setup-progress { height: 3px; margin-top: 18px; overflow: hidden; border-radius: 3px; background: color-mix(in srgb, var(--setup-accent) 16%, transparent); }
.neo-setup-progress span { display: block; height: 100%; background: var(--setup-accent); transition: width .25s ease; }
.neo-setup-body { min-height: 0; overflow-y: auto; overflow-x: hidden; flex: 1 1 auto; padding: 24px 28px; }
.neo-setup-header, .neo-setup-actions { flex-shrink: 0; }
.neo-setup-body h3 { margin: 0 0 10px; font-size: 21px; line-height: 1.4; }
.setup-working { display: flex; align-items: center; gap: 8px; margin-bottom: 16px; color: var(--setup-accent); }
.setup-banner { margin-top: 18px; overflow-wrap: anywhere; }
.setup-banner:not(.pcl-hint) { background: color-mix(in srgb, var(--setup-accent) 10%, var(--setup-bg)); color: var(--setup-text); border-left: 3px solid var(--setup-accent); }
.setup-error:not(.pcl-hint) { color: var(--q-negative); border-left-color: var(--q-negative); background: color-mix(in srgb, var(--q-negative) 10%, var(--setup-bg)); }
.setup-banner code { overflow-wrap: anywhere; }
.migration-list { margin-top: 20px; border: 1px solid var(--setup-border); border-radius: var(--pcl-radius, 10px); }
.migration-item { display: flex; align-items: center; gap: 16px; padding: 16px; }
.migration-item + .migration-item { border-top: 1px solid var(--setup-border); }
.migration-details { flex: 1; min-width: 0; }
.migration-path { overflow-wrap: anywhere; font-size: .9em; }
.migration-actions { display: flex; align-items: center; flex-shrink: 0; gap: 8px; }
.setup-warning { color: var(--pcl-warning, var(--q-warning)); line-height: 1.7; }
.setup-group { min-width: 0; margin: 24px 0 0; padding: 0; border: 0; }
.setup-group legend { width: 100%; padding: 0 0 8px; border-bottom: 1px solid var(--setup-border); color: var(--setup-accent); font-size: 1em; font-weight: 600; }
.setup-grid { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); align-items: center; gap: 16px; margin-top: 16px; }
.setup-field { display: flex; flex-direction: column; min-width: 0; gap: 8px; }
.setup-wide { grid-column: 1 / -1; }
.setup-java { display: flex; flex-direction: column; gap: 16px; }
.setup-java-path { display: flex; align-items: center; flex-wrap: wrap; gap: 8px; }
.setup-java-path > :first-child { flex: 1 1 200px; min-width: 0; }
.setup-device-code { margin: 18px 0; font: 600 26px monospace; letter-spacing: .12em; }
.setup-login-button { margin-top: 24px; }
.setup-login-actions { display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 8px; }
.finish-icon { display: flex; justify-content: center; margin: 40px auto 20px; color: var(--setup-accent); }
.neo-setup-actions { gap: 12px; padding: 16px 28px; border-top: 1px solid var(--setup-border); background: var(--setup-bg); }
@media (max-width: 640px) {
  .setup-grid { grid-template-columns: minmax(0, 1fr); }
  .neo-setup-body { padding: 20px; }
  .neo-setup-header { padding: 20px 20px 16px; }
  .neo-setup-actions { padding: 16px 20px; }
  .migration-item { align-items: flex-start; flex-direction: column; }
}
</style>
