export function useSingleton<T extends (...args: any[]) => Promise<any>>(f: T): T {
  let active: Promise<any> | undefined
  return ((...args: Parameters<T>): any => {
    if (active) {
      return active
    }
    const p = (async () => {
      try {
        return await f(...args)
      } finally {
        active = undefined
      }
    })()
    active = p
    return p
  }) as T
}