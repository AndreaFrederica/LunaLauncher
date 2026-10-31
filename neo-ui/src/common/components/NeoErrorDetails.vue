<template>
  <section class="neo-error-details" role="alert" aria-live="assertive">
    <header class="neo-error-details__header">
      <strong>{{ title }}</strong>
      <button v-if="closable" type="button" class="neo-error-details__close" :aria-label="closeLabel" @click="$emit('close')">×</button>
    </header>
    <p class="neo-error-details__message">{{ normalized.message }}</p>

    <dl v-if="operation || contextEntries.length" class="neo-error-details__facts">
      <template v-if="operation">
        <dt>操作</dt>
        <dd><code>{{ operation }}</code></dd>
      </template>
      <template v-for="entry in contextEntries" :key="entry[0]">
        <dt>{{ entry[0] }}</dt>
        <dd><code>{{ entry[1] }}</code></dd>
      </template>
    </dl>

    <details class="neo-error-details__raw" open>
      <summary>完整错误详情</summary>
      <pre>{{ normalized.raw }}</pre>
      <pre v-if="parameterText">请求参数：
{{ parameterText }}</pre>
    </details>

    <div v-if="$slots.actions" class="neo-error-details__actions">
      <slot name="actions" />
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed } from 'vue';

type ErrorDetails = { message: string; raw: string };

const props = withDefaults(defineProps<{
  error: unknown;
  title?: string;
  operation?: string;
  parameters?: Record<string, unknown>;
  context?: Record<string, unknown>;
  closeLabel?: string;
  closable?: boolean;
}>(), {
  title: '操作失败',
  operation: '',
  parameters: undefined,
  context: undefined,
  closeLabel: '关闭错误详情',
  closable: false,
});

defineEmits<{ close: [] }>();

function safeJson(value: unknown): string {
  try { return JSON.stringify(value, (_key, item) => item instanceof Error ? item.message : item, 2); }
  catch { return String(value); }
}

function normalize(value: unknown): ErrorDetails {
  if (value instanceof Error) return { message: value.message || String(value), raw: value.stack || value.message || String(value) };
  if (typeof value === 'string') {
    const text = value.trim();
    if (!text) return { message: '未知错误', raw: '未知错误' };
    try {
      const parsed = JSON.parse(text) as any;
      if (parsed && typeof parsed === 'object') {
        const message = String(parsed.message || parsed.error || parsed.reason || text);
        return { message, raw: safeJson(parsed) };
      }
    } catch { /* The backend commonly returns a plain human-readable string. */ }
    return { message: text, raw: text };
  }
  if (value && typeof value === 'object') {
    const item = value as any;
    const message = String(item.message || item.error || item.reason || '未知错误');
    return { message, raw: safeJson(value) };
  }
  return { message: String(value ?? '未知错误'), raw: String(value ?? '未知错误') };
}

const normalized = computed(() => normalize(props.error));
const parameterText = computed(() => props.parameters ? safeJson(props.parameters) : '');
const contextEntries = computed(() => Object.entries(props.context || {})
  .filter(([, value]) => value !== undefined && value !== null && value !== '')
  .map(([key, value]) => [key, typeof value === 'string' ? value : safeJson(value)] as const));
</script>

<style scoped>
.neo-error-details{margin:12px 0;padding:12px 14px;border:1px solid color-mix(in srgb,#d14343 58%,transparent);border-left:4px solid #d14343;border-radius:6px;background:color-mix(in srgb,#d14343 9%,var(--pcl-card-bg,#fff));color:var(--pcl-text,#263238)}
.neo-error-details__header{display:flex;align-items:center;justify-content:space-between;gap:12px;color:#a52222}
.neo-error-details__message{margin:8px 0;white-space:pre-wrap;overflow-wrap:anywhere;font-weight:600}
.neo-error-details__close{border:0;background:transparent;color:inherit;font-size:20px;line-height:1;cursor:pointer}
.neo-error-details__facts{display:grid;grid-template-columns:max-content minmax(0,1fr);gap:4px 12px;margin:8px 0;font-size:12px}
.neo-error-details__facts dt{color:color-mix(in srgb,currentColor 68%,transparent)}
.neo-error-details__facts dd{margin:0;overflow-wrap:anywhere}
.neo-error-details__raw{margin-top:8px;font-size:12px}
.neo-error-details__raw summary{cursor:pointer;color:#7f1d1d;font-weight:600}
.neo-error-details__raw pre{max-height:220px;margin:7px 0 0;padding:8px;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;border-radius:4px;background:color-mix(in srgb,#000 8%,transparent);font:11px/1.4 Consolas,monospace}
.neo-error-details__actions{display:flex;gap:8px;margin-top:10px}

:global(:root[data-neo-theme="dark"]) .neo-error-details {
  border-color: color-mix(in srgb, #ff7777 62%, transparent);
  border-left-color: #ff7777;
  background: color-mix(in srgb, #8f1d2c 30%, var(--pcl-card-bg, #242831));
  color: var(--pcl-text, #edf2f7);
}
:global(:root[data-neo-theme="dark"]) .neo-error-details__header { color: #ff9b9b; }
:global(:root[data-neo-theme="dark"]) .neo-error-details__raw summary { color: #ffb0b0; }
:global(:root[data-neo-theme="dark"]) .neo-error-details__raw pre {
  background: color-mix(in srgb, #000 32%, transparent);
  color: #e5edf7;
}
</style>
