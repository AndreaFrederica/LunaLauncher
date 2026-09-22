import { Router, useRouter } from 'vue-router'

export function useExternalRoute(router: Router) {
  router.beforeEach((to, from, next) => {
    const full = to.fullPath.substring(1)
    if (full.startsWith('https:') || full.startsWith('http:')) {
      next(false)
      window.open(full, 'browser')
    } else {
      next()
    }
  })
}

