<template>
  <div class="pcl-settings-section">
    <PclCard :title="t('about.title')"><div class="pcl-about-head"><div class="pcl-about-logo"><img :src="lunaLogo" alt="Luna Launcher" /></div><div><div class="pcl-about-name">{{ neoUiPreferences.pclTitleText || t('brand.name') }} <span>{{ t('brand.neoUi') }}</span></div><div class="pcl-about-version">{{ applicationVersion }} · {{ runtimeInfo.platform || 'Windows' }}</div></div></div><div class="pcl-about-grid"><span>{{ t('pcl.auto2.k9742cd9ffb') }}</span><code>{{ runtimeInfo.dataRoot || '—' }}</code><span>{{ t('pcl.auto2.k8c27cb90e0') }}</span><code>{{ runtimeInfo.root || runtimeInfo.programRoot || '—' }}</code><span>{{ t('pcl.auto2.k5068de9911') }}</span><code>{{ runtimeInfo.apiVersion || runtimeInfo.capabilities?.apiVersion || '1' }}</code><span>{{ t('pcl.auto2.keb9d53ce7f') }}</span><code>{{ (runtimeInfo.capabilities || []).join(' / ') || '—' }}</code></div><div class="pcl-actions"><PclButton icon="external-link" @click="openUrl(projectUrl)">{{ t('about.projectHome') }}</PclButton><PclButton icon="book-marked" @click="openUrl(docsUrl)">{{ t('about.docs') }}</PclButton></div></PclCard>
    <PclCard :title="t('about.services')"><p class="pcl-muted">{{ t('about.servicesDescription') }}</p><div class="pcl-actions"><PclButton icon="external-link" @click="openUrl(bmclApiUrl)">{{ t('about.bmclapi') }}</PclButton><PclButton icon="external-link" @click="openUrl(mcimUrl)">{{ t('about.mcim') }}</PclButton></div></PclCard>
    <PclCard :title="t('about.credits')"><PclListItem v-for="credit in credits" :key="credit.name" :title="credit.name" :info="credit.info" @click="openUrl(credit.url)"><template #icon><PclIcon name="heart-filled" :size="18" /></template><template #actions><PclButton size="sm" variant="ghost" icon="external-link" @click.stop="openUrl(credit.url)">{{ t('pcl.auto2.k65fc81e161') }}</PclButton></template></PclListItem></PclCard>
    <PclCard :title="t('about.contributors')"><div class="pcl-credit-tiles"><span v-for="name in contributors" :key="name">{{ name }}</span></div><p class="pcl-muted pcl-credit-note">{{ t('pcl.auto2.k1c55d41283') }}</p></PclCard>
    <PclCard :title="t('about.licenses')"><p class="pcl-muted">{{ t('about.licenseText') }}</p><p class="pcl-muted">{{ t('about.pclIconCredit') }}</p><div class="pcl-actions"><PclButton icon="file-box" @click="openUrl(licenseUrl)">{{ t('ui.licenseLabel') }}</PclButton><PclButton icon="file-box" @click="openUrl('https://github.com/PrismLauncher/PrismLauncher/blob/develop/LICENSE')">{{ t('pcl.auto2.k2185d95c14') }}</PclButton><PclButton icon="file-box" @click="openUrl('https://lucide.dev/license')">{{ t('about.lucideLicense') }}</PclButton></div></PclCard>
  </div>
</template>
<script setup lang="ts">
import { toRefs } from 'vue';
import PclCard from '../../components/PclCard.vue'; import PclButton from '../../components/PclButton.vue'; import PclListItem from '../../components/PclListItem.vue'; import PclIcon from '../../components/PclIcon.vue'; import lunaLogo from '../../../../assets/luna-launcher.png';
import { neoUiPreferences } from '../../../../neo-ui-preferences';
type Translator = (key: string, params?: Record<string, unknown>) => string;
const props = defineProps<{ t: Translator; runtimeInfo: any; applicationVersion: string; credits: any[]; contributors: string[]; openUrl: (url: string) => void | Promise<void>; projectUrl: string; licenseUrl: string; docsUrl: string; bmclApiUrl: string; mcimUrl: string }>();
// Keep parent-provided runtime information reactive. Destructuring the props
// object directly captures the initial empty runtime snapshot and prevents
// the about page from updating after runtime.info resolves.
const { t, runtimeInfo, applicationVersion, credits, contributors, openUrl, projectUrl, licenseUrl, docsUrl, bmclApiUrl, mcimUrl } = toRefs(props);
</script>
