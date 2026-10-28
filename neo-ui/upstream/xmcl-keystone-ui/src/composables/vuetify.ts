import { vuetify } from '@/vuetify'

export function useVuetifyColor() {
  const getColorCode = (code: string) => {
    const themeName = vuetify.theme.global.name.value
    const colors = vuetify.theme.themes.value[themeName]?.colors as Record<string, string> | undefined
    const direct = colors?.[code]
    if (direct) return direct
    // Fall back to the input string so plain CSS color names (e.g. "red", "#abc") still work.
    return code
  }

  return {
    getColorCode,
  }
}

