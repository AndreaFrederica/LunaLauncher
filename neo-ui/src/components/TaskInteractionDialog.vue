<template>
  <q-dialog :model-value="!!current" persistent>
    <q-card class="task-interaction" :class="{ 'task-interaction--pcl': pcl }">
      <q-card-section>
        <h2>{{ restricted ? '需要手动下载的模组' : current?.request.choices ? '需要确认' : '需要输入' }}</h2>
        <p v-if="restricted">以下文件不允许第三方直接下载。请从作者的下载页面获取文件，校验通过后确认继续安装。</p>
        <p v-else>{{ current?.request.prompt }}</p>
        <p v-if="error || current?.request.error" class="task-interaction__error" role="alert">{{ error || current?.request.error }}</p>
      </q-card-section>
      <q-card-section v-if="restricted" class="task-interaction__body">
        <div class="task-interaction__tools">
          <PclButton :disabled="submitting" @click="chooseDirectory">选择下载目录</PclButton>
          <PclButton :disabled="submitting" @click="submit('scan')">重新检测下载</PclButton>
          <span>{{ matchedCount }} / {{ files.length }} 个文件已校验</span>
        </div>
        <p class="task-interaction__folders">检测目录：{{ (current?.request.directories || []).join('、') }}</p>
        <ul class="task-interaction__files">
          <li v-for="(file, index) in files" :key="`${file.hash}:${index}`">
            <div class="task-interaction__file-info">
              <strong>{{ file.name }}</strong>
              <span :class="{ verified: file.matched }">{{ file.matched ? '校验通过' : '等待下载并校验' }}</span>
              <small v-if="file.localPath">{{ file.localPath }}</small>
            </div>
            <div class="task-interaction__tools">
              <PclButton size="sm" :disabled="submitting || !safeUrl(file.url)" @click="openDownload(file.url)">打开下载页</PclButton>
              <PclButton size="sm" :disabled="submitting" @click="chooseFile(file.name)">选择文件</PclButton>
            </div>
          </li>
        </ul>
      </q-card-section>
      <q-card-section v-else>
        <PclSelect v-if="current?.request.choices" v-model="answer" :options="choiceOptions" />
        <PclInput v-else v-model="answer" :type="current?.request.secret ? 'password' : 'text'" />
      </q-card-section>
      <q-card-actions align="right">
        <PclButton :disabled="submitting" @click="submit(null)">{{ restricted ? '取消安装' : '取消' }}</PclButton>
        <PclButton variant="highlight" :disabled="submitting || (restricted && !current?.request.complete)"
          @click="submit(restricted ? 'continue' : answer)">{{ restricted ? '确认文件并继续安装' : current?.request.choices ? '继续' : '确认' }}</PclButton>
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { openUrl } from '@tauri-apps/plugin-opener';
import { launcher } from '../boot/launcher-api';
import PclButton from '../pcl/components/PclButton.vue';
import PclInput from '../pcl/components/PclInput.vue';
import PclSelect from '../pcl/components/PclSelect.vue';
import { pickPclDirectory, pickPclFile } from '../pcl/composables/usePclFileDialog';

type Request = {
  interactionId: string; type?: string; prompt?: string; secret?: boolean; choices?: any[];
  files?: { name: string; hash: string; url: string; matched: boolean; localPath: string }[];
  directories?: string[]; complete?: boolean; error?: string;
};
const props = defineProps<{
  execute: (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
  pcl?: boolean; legacy?: { interactionId: string; context: Request } | null;
}>();
const emit = defineEmits<{ 'legacy-finished': [] }>();
const tasks = ref<any[]>([]);
const submitted = new Set<string>();
const current = computed(() => {
  if (props.legacy && !submitted.has(props.legacy.interactionId))
    return { taskId: '', request: { ...props.legacy.context, interactionId: props.legacy.interactionId } };
  const task = tasks.value.find(item => !item.finished && item.interaction?.interactionId && !submitted.has(item.interaction.interactionId));
  return task ? { taskId: String(task.taskId), request: task.interaction as Request } : null;
});
const restricted = computed(() => current.value?.request.type === 'restricted-downloads');
const files = computed(() => current.value?.request.files || []);
const matchedCount = computed(() => files.value.filter(file => file.matched).length);
const choiceOptions = computed(() => (current.value?.request.choices || []).map((choice, value) => ({
  label: typeof choice === 'string' ? choice : choice.label || choice.name || String(value), value,
})));
const answer = ref<string | number>('');
const error = ref('');
const submitting = ref(false);
let stopped = false;
let timer: ReturnType<typeof setTimeout> | undefined;
watch(() => current.value?.request.interactionId, () => {
  answer.value = current.value?.request.choices ? 0 : '';
  error.value = '';
});

async function refresh() {
  try {
    const result = await props.execute('neo.task.list');
    if (!stopped) tasks.value = result?.tasks || [];
  } catch { /* Backend disconnects are reported by the application shell. */ }
  if (!stopped) timer = setTimeout(refresh, 800);
}
onMounted(() => { void refresh(); });
onBeforeUnmount(() => { stopped = true; clearTimeout(timer); });

async function submit(value: string | number | null) {
  const pending = current.value;
  if (!pending || submitting.value) return;
  submitting.value = true;
  error.value = '';
  try {
    const parameters = { interactionId: pending.request.interactionId, ...(value === null ? { cancel: true } : { value }) };
    if (pending.taskId && value === null) await props.execute('neo.task.cancel', { taskId: pending.taskId });
    else if (pending.taskId) await props.execute('neo.task.respond', { taskId: pending.taskId, ...parameters });
    else { await launcher.respond(parameters); emit('legacy-finished'); }
    submitted.add(pending.request.interactionId);
    tasks.value = tasks.value.filter(task => task.interaction?.interactionId !== pending.request.interactionId);
  } catch (exception) { error.value = exception instanceof Error ? exception.message : String(exception); }
  finally { submitting.value = false; }
}
async function chooseDirectory() {
  const path = await pickPclDirectory({ title: '选择保存受限模组的下载目录' });
  if (path) await submit(path);
}
async function chooseFile(name: string) {
  const path = await pickPclFile({ title: `选择下载好的 ${name}` });
  if (path) await submit(path);
}
function safeUrl(value: string): boolean {
  try { return ['https:', 'http:'].includes(new URL(value).protocol); } catch { return false; }
}
async function openDownload(url: string) {
  if (!safeUrl(url)) return;
  try { await openUrl(url); }
  catch (exception) { error.value = exception instanceof Error ? exception.message : String(exception); }
}
</script>

<style scoped>
.task-interaction { width: 780px; max-width: 94vw; background: var(--neo-panel, #202733); color: var(--neo-text, #eef4ff); }
.task-interaction--pcl { background: var(--pcl-c7); color: var(--pcl-text); border-radius: var(--pcl-radius); }
.task-interaction:not(.task-interaction--pcl) :deep(.pcl-btn) { padding: 7px 12px; border: 1px solid #64748b; border-radius: 6px; background: #283549; color: #eef4ff; cursor: pointer; }
.task-interaction:not(.task-interaction--pcl) :deep(.pcl-btn:disabled) { opacity: .45; cursor: default; }
.task-interaction:not(.task-interaction--pcl) :deep(.pcl-btn--highlight) { background: #246bd4; }
.task-interaction h2 { margin: 0 0 10px; font-size: 19px; }
.task-interaction p { margin: 6px 0; line-height: 1.6; }
.task-interaction__body { max-height: 58vh; overflow: auto; }
.task-interaction__tools { display: flex; align-items: center; flex-wrap: wrap; gap: 8px; }
.task-interaction__folders { font-size: 12px; overflow-wrap: anywhere; opacity: .75; }
.task-interaction__files { padding: 0; margin: 16px 0 0; list-style: none; }
.task-interaction__files li { display: flex; flex-wrap: wrap; align-items: center; gap: 12px; padding: 12px 0; border-top: 1px solid var(--neo-border, #526075); }
.task-interaction__file-info { display: flex; flex: 1; min-width: 200px; flex-direction: column; gap: 4px; overflow-wrap: anywhere; }
.task-interaction__file-info span, .task-interaction__file-info small { font-size: 12px; }
.task-interaction__file-info .verified { color: #4fbd8b; }
.task-interaction__error { color: #ed7373; }
</style>
