// SPDX-License-Identifier: MPL-2.0
export const settingCategoryLabels: Record<string, string> = {
  all: '全部设置',
  launch: '启动与窗口',
  java: 'Java 与内存',
  paths: '目录与文件',
  network: '网络连接',
  downloads: '下载与资源',
  compatibility: '兼容性与性能',
  console: '控制台与日志',
  playtime: '游戏时间',
  instance: '客户端信息',
  'instance-overrides': '客户端覆盖项',
  'qt-ui': 'Qt 旧界面兼容',
  advanced: '高级设置',
};

export const settingCategoryOrder = [
  'launch',
  'java',
  'paths',
  'network',
  'downloads',
  'compatibility',
  'console',
  'playtime',
  'instance',
  'instance-overrides',
  'qt-ui',
  'advanced',
];

export const settingCategoryIcons: Record<string, string> = {
  all: 'tune',
  launch: 'play_arrow',
  java: 'code',
  paths: 'folder',
  network: 'wifi',
  downloads: 'download',
  compatibility: 'speed',
  console: 'terminal',
  playtime: 'schedule',
  instance: 'sports_esports',
  'instance-overrides': 'tune',
  'qt-ui': 'desktop_windows',
  advanced: 'build',
};
