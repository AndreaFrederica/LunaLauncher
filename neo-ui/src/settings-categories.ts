// SPDX-License-Identifier: MPL-2.0
export const settingCategoryLabels: Record<string, string> = {
  all: 'settings.categories.all',
  launch: 'settings.categories.launch',
  java: 'settings.categories.java',
  paths: 'settings.categories.paths',
  network: 'settings.categories.network',
  downloads: 'settings.categories.downloads',
  compatibility: 'settings.categories.compatibility',
  console: 'settings.categories.console',
  playtime: 'settings.categories.playtime',
  instance: 'settings.categories.instance',
  'instance-overrides': 'settings.categories.instance-overrides',
  'qt-ui': 'settings.categories.qt-ui',
  advanced: 'settings.categories.advanced',
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
