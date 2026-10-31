import { windows } from '../types';

// Measure rendered text, not screenshot pixels. Native and browser hosts receive
// the same width before opening; very long messages wrap in a scrollable body.
export function messageWidth(message: string, title = '提示:'): number {
  const { width, maxWidth } = windows.message;
  const context = document.createElement('canvas').getContext('2d');
  if (!context) return width;
  context.font = '18px "Microsoft YaHei", "微软雅黑", sans-serif';
  let contentWidth = 0;
  for (const line of message.split('\n')) contentWidth = Math.max(contentWidth, context.measureText(line).width);
  const titleWidth = context.measureText(title).width;
  // 38 px content insets, two borders, plus a little font-rendering tolerance.
  return Math.ceil(Math.min(maxWidth, Math.max(width, contentWidth + 82, titleWidth + 80)));
}
