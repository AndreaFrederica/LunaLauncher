import { reactive, watch } from 'vue';

const storageKey = 'luna-pcl-hidden-features';

export interface PclFeature {
  id: string;
  label: string;
  description?: string;
}

export const pclSetupFeatures: PclFeature[] = [
  { id: 'java', label: 'Java', description: 'Java 运行时与诊断' },
  { id: 'advanced', label: '高级启动', description: 'JVM 参数与启动前后命令' },
  { id: 'game', label: '资源与整合包', description: '模组、整合包与数据包行为' },
  { id: 'network', label: '联机', description: '启动器代理与连接诊断' },
  { id: 'language', label: '语言', description: '界面语言与翻译贡献' },
  { id: 'misc', label: '网络代理', description: '启动器代理与连接诊断' },
  { id: 'download-settings', label: '下载与传输', description: '并发、重试、Aria2 与下载代理' },
  { id: 'paths', label: '路径与工具', description: '实例、下载、Java 与外部工具路径' },
  { id: 'console', label: '控制台', description: '控制台字体与输出设置' },
  { id: 'advanced-settings', label: '高级兼容性', description: '资源校验、原生库与游戏时间' },
  { id: 'about', label: '软件信息', description: '版本、目录与许可信息' },
  { id: 'update', label: '软件更新', description: '更新通道与检查' },
  { id: 'feedback', label: '反馈', description: '问题反馈与进度' },
  { id: 'logs', label: '查看日志', description: '启动器日志入口' },
];

export const pclDownloadFeatures: PclFeature[] = [
  { id: 'mods', label: '模组', description: '浏览并安装模组' },
  { id: 'modpacks', label: '整合包', description: '浏览并安装整合包' },
  { id: 'datapacks', label: '数据包', description: '为实例安装数据包' },
  { id: 'resourcepacks', label: '资源包', description: '为实例安装资源包' },
  { id: 'shaders', label: '光影包', description: '为实例安装光影' },
  { id: 'worlds', label: '世界', description: '世界资源入口' },
  { id: 'favorites', label: '收藏夹', description: '收藏的下载项目' },
  { id: 'forge', label: 'Forge', description: 'Forge 安装包入口' },
  { id: 'neoforge', label: 'NeoForge', description: 'NeoForge 安装包入口' },
  { id: 'cleanroom', label: 'Cleanroom', description: 'Cleanroom 安装包入口' },
  { id: 'fabric', label: 'Fabric', description: 'Fabric 安装包入口' },
  { id: 'liteloader', label: 'LiteLoader', description: 'LiteLoader 入口' },
  { id: 'legacyfabric', label: 'Legacy Fabric', description: 'Legacy Fabric 安装入口' },
  { id: 'labymod-production', label: 'LabyMod（正式版）', description: 'LabyMod 正式版安装入口' },
  { id: 'labymod-snapshot', label: 'LabyMod（快照）', description: 'LabyMod 快照版安装入口' },
];

function load(): Record<string, boolean> {
  if (typeof localStorage === 'undefined') return {};
  try {
    const value = JSON.parse(localStorage.getItem(storageKey) || '{}');
    return value && typeof value === 'object' ? value : {};
  } catch {
    return {};
  }
}

export const hiddenPclFeatures = reactive<Record<string, boolean>>(load());

watch(hiddenPclFeatures, value => {
  if (typeof localStorage === 'undefined') return;
  try {
    localStorage.setItem(storageKey, JSON.stringify(value));
  } catch {
    // Storage may be unavailable in a restricted webview.
  }
}, { deep: true });

export function isPclFeatureHidden(id: string): boolean {
  return Boolean(hiddenPclFeatures[id]);
}

export function setPclFeatureHidden(id: string, hidden: boolean) {
  if (hidden) hiddenPclFeatures[id] = true;
  else delete hiddenPclFeatures[id];
}

export function resetPclFeatures() {
  for (const key of Object.keys(hiddenPclFeatures)) delete hiddenPclFeatures[key];
}
