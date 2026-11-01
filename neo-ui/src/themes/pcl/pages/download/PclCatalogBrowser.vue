<template>
  <div ref="catalogRoot" class="pcl-catalog">
    <div class="pcl-catalog-toolbar">
      <PclSelect v-model="provider" :options="providerOptions" :disabled="busy || searching" @update:model-value="changeProvider" />
      <PclInput v-model="query" :placeholder="t('pcl.auto2.kb617f05c84')" @enter="search(0)" />
      <PclInput v-if="packs && provider === 'legacy-ftb'" v-model="privateCode" :placeholder="t('pcl.auto2.k0b67abdebc')" @enter="search(0)" />
      <PclButton variant="highlight" icon="search" :disabled="busy || searching" @click="search(0)">{{ t('pcl.auto2.kf04090805c') }}</PclButton>
    </div>
    <div v-if="!packs" class="pcl-catalog-filters">
      <PclInput v-model="gameVersion" :placeholder="t('pcl.auto2.kff0b86d5da')" />
      <PclSelect v-model="loader" :options="loaderOptions" />
      <PclSelect v-model="sort" :options="sortOptions" />
      <PclButton variant="ghost" icon="rotate-ccw" :disabled="busy" @click="resetFilters">{{ t('pcl.auto2.k3d81345303') }}</PclButton>
    </div>

    <div v-if="error" class="pcl-catalog-error">
      <PclIcon name="info" :size="15" />
      <span>{{ error }}</span>
      <button type="button" @click="error = ''">{{ t('pcl.auto2.k6c14bd7f6f') }}</button>
    </div>
    <div class="pcl-catalog-loading" :class="{ 'is-active': searching || detailLoading || installing || cancelling }" aria-live="polite">
      <PclLoading inline />
      <PclButton
        v-if="searching || detailLoading || cancelling"
        size="sm"
        variant="ghost"
        icon="x"
        :disabled="cancelling"
        @click="cancelLoading"
      >
        {{ t('common.cancel') }}
      </PclButton>
    </div>

          <!-- Keep the search result rows mounted. The selected project is
               expanded directly after its row by PclCatalogResultsView so
               opening a mod does not jump the catalog to its top. -->
          <PclCatalogResultsView />
  </div>
</template>
<script setup lang="ts">
import { useI18n } from "vue-i18n";
const { t } = useI18n();
import { computed, nextTick, onMounted, provide, reactive, ref, toRef, watch } from 'vue';
import PclButton from '../../components/PclButton.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import PclInput from '../../components/PclInput.vue';
import PclSelect from '../../components/PclSelect.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclCatalogResultList from './PclCatalogResultList.vue';
import { PclCatalogContextKey } from './PclCatalogContext';
import PclCatalogResultsView from './PclCatalogResultsView.vue';
import { isTaskItemFinished } from '../../composables/useTaskProgress';

type Execute=(op:string,p?:Record<string,unknown>)=>Promise<any>;
const props=defineProps<{packs?:boolean;instance?:string;kind?:string;minecraftVersion?:string;loader?:string;busy:boolean;execute:Execute;defaultProvider?:string;tag?:string;autoRefresh?:boolean;refreshToken?:number;active?:boolean}>();
const emit=defineEmits<{installed:[result:any]; 'install-start':[info:any]; 'install-progress':[task:any]; 'install-done':[info:any]}>();
const providers = ref<string[]>(props.packs ? ['modrinth','curseforge','atlauncher','ftb','technic','legacy-ftb'] : ['modrinth','curseforge']);
const providerCatalog = ref<any[]>([]);
const catalogRoot = ref<HTMLElement | null>(null);
const provider=ref(props.defaultProvider||'modrinth'),query=ref<any>(''),privateCode=ref<any>(''),results=ref<any[]>([]),project=ref<any>(null),versions=ref<any[]>([]),version=ref<any>(''),error=ref<any>(''),gameVersion=ref(props.minecraftVersion||''),loader=ref(props.loader||''),sort=ref<any>('relevance'),optional=ref(false),instanceName=ref<any>(''),offset=ref(0),searched=ref(false),searching=ref(false),detailLoading=ref(false),installing=ref(false),cancelling=ref(false),projectTab=ref<'overview'|'versions'>('overview'),favorite=ref(false);
const selectedProjects = reactive<Record<string, any>>({});
const selectedProjectsList = computed(() => Object.values(selectedProjects));
const batchConfirmOpen = ref(false);
const autoRefreshSuppressed = ref(false);
let searchGeneration = 0;
let selectionGeneration = 0;
// The selected instance is only the installation target; changing it must not
// clear and rebuild the visible catalog results. Provider/kind changes still
// reset the query because they change the catalog itself.
watch(()=>[props.kind,props.packs],()=>reset());watch(()=>props.defaultProvider,v=>{if(v){provider.value=v;reset();}});watch(()=>[props.minecraftVersion,props.loader],()=>{gameVersion.value=props.minecraftVersion||'';loader.value=props.loader||'';});
function providerLabel(v:string){return ({modrinth:'Modrinth',curseforge:'CurseForge',atlauncher:'ATLauncher',ftb:'FTB',technic:'Technic','legacy-ftb':'Legacy FTB',hangar:'Hangar'} as Record<string,string>)[v]||(v.startsWith('js:')?`JS · ${v.slice(3)}`:v);}
function clearSelectedProjects() { for (const key of Object.keys(selectedProjects)) delete selectedProjects[key]; batchConfirmOpen.value = false; }
function reset(clearResults = true){searchGeneration += 1; selectionGeneration += 1; clearSelectedProjects(); if(clearResults){results.value=[];searched.value=false;offset.value=0;}project.value=null;versions.value=[];version.value='';error.value='';detailLoading.value=false;projectTab.value='overview';gameVersion.value=props.minecraftVersion||'';loader.value=props.loader||'';}
function resetFilters(){query.value='';gameVersion.value=props.minecraftVersion||'';loader.value=props.loader||'';sort.value=sortOptions.value[0]?.value || '';reset();}
const providerOptions = computed(() => providers.value.map(value => ({ label: providerLabel(value), value })));
const sortOptions = computed(() => {
  if (props.packs) return [];
  const methods = providerCatalog.value.find(entry => String(entry?.id) === provider.value)?.sorts;
  if (Array.isArray(methods) && methods.length) return methods.map((entry: any) => ({ label: String(entry.name || entry.id), value: String(entry.id) }));
  return [
    { label: t('pcl.auto2.k0e1b55305e'), value: 'relevance' },
    { label: t('pcl.auto2.k2a6c7441d8'), value: 'downloads' },
    { label: t('pcl.auto2.k7e805a1230'), value: 'newest' },
    { label: t('pcl.auto2.k80920cd667'), value: 'updated' },
  ];
});
function changeProvider() {
  if (!sortOptions.value.some(option => option.value === sort.value)) sort.value = sortOptions.value[0]?.value || '';
  reset();
}
async function loadProviders() {
  try {
    const result: any = await props.execute('resource.providers');
    const list = Array.isArray(result) ? result : [];
    providerCatalog.value = list;
    const ids = list
      .filter((entry: any) => Array.isArray(entry.kinds) && entry.kinds.includes(props.packs ? 'modpacks' : (props.kind || 'mods')))
      .map((entry: any) => String(entry.id));
    const staticIds = props.packs ? ['modrinth', 'curseforge', 'atlauncher', 'ftb', 'technic', 'legacy-ftb'] : ['modrinth', 'curseforge'];
    const availableIds = [...new Set([...staticIds, ...ids])];
    if (availableIds.length) {
      providers.value = availableIds;
      if (!availableIds.includes(provider.value)) provider.value = availableIds[0];
      if (!sortOptions.value.some(option => option.value === sort.value)) sort.value = sortOptions.value[0]?.value || '';
    }
  } catch {
    // Static providers remain available if dynamic discovery fails.
  }
}
onMounted(async () => {
  if (props.active === false) return;
  await loadProviders();
  if (props.autoRefresh !== false && !props.busy) void search(0);
});
watch(() => props.busy, (busy, previous) => {
  if (props.active !== false && !busy && previous && props.autoRefresh !== false && !searched.value && !autoRefreshSuppressed.value) void search(0);
});
watch(() => props.autoRefresh, (enabled, previous) => {
  if (props.active !== false && enabled && !previous && !searched.value && !props.busy && !autoRefreshSuppressed.value) void search(0);
});
watch(() => props.refreshToken, token => {
  if (token === undefined) return;
  reset(false);
  if (props.active !== false && props.autoRefresh !== false && !props.busy) void search(0);
});
watch(() => props.kind, () => { if (props.active !== false) void loadProviders(); });

const loaderOptions = [
  { label: t('pcl.auto2.k93bd6737dc'), value: '' },
  { label: 'Forge', value: 'forge' },
  { label: 'NeoForge', value: 'neoforge' },
  { label: 'Fabric', value: 'fabric' },
  { label: 'Quilt', value: 'quilt' },
];
const versionOptions = computed(() => versions.value.map(v => ({ label: versionLabel(v), value: v.versionId })));
const renderedBody = computed(() => renderMarkdown(String(project.value?.body || project.value?.description || t('pcl.auto2.kcbbd8f733d'))));
const providerMeta = computed(() => providerCatalog.value.find(entry => String(entry?.id) === provider.value) || null);

const allowedTags = new Set(['a', 'p', 'br', 'strong', 'em', 'del', 'code', 'pre', 'ul', 'ol', 'li', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'blockquote', 'hr', 'img', 'table', 'thead', 'tbody', 'tr', 'td', 'th', 'div', 'span']);
const allowedAttributes: Record<string, string[]> = {
  a: ['href', 'title'],
  img: ['src', 'alt', 'title', 'width', 'height'],
  td: ['colspan', 'rowspan'],
  th: ['colspan', 'rowspan'],
  p: ['align'],
  div: ['align'],
};

function escapeHtml(value: string) {
  return value.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

function safeUrl(value: string) {
  const url = String(value || '').replace(/&amp;/g, '&').trim();
  return /^(https?:|mailto:)/i.test(url) || /^data:image\//i.test(url) ? escapeHtml(url) : '#';
}

function attribute(tag: string, name: string) {
  const match = tag.match(new RegExp(name + String.raw`\s*=\s*("([^"]*)"|'([^']*)'|([^\s>]+))`, 'i'));
  return match ? (match[2] ?? match[3] ?? match[4] ?? '') : '';
}

function normalizeRawHtml(value: string) {
  let text = value;
  text = text.replace(/<img\b[^>]*>/gi, tag => {
    const src = attribute(tag, 'src');
    return src ? `![${attribute(tag, 'alt')}](${src})` : '';
  });
  text = text.replace(/<a\b[^>]*href\s*=\s*("([^"]*)"|'([^']*)')[^>]*>([\s\S]*?)<\/a>/gi, (_all, _quoted, doubleQuoted, singleQuoted, label) => {
    return `[${String(label).replace(/<[^>]+>/g, '')}](${doubleQuoted || singleQuoted || ''})`;
  });
  text = text.replace(/<br\s*\/?>/gi, '\n');
  text = text.replace(/<\/?(p|div|center|span|strong|em|code|pre|ul|ol|li|h[1-6]|blockquote|table|thead|tbody|tr|td|th)\b[^>]*>/gi, '\n');
  return text.replace(/<[^>]+>/g, '');
}

function inlineMarkdown(value: string) {
  let text = escapeHtml(value);
  const code: string[] = [];
  text = text.replace(/`([^`]+)`/g, (_all, body) => {
    code.push(`<code>${body}</code>`);
    return `@@INLINE${code.length - 1}@@`;
  });
  text = text.replace(/!\[([^\]]*)\]\((https?:\/\/[^)\s]+|data:image\/[^)\s]+)\)/g, (_all, alt, url) => {
    return `<img src="${safeUrl(url)}" alt="${alt}" loading="lazy" />`;
  });
  text = text.replace(/\[([^\]]+)\]\((https?:\/\/[^)\s]+|mailto:[^)\s]+)\)/g, (_all, label, url) => {
    return `<a href="${safeUrl(url)}" target="_blank" rel="noopener noreferrer">${label}</a>`;
  });
  text = text.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
  text = text.replace(/(^|[^*])\*([^*]+)\*/g, '$1<em>$2</em>');
  text = text.replace(/~~([^~]+)~~/g, '<del>$1</del>');
  return text.replace(/@@INLINE(\d+)@@/g, (_all, index) => code[Number(index)] || '');
}

function renderMarkdown(value: string) {
  const codeBlocks: string[] = [];
  let text = String(value || '').replace(/\r\n/g, '\n');
  text = text.replace(/```[^\n]*\n([\s\S]*?)```/g, (_all, code) => {
    codeBlocks.push(`<pre><code>${escapeHtml(code)}</code></pre>`);
    return `\n@@CODE${codeBlocks.length - 1}@@\n`;
  });
  text = normalizeRawHtml(text);
  const lines = text.split('\n');
  const html: string[] = [];
  let paragraph: string[] = [];
  let list = '';
  const flushParagraph = () => {
    if (paragraph.length) html.push(`<p>${inlineMarkdown(paragraph.join(' '))}</p>`);
    paragraph = [];
  };
  const closeList = () => {
    if (list) html.push(`</${list}>`);
    list = '';
  };
  for (const line of lines) {
    const trimmed = line.trim();
    if (!trimmed) {
      flushParagraph();
      closeList();
      continue;
    }
    const code = trimmed.match(/^@@CODE(\d+)@@$/);
    if (code) {
      flushParagraph();
      closeList();
      html.push(codeBlocks[Number(code[1])] || '');
      continue;
    }
    const heading = trimmed.match(/^(#{1,6})\s+(.*)$/);
    if (heading) {
      flushParagraph();
      closeList();
      const level = heading[1].length;
      html.push(`<h${level}>${inlineMarkdown(heading[2])}</h${level}>`);
      continue;
    }
    if (/^([-*_])\1{2,}$/.test(trimmed)) {
      flushParagraph();
      closeList();
      html.push('<hr />');
      continue;
    }
    const quote = trimmed.match(/^>\s?(.*)$/);
    if (quote) {
      flushParagraph();
      closeList();
      html.push(`<blockquote>${inlineMarkdown(quote[1])}</blockquote>`);
      continue;
    }
    const unordered = trimmed.match(/^[-*+]\s+(.*)$/);
    const ordered = trimmed.match(/^\d+[.)]\s+(.*)$/);
    if (unordered || ordered) {
      flushParagraph();
      const nextList = unordered ? 'ul' : 'ol';
      if (list !== nextList) {
        closeList();
        html.push(`<${nextList}>`);
        list = nextList;
      }
      html.push(`<li>${inlineMarkdown((unordered || ordered)![1])}</li>`);
      continue;
    }
    paragraph.push(trimmed);
  }
  flushParagraph();
  closeList();
  return sanitizeHtml(html.join('\n'));
}

function sanitizeHtml(value: string) {
  const document = new DOMParser().parseFromString(`<div>${value}</div>`, 'text/html');
  const root = document.body.firstElementChild;
  if (!root) return '';
  const walk = (node: Element) => {
    for (const child of Array.from(node.children)) {
      const tag = child.tagName.toLowerCase();
      if (!allowedTags.has(tag)) {
        child.remove();
        continue;
      }
      for (const attr of Array.from(child.attributes)) {
        const name = attr.name.toLowerCase();
        if (name.startsWith('on') || name === 'style' || !(allowedAttributes[tag] || []).includes(name)) child.removeAttribute(attr.name);
      }
      if (tag === 'a') {
        child.setAttribute('target', '_blank');
        child.setAttribute('rel', 'noopener noreferrer');
      }
      if (tag === 'img') {
        const src = child.getAttribute('src') || '';
        if (!/^(https?:|data:image\/)/i.test(src)) child.remove();
        else child.setAttribute('loading', 'lazy');
      }
      walk(child);
    }
  };
  walk(root);
  return root.innerHTML;
}

function normalize(v:any){const id=String(v?.projectId||v?.slug||v?.id||v?.url||'');const icon=v?.iconUrl||v?.icon||v?.logoUrl||v?.logo?.url||'';return {...v,projectId:id,provider:String(v?.provider||provider.value),name:v?.displayName||v?.name||v?.title||v?.display_name||id,description:v?.description||v?.synopsis||v?.summary||'',iconUrl:icon};}
function params(providerOverride = provider.value){const p:any={provider:providerOverride};if(props.packs){if(privateCode.value.trim())p.privateCode=privateCode.value.trim();}else{p.kind=props.kind||'mods';if(gameVersion.value)p.minecraftVersion=gameVersion.value;if(loader.value)p.loaders=[loader.value];if(sort.value!=='relevance')p.sort=sort.value;}return p;}
function versionLabel(v:any){const game=Array.isArray(v.minecraftVersions)?v.minecraftVersions.slice(0,2).join(', '):(v.minecraftVersion||'');return `${v.name||v.version||v.versionId}${game?' · '+game:''}`;}
function scrollCatalogToTop() {
  void nextTick(() => {
    let node = catalogRoot.value?.parentElement as HTMLElement | null;
    while (node && node !== document.body) {
      const style = getComputedStyle(node);
      if (/(auto|scroll)/.test(style.overflowY) && node.scrollHeight > node.clientHeight) {
        node.scrollTo({ top: 0, behavior: 'smooth' });
        return;
      }
      node = node.parentElement;
    }
    window.scrollTo({ top: 0, behavior: 'smooth' });
  });
}
async function search(start:number){if(props.busy || searching.value || cancelling.value)return;autoRefreshSuppressed.value=false;const generation=++searchGeneration;searching.value=true;error.value='';if(start===0)clearSelectedProjects();try{const text=[query.value,props.tag&&props.tag!==t('pcl.auto2.k778fc8f994')?props.tag:''].filter(Boolean).join(' ');const aggregateMods=!props.packs&&props.kind==='mods';const operation=props.packs?'modpack.search':aggregateMods?'resource.search.v2':'resource.search';const request=aggregateMods?{...params(),providers:['modrinth','curseforge'],query:text,offset:start}:{...params(),query:text,offset:start};const r=await props.execute(operation,request);if(generation!==searchGeneration)return;const list=Array.isArray(r)?r:(r?.results||r?.projects||r?.modpacks||[]);results.value=list.map(normalize);offset.value=start;searched.value=true;project.value=null;scrollCatalogToTop();}catch(e){if(generation===searchGeneration)error.value=String(e);}finally{if(generation===searchGeneration)searching.value=false;}}
async function cancelLoading(){
  if ((!searching.value && !detailLoading.value) || cancelling.value) return;
  // Invalidate the current request immediately so a late response cannot
  // replace the catalog after the user has cancelled it.
  searchGeneration += 1;
  selectionGeneration += 1;
  autoRefreshSuppressed.value = true;
  searching.value = false;
  detailLoading.value = false;
  cancelling.value = true;
  try {
    // resource.search/resource.project/resource.versions are synchronous API
    // operations, but the launcher tracks their network Task and accepts a
    // concurrent task.cancel request without waiting for the operation result.
    await props.execute('task.cancel', {});
  } catch {
    // The request may have completed between the click and task.cancel.
  } finally {
    cancelling.value = false;
  }
}
function favoriteKey(item:any){return `${item?.provider||provider.value}:${props.kind || 'modpack'}:${item.projectId}`;}
function readFavorites():any[]{try{return JSON.parse(localStorage.getItem('luna-pcl-favorites') || '[]');}catch{return [];}}
function writeFavorites(value:any[]){try{localStorage.setItem('luna-pcl-favorites',JSON.stringify(value.slice(-200)));}catch{/* storage unavailable in native webview */}}
function toggleFavorite(){if(!project.value)return;const key=favoriteKey(project.value),all=readFavorites();if(favorite.value){writeFavorites(all.filter(item=>item.key!==key));favorite.value=false;}else{writeFavorites([...all.filter(item=>item.key!==key),{key,provider:provider.value,kind:props.kind,name:project.value.name,description:project.value.description,projectId:project.value.projectId,iconUrl:project.value.iconUrl}]);favorite.value=true;}}
async function select(item:any){if(props.busy)return;const generation=++selectionGeneration;const itemProvider=String(item?.provider||provider.value);error.value='';versions.value=[];version.value='';project.value=normalize({...item,provider:itemProvider});favorite.value=readFavorites().some(entry=>entry.key===favoriteKey(project.value));detailLoading.value=true;projectTab.value='overview';try{const detail=await props.execute(props.packs?'modpack.project':'resource.project',{...params(itemProvider),projectId:item.projectId});if(generation!==selectionGeneration)return;project.value={...project.value,...normalize({...detail,provider:itemProvider})};const r=await props.execute(props.packs?'modpack.versions':'resource.versions',{...params(itemProvider),projectId:item.projectId});if(generation!==selectionGeneration)return;versions.value=(Array.isArray(r)?r:(r?.versions||[])).map((entry:any)=>({ ...entry, versionId:String(entry?.versionId||entry?.id||entry?.version||'') })).filter((entry:any)=>entry.versionId).sort((a:any,b:any)=>String(b.datePublished||b.created||b.date||'').localeCompare(String(a.datePublished||a.created||a.date||'')));version.value=versions.value[0]?.versionId||'';instanceName.value=item.name;}catch(e){if(generation===selectionGeneration)error.value=String(e);}finally{if(generation===selectionGeneration)detailLoading.value=false;}}
async function toggleSelected(item: any, enabled: boolean) {
  if (props.packs) return;
  if (!enabled) { delete selectedProjects[item.projectId]; return; }
  const entry = selectedProjects[item.projectId] = { ...normalize(item), loadingVersions: true, checked: true, versionId: '', versionOptions: [], versionLabel: t('pcl.resourceDownload.searching') };
  try {
    const response: any = await props.execute('resource.versions', { ...params(entry.provider), projectId: item.projectId });
    const list = (Array.isArray(response) ? response : response?.versions || [])
      .map((value: any) => ({ ...value, versionId: String(value?.versionId || value?.id || value?.version || '') }))
      .filter((value: any) => value.versionId);
    if (selectedProjects[item.projectId] !== entry) return;
    const selectedVersion = list[0];
    entry.rawVersions = list;
    entry.versionOptions = list.map((value: any) => ({ label: versionLabel(value), value: value.versionId }));
    entry.versionId = selectedVersion?.versionId || '';
    entry.versionLabel = selectedVersion ? versionLabel(selectedVersion) : t('pcl.auto2.k30a123c455');
  } catch (exception) {
    entry.versionLabel = exception instanceof Error ? exception.message : String(exception);
  } finally {
    if (selectedProjects[item.projectId] === entry) entry.loadingVersions = false;
  }
}
function prepareBatchDownload() { if (selectedProjectsList.value.some(entry => entry.loadingVersions || !entry.versionId)) return; batchConfirmOpen.value = true; }
async function installSelected() {
  const entries = selectedProjectsList.value.filter(entry => entry.checked && entry.versionId);
  if (!entries.length || installing.value) return;
  batchConfirmOpen.value = false;
  installing.value = true;
  const info = { title: t('pcl.auto2.k6e263f278c'), name: t('resource.selected', { count: entries.length }), kind: 'resource' };
  emit('install-start', info);
  let failure = '';
  try {
    for (const entry of entries) {
      const recursive = ['modrinth', 'curseforge'].includes(entry.provider) && providerMeta.value?.dependencyResolution !== false;
      let task: any = await props.execute('neo.task.start', { operation: recursive ? 'resource.install-with-dependencies' : 'resource.install-version', priority: 'high', parameters: { ...params(entry.provider), instance: props.instance, projectId: entry.projectId, versionId: entry.versionId, ...(recursive ? { includeOptional: optional.value } : {}) } });
      while (task && !isTaskItemFinished(task)) { emit('install-progress', task); await new Promise(resolve => setTimeout(resolve, 400)); task = await props.execute('neo.task.status', { taskId: task.taskId }); }
      emit('install-progress', task);
      if (!task || task.state !== 'succeeded') throw new Error(task?.error || t('pcl.auto2.k6c24c6cb06'));
      delete selectedProjects[entry.projectId];
    }
    emit('installed', null);
  } catch (exception) { failure = exception instanceof Error ? exception.message : String(exception); error.value = failure; }
  finally { installing.value = false; emit('install-done', failure ? { ...info, error: failure } : info); }
}
async function install(){
  if(!project.value||!version.value)return;
  installing.value=true;error.value='';
  const info={title:props.packs?t('pcl.auto2.k4c095994c9'):t('pcl.auto2.k6e263f278c'),name:props.packs?instanceName.value:project.value.name,kind:props.packs?'modpack':'resource'};
  emit('install-start',info);
  let failure = '';
  try{
    let completedResult: any = null;
    if(props.packs){
      let task:any=await props.execute('neo.task.start',{
        operation:'modpack.install',
        priority:'high',
        parameters:{provider:provider.value,projectId:project.value.projectId,versionId:version.value,name:instanceName.value,...(privateCode.value.trim() ? { privateCode: privateCode.value.trim() } : {})},
      });
      while(task && !isTaskItemFinished(task)){
        emit('install-progress',task);
        await new Promise(resolve=>setTimeout(resolve,400));
        task=await props.execute('neo.task.status',{taskId:task.taskId});
      }
      emit('install-progress',task);
      if(!task || task.state!=='succeeded')throw new Error(task?.error || t('pcl.auto2.k41e0e243d7'));
      completedResult = { id: task.createdInstanceId || '', name: instanceName.value };
    }else{
      // The recursive task is only implemented by the built-in providers.
      // Other providers still support direct version installation and must not
      // be rejected just because the optional-dependency checkbox is enabled.
      const projectProvider = String(project.value.provider || provider.value);
      const recursive = ['modrinth', 'curseforge'].includes(projectProvider) && providerMeta.value?.dependencyResolution !== false;
      let task:any=await props.execute('neo.task.start',{
        operation:recursive ? 'resource.install-with-dependencies' : 'resource.install-version',
        priority:'high',
        parameters:{...params(projectProvider),instance:props.instance,projectId:project.value.projectId,versionId:version.value,...(recursive ? { includeOptional: optional.value } : {})},
      });
      while(task && !isTaskItemFinished(task)){
        emit('install-progress',task);
        await new Promise(resolve=>setTimeout(resolve,400));
        task=await props.execute('neo.task.status',{taskId:task.taskId});
      }
      emit('install-progress',task);
      if(!task || task.state!=='succeeded')throw new Error(task?.error || t('pcl.auto2.k6c24c6cb06'));
    }
    emit('installed', completedResult);
  }catch(e){failure=e instanceof Error ? e.message : String(e);error.value=failure;}
  finally{installing.value=false;emit('install-done',failure ? {...info,error:failure} : info);}
}

provide(PclCatalogContextKey, { t, props, emit, busy: toRef(props, 'busy'), searching, detailLoading, installing, cancelling, packs: props.packs, provider, query, privateCode, results, project, versions, version, error, gameVersion, loader, sort, optional, instanceName, offset, searched, projectTab, favorite, providerLabel, providerOptions, sortOptions, loaderOptions, versionOptions, renderedBody, resetFilters, changeProvider, search, select, install, toggleFavorite, versionLabel, catalogRoot, scrollCatalogToTop, selectedProjects, selectedProjectsList, batchConfirmOpen, toggleSelected, prepareBatchDownload, installSelected, clearSelectedProjects });
</script>
<style>
.pcl-catalog {
  position: relative;
  color: var(--pcl-text);
}

.pcl-catalog-toolbar {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 9px;
}

.pcl-catalog-toolbar .pcl-select {
  flex: 0 0 158px;
  width: 158px;
}

.pcl-catalog-toolbar .pcl-input {
  flex: 1;
}

.pcl-catalog-filters {
  display: grid;
  grid-template-columns: minmax(120px, 1fr) 158px 132px auto;
  align-items: center;
  gap: 8px;
  margin-bottom: 12px;
}

.pcl-catalog-loading {
  display: flex;
  align-items: center;
  justify-content: center;
  position: absolute;
  top: 8px;
  right: 8px;
  min-height: 24px;
  margin: 0;
  gap: 3px;
  padding: 0 2px;
  border-radius: 4px;
  background: color-mix(in srgb, var(--pcl-card) 90%, transparent);
  pointer-events: none;
  visibility: hidden;
  opacity: 0;
  transition: opacity .12s ease;
}

.pcl-catalog-loading button {
  pointer-events: auto;
}

.pcl-catalog-loading.is-active {
  visibility: visible;
  opacity: 1;
}

.pcl-catalog-error {
  display: flex;
  align-items: center;
  gap: 7px;
  padding: 9px 11px;
  margin-bottom: 11px;
  border: 1px solid var(--pcl-danger);
  border-radius: 4px;
  background: color-mix(in srgb, var(--pcl-danger) 14%, transparent);
  color: var(--pcl-danger);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-catalog-error span {
  min-width: 0;
  flex: 1;
}

.pcl-catalog-error button {
  border: 0;
  background: transparent;
  color: inherit;
  font: inherit;
  cursor: pointer;
}

.pcl-catalog-back {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  margin: 0 0 9px;
  padding: 0;
  border: 0;
  background: none;
  color: var(--pcl-c2);
  font: inherit;
  font-size: calc(11px * var(--pcl-font-scale, 1));
  cursor: pointer;
}

.pcl-project {
  padding: 16px;
  border: 1px solid var(--pcl-card-border);
  border-radius: 5px;
  background: var(--pcl-card);
}

.pcl-project-head {
  display: flex;
  align-items: flex-start;
  gap: 13px;
}

.pcl-project-head > img,
.pcl-project-icon {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 74px;
  width: 74px;
  height: 74px;
  border-radius: 6px;
  background: var(--pcl-c6);
  color: var(--pcl-c2);
  object-fit: cover;
}

.pcl-project-copy {
  min-width: 0;
  flex: 1;
}

.pcl-project-copy small {
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-project-copy h2 {
  margin: 3px 0 4px;
  color: var(--pcl-text);
  font-size: calc(21px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-project-copy p {
  margin: 0;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-favorite--active {
  color: var(--pcl-c2);
}

.pcl-project-tabs {
  display: flex;
  gap: 3px;
  margin-top: 14px;
  border-bottom: 1px solid var(--pcl-border);
}

.pcl-project-tabs button {
  padding: 7px 10px;
  border: 0;
  border-bottom: 2px solid transparent;
  background: transparent;
  color: var(--pcl-muted);
  font: inherit;
  font-size: calc(11px * var(--pcl-font-scale, 1));
  cursor: pointer;
}

.pcl-project-tabs button.active {
  border-bottom-color: var(--pcl-c3);
  color: var(--pcl-text);
}

.pcl-project-body {
  max-height: 300px;
  margin: 12px 0;
  padding: 12px;
  overflow: auto;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c8);
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  line-height: 1.65;
  overflow-wrap: anywhere;
}

.pcl-project-body p {
  margin: 0 0 8px;
}

.pcl-project-body p:last-child {
  margin-bottom: 0;
}

.pcl-project-body h1,
.pcl-project-body h2,
.pcl-project-body h3,
.pcl-project-body h4 {
  margin: 12px 0 6px;
  color: var(--pcl-text);
  font-weight: 600;
}

.pcl-project-body h1 { font-size: calc(17px * var(--pcl-font-scale, 1)); }
.pcl-project-body h2 { font-size: calc(15px * var(--pcl-font-scale, 1)); }
.pcl-project-body h3 { font-size: calc(13px * var(--pcl-font-scale, 1)); }

.pcl-project-body a {
  color: var(--pcl-c2);
  text-decoration: none;
}

.pcl-project-body a:hover {
  text-decoration: underline;
}

.pcl-project-body img {
  max-width: 100%;
  height: auto;
  border-radius: 3px;
  vertical-align: middle;
}

.pcl-project-body code {
  padding: 1px 4px;
  border-radius: 3px;
  background: var(--pcl-c7);
  color: var(--pcl-c1);
  font: calc(11px * var(--pcl-font-scale, 1))/1.5 Consolas, monospace;
}

.pcl-project-body pre {
  margin: 8px 0;
  padding: 9px;
  overflow: auto;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c7);
}

.pcl-project-body pre code {
  padding: 0;
  background: transparent;
}

.pcl-project-body blockquote {
  margin: 8px 0;
  padding: 4px 9px;
  border-left: 3px solid var(--pcl-c3);
  background: var(--pcl-c7);
  color: var(--pcl-muted);
}

.pcl-project-body ul,
.pcl-project-body ol {
  margin: 6px 0;
  padding-left: 20px;
}

.pcl-project-body li {
  margin: 2px 0;
}

.pcl-project-body hr {
  margin: 10px 0;
  border: 0;
  border-top: 1px solid var(--pcl-border);
}

.pcl-project-body table {
  width: 100%;
  margin: 8px 0;
  border-collapse: collapse;
}

.pcl-project-body th,
.pcl-project-body td {
  padding: 4px 6px;
  border: 1px solid var(--pcl-border);
  text-align: left;
}

.pcl-version-list {
  display: flex;
  flex-direction: column;
  max-height: 210px;
  margin: 11px 0;
  overflow: auto;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
}

.pcl-version-option {
  display: flex;
  align-items: center;
  gap: 9px;
  padding: 8px 10px;
  border-bottom: 1px solid var(--pcl-border);
  cursor: pointer;
}

.pcl-version-option:last-child {
  border-bottom: 0;
}

.pcl-version-option:hover {
  background: var(--pcl-hover);
}

.pcl-version-option input {
  accent-color: var(--pcl-c3);
}

.pcl-version-option span {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.pcl-version-option strong {
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-version-option small {
  margin-top: 2px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-project-field {
  display: grid;
  grid-template-columns: 84px minmax(0, 1fr);
  align-items: center;
  gap: 9px;
  margin: 9px 0;
}

.pcl-project-field label {
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-optional {
  margin: 10px 0;
}

.pcl-project-actions {
  display: flex;
  justify-content: flex-start;
  margin-top: 11px;
}

.pcl-pagination {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 13px;
  margin-top: 12px;
  color: var(--pcl-muted);
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

.pcl-catalog-empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 7px;
  padding: 45px 20px;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-catalog-empty svg {
  color: var(--pcl-c5);
}

@media (max-width: 760px) {
  .pcl-catalog-filters {
    grid-template-columns: 1fr 1fr;
  }

  .pcl-catalog-toolbar {
    flex-wrap: wrap;
  }

  .pcl-catalog-toolbar .pcl-select {
    flex: 1 1 130px;
  }

  .pcl-catalog-toolbar .pcl-input {
    flex: 1 1 100%;
  }

  .pcl-project-field {
    grid-template-columns: 1fr;
  }
}
</style>

