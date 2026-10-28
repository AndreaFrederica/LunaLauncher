import { onUnmounted, watch, type Ref } from 'vue'
import { isDraggingInstance } from './instanceGroup'

interface UseDragAutoScrollOptions {
  /**
   * px from the edge that triggers auto-scroll.
   */
  edgeThreshold?: number
  /**
   * px per tick at the very edge.
   */
  maxStep?: number
  /**
   * Tick interval in ms.
   */
  intervalMs?: number
}

/**
 * Auto-scroll a container when the cursor sits near its edge during a drag
 * (gh #1396).
 *
 * Note on mouse-wheel scrolling during drag: Chromium fully suppresses
 * `wheel` events while a native HTML5 drag is in progress (no listener
 * position receives them — every drag-and-drop library hits this). Edge
 * auto-scroll is the standard workaround used by Trello, SortableJS, etc.
 *
 * Listeners are attached at `document` capture only while
 * `isDraggingInstance` is true, so they bypass any handler that calls
 * `stopPropagation` and don't cost anything when no drag is in progress.
 */
export function useDragAutoScroll(
  target: Ref<HTMLElement | null | undefined>,
  options: UseDragAutoScrollOptions = {},
) {
  const {
    edgeThreshold = 60,
    maxStep = 20,
    intervalMs = 16,
  } = options

  let timer: ReturnType<typeof setInterval> | null = null
  let stepX = 0
  let stepY = 0

  function stopTimer() {
    if (timer !== null) {
      clearInterval(timer)
      timer = null
    }
    stepX = 0
    stepY = 0
  }

  function ensureTimer() {
    if (timer !== null) return
    timer = setInterval(() => {
      const el = target.value
      if (!el || (stepX === 0 && stepY === 0)) {
        stopTimer()
        return
      }
      if (stepY !== 0) el.scrollTop += stepY
      if (stepX !== 0) el.scrollLeft += stepX
    }, intervalMs)
  }

  function onCaptureDragOver(e: DragEvent) {
    if (!isDraggingInstance.value) return
    const el = target.value
    if (!el) return

    const rect = el.getBoundingClientRect()
    const x = e.clientX
    const y = e.clientY

    if (x < rect.left || x > rect.right || y < rect.top || y > rect.bottom) {
      if (stepX !== 0 || stepY !== 0) stopTimer()
      return
    }

    const canScrollY = el.scrollHeight > el.clientHeight
    const canScrollX = el.scrollWidth > el.clientWidth

    let nextStepY = 0
    if (canScrollY) {
      const fromTop = y - rect.top
      const fromBottom = rect.bottom - y
      if (fromTop < edgeThreshold) {
        const f = Math.max(0, (edgeThreshold - fromTop) / edgeThreshold)
        nextStepY = -Math.ceil(f * maxStep)
      } else if (fromBottom < edgeThreshold) {
        const f = Math.max(0, (edgeThreshold - fromBottom) / edgeThreshold)
        nextStepY = Math.ceil(f * maxStep)
      }
    }

    let nextStepX = 0
    if (canScrollX) {
      const fromLeft = x - rect.left
      const fromRight = rect.right - x
      if (fromLeft < edgeThreshold) {
        const f = Math.max(0, (edgeThreshold - fromLeft) / edgeThreshold)
        nextStepX = -Math.ceil(f * maxStep)
      } else if (fromRight < edgeThreshold) {
        const f = Math.max(0, (edgeThreshold - fromRight) / edgeThreshold)
        nextStepX = Math.ceil(f * maxStep)
      }
    }

    stepX = nextStepX
    stepY = nextStepY

    if (stepX !== 0 || stepY !== 0) ensureTimer()
    else if (timer !== null) stopTimer()
  }

  function onDragEnd() {
    stopTimer()
  }

  let attached = false
  function attach() {
    if (attached) return
    document.addEventListener('dragover', onCaptureDragOver, { capture: true })
    document.addEventListener('dragend', onDragEnd, { capture: true })
    document.addEventListener('drop', onDragEnd, { capture: true })
    window.addEventListener('blur', onDragEnd)
    attached = true
  }
  function detach() {
    if (!attached) return
    document.removeEventListener('dragover', onCaptureDragOver, { capture: true } as any)
    document.removeEventListener('dragend', onDragEnd, { capture: true } as any)
    document.removeEventListener('drop', onDragEnd, { capture: true } as any)
    window.removeEventListener('blur', onDragEnd)
    attached = false
    stopTimer()
  }

  watch(isDraggingInstance, (dragging) => {
    if (dragging) attach()
    else detach()
  }, { immediate: true })

  onUnmounted(() => {
    detach()
  })
}
