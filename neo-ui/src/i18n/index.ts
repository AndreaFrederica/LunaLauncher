import { createI18n } from 'vue-i18n';
import zhCN from './locales/zh-CN';
import enUS from './locales/en-US';

export const supportedLocales = ['zh-CN', 'en-US'] as const;
export type NeoLocale = typeof supportedLocales[number];

function normalizeLocale(value: unknown): NeoLocale {
  const normalized = String(value || '').trim().toLowerCase().replace('_', '-');
  if (normalized.startsWith('en')) return 'en-US';
  return 'zh-CN';
}

function initialLocale(): NeoLocale {
  if (typeof window !== 'undefined') {
    const stored = window.localStorage.getItem('luna-neo-ui-locale');
    if (stored) return normalizeLocale(stored);
    return normalizeLocale(window.navigator.language);
  }
  return 'zh-CN';
}

export const i18n = createI18n({
  legacy: false,
  globalInjection: true,
  locale: initialLocale(),
  fallbackLocale: 'zh-CN',
  messages: { 'zh-CN': zhCN, 'en-US': enUS },
});

export function setNeoLocale(value: unknown): NeoLocale {
  const locale = normalizeLocale(value);
  i18n.global.locale.value = locale;
  if (typeof window !== 'undefined') window.localStorage.setItem('luna-neo-ui-locale', locale);
  return locale;
}

export function localeFromLauncherLanguage(value: unknown): NeoLocale {
  return normalizeLocale(value);
}
