import { reactive } from 'vue';
import { i18n } from '../../i18n';

export type PclToastType = 'success' | 'error' | 'warning' | 'info';

export interface PclToastItem {
  id: number;
  title: string;
  message: string;
  type: PclToastType;
  timeout: number;
}

export const pclToasts = reactive<PclToastItem[]>([]);
let nextToastId = 0;
const timers = new Map<number, ReturnType<typeof setTimeout>>();

const titles: Record<PclToastType, string> = {
  success: i18n.global.t('pcl.toast.success'),
  error: i18n.global.t('pcl.toast.error'),
  warning: i18n.global.t('pcl.toast.warning'),
  info: i18n.global.t('pcl.toast.info'),
};

export function showPclToast(options: {
  message: string;
  title?: string;
  type?: PclToastType;
  timeout?: number;
}): number {
  const id = ++nextToastId;
  const timeout = Math.max(0, options.timeout ?? 3500);
  pclToasts.push({
    id,
    title: options.title || titles[options.type || 'info'],
    message: String(options.message || ''),
    type: options.type || 'info',
    timeout,
  });
  while (pclToasts.length > 4) {
    const removed = pclToasts.shift();
    if (removed) clearToastTimer(removed.id);
  }
  if (timeout > 0) {
    timers.set(id, setTimeout(() => dismissPclToast(id), timeout));
  }
  return id;
}

export function dismissPclToast(id: number) {
  clearToastTimer(id);
  const index = pclToasts.findIndex(item => item.id === id);
  if (index >= 0) pclToasts.splice(index, 1);
}

export function clearPclToasts() {
  for (const item of [...pclToasts]) dismissPclToast(item.id);
}

function clearToastTimer(id: number) {
  const timer = timers.get(id);
  if (timer) clearTimeout(timer);
  timers.delete(id);
}
