// Labels and numeric constraints mirrored from the Qt settings widgets.
export const settingMetadata: Record<string, {label:string;group:string;minimum?:number;maximum?:number}> = {
  "LaunchMaximized": {
    "label": "启动时最大化",
    "group": "窗口与启动"
  },
  "MinecraftWinWidth": {
    "label": "窗口宽度",
    "group": "窗口与启动",
    "minimum": 1,
    "maximum": 65536
  },
  "MinecraftWinHeight": {
    "label": "窗口高度",
    "group": "窗口与启动",
    "minimum": 1,
    "maximum": 65536
  },
  "CloseAfterLaunch": {
    "label": "游戏启动后隐藏启动器",
    "group": "窗口与启动"
  },
  "QuitAfterGameStop": {
    "label": "游戏结束后退出启动器",
    "group": "窗口与启动"
  },
  "ShowGameTime": {
    "label": "显示游戏时间",
    "group": "游戏时间"
  },
  "RecordGameTime": {
    "label": "记录游戏时间",
    "group": "游戏时间"
  },
  "ShowConsole": {
    "label": "显示控制台",
    "group": "控制台"
  },
  "AutoCloseConsole": {
    "label": "正常退出后关闭控制台",
    "group": "控制台"
  },
  "ShowConsoleOnError": {
    "label": "出错时显示控制台",
    "group": "控制台"
  },
  "OnlineFixes": {
    "label": "旧版本在线服务修复",
    "group": "性能与兼容"
  },
  "UseNativeGLFW": {
    "label": "使用系统 GLFW",
    "group": "性能与兼容"
  },
  "CustomGLFWPath": {
    "label": "自定义 GLFW 路径",
    "group": "性能与兼容"
  },
  "UseNativeOpenAL": {
    "label": "使用系统 OpenAL",
    "group": "性能与兼容"
  },
  "CustomOpenALPath": {
    "label": "自定义 OpenAL 路径",
    "group": "性能与兼容"
  },
  "EnableFeralGamemode": {
    "label": "启用 GameMode",
    "group": "性能与兼容"
  },
  "EnableMangoHud": {
    "label": "启用 MangoHud",
    "group": "性能与兼容"
  },
  "UseDiscreteGpu": {
    "label": "使用独立显卡",
    "group": "性能与兼容"
  },
  "UseZink": {
    "label": "使用 Zink",
    "group": "性能与兼容"
  },
  "JoinServerOnLaunch": {
    "label": "启动后快速加入",
    "group": "账户与快速加入"
  },
  "UseAccountForInstance": {
    "label": "为实例指定账户",
    "group": "账户与快速加入"
  },
  "OverrideLegacySettings": {
    "label": "Legacy Tweaks",
    "group": "游戏与启动"
  },
  "GlobalDataPacksEnabled": {
    "label": "启用全局数据包",
    "group": "下载与资源"
  },
  "GlobalDataPacksPath": {
    "label": "全局数据包路径",
    "group": "下载与资源"
  },
  "AssetCacheExpiryDays": {
    "label": "资源校验缓存有效天数",
    "group": "下载与资源",
    "minimum": 1,
    "maximum": 365
  },
  "OverrideJavaLocation": {
    "label": "Java Installation",
    "group": "Java 与内存"
  },
  "JavaPath": {
    "label": "Java 可执行文件路径",
    "group": "Java 与内存"
  },
  "IgnoreJavaCompatibility": {
    "label": "跳过 Java 兼容性检查",
    "group": "Java 与内存"
  },
  "JvmArgs": {
    "label": "JVM 参数",
    "group": "Java 与内存"
  },
  "IgnoreJavaWizard": {
    "label": "跳过 Java 向导",
    "group": "Java 与内存"
  },
  "AutomaticJavaSwitch": {
    "label": "自动选择 Java",
    "group": "Java 与内存"
  },
  "AutomaticJavaDownload": {
    "label": "自动下载 Java",
    "group": "Java 与内存"
  },
  "PermGen": {
    "label": "PermGen（MiB）",
    "group": "Java 与内存",
    "minimum": 4,
    "maximum": 1048576
  },
  "LowMemWarning": {
    "label": "显示内存不足警告",
    "group": "Java 与内存"
  },
  "ShowGlobalGameTime": {
    "label": "显示总游戏时间",
    "group": "游戏时间"
  },
  "ShowGameTimeWithoutDays": {
    "label": "以小时显示游戏时间",
    "group": "游戏时间"
  },
  "MinMemAlloc": {
    "label": "最小内存（MiB）",
    "group": "Java 与内存"
  },
  "MaxMemAlloc": {
    "label": "最大内存（MiB）",
    "group": "Java 与内存"
  },
  "PreLaunchCommand": {
    "label": "启动前命令",
    "group": "命令与环境"
  },
  "WrapperCommand": {
    "label": "包装命令",
    "group": "命令与环境"
  },
  "PostExitCommand": {
    "label": "退出后命令",
    "group": "命令与环境"
  },
  "Env": {
    "label": "环境变量（JSON 文本）",
    "group": "命令与环境"
  },
  "JoinServerOnLaunchAddress": {
    "label": "服务器地址",
    "group": "账户与快速加入"
  },
  "JoinWorldOnLaunch": {
    "label": "世界名称",
    "group": "账户与快速加入"
  },
  "InstanceAccountId": {
    "label": "指定账户 ID",
    "group": "账户与快速加入"
  },
  "OverrideModDownloadLoaders": {
    "label": "覆盖模组下载加载器筛选",
    "group": "下载与资源"
  },
  "ModDownloadLoaders": {
    "label": "模组下载加载器（JSON 文本）",
    "group": "下载与资源"
  },
  "AssetVerificationMode": {
    "label": "资源校验模式",
    "group": "下载与资源"
  },
  "TechnicClientID": {
    "label": "TechnicClientID",
    "group": "API"
  },
  "JProfilerPath": {
    "label": "JProfilerPath",
    "group": "ExternalTools"
  },
  "JVisualVMPath": {
    "label": "JVisualVMPath",
    "group": "ExternalTools"
  },
  "MCEditPath": {
    "label": "MCEditPath",
    "group": "ExternalTools"
  },
  "CurseForgeExternalToolPath": {
    "label": "CurseForgeExternalToolPath",
    "group": "ExternalTools"
  },
  "CurseForgeExternalToolEnabled": {
    "label": "Allow the launcher to invoke an external download tool",
    "group": "ExternalTools"
  },
  "JsonEditor": {
    "label": "JsonEditor",
    "group": "ExternalTools"
  },
  "MenuBarInsteadOfToolBar": {
    "label": "Replace toolbar with menubar",
    "group": "Launcher"
  },
  "UseNewUI": {
    "label": "Use New UI Layout (Requires Restart)",
    "group": "Launcher"
  },
  "ShowNewsBar": {
    "label": "Show News Bar",
    "group": "Launcher"
  },
  "StatusBarVisible": {
    "label": "Show Status Bar",
    "group": "Launcher"
  },
  "ShowServerPreview": {
    "label": "Show Server Preview in toolbar",
    "group": "Launcher"
  },
  "ServerPreviewDynamicWidth": {
    "label": "Auto-adjust Server Preview width to fit names",
    "group": "Launcher"
  },
  "ShowTerracottaToolBar": {
    "label": "Show Terracotta button in toolbar",
    "group": "Launcher"
  },
  "ShowYukariConnectToolBar": {
    "label": "Show YukariConnect button in toolbar",
    "group": "Launcher"
  },
  "ShowTerracottaStatusBar": {
    "label": "Show Terracotta status in status bar",
    "group": "Launcher"
  },
  "ShowYukariConnectStatusBar": {
    "label": "Show YukariConnect status in status bar",
    "group": "Launcher"
  },
  "NumberOfConcurrentTasks": {
    "label": "NumberOfConcurrentTasks",
    "group": "Launcher"
  },
  "NumberOfConcurrentDownloads": {
    "label": "NumberOfConcurrentDownloads",
    "group": "Launcher"
  },
  "NumberOfManualRetries": {
    "label": "NumberOfManualRetries",
    "group": "Launcher"
  },
  "RequestTimeout": {
    "label": "RequestTimeout",
    "group": "Launcher"
  },
  "ConsoleMaxLines": {
    "label": "ConsoleMaxLines",
    "group": "Launcher"
  },
  "ConsoleOverflowStop": {
    "label": "Stop logging when log overflows",
    "group": "Launcher"
  },
  "InstanceDir": {
    "label": "InstanceDir",
    "group": "Launcher"
  },
  "CentralModsDir": {
    "label": "CentralModsDir",
    "group": "Launcher"
  },
  "IconsDir": {
    "label": "IconsDir",
    "group": "Launcher"
  },
  "DownloadsDir": {
    "label": "DownloadsDir",
    "group": "Launcher"
  },
  "SkinsDir": {
    "label": "SkinsDir",
    "group": "Launcher"
  },
  "JavaDir": {
    "label": "JavaDir",
    "group": "Launcher"
  },
  "DownloadsDirWatchRecursive": {
    "label": "Check subfolders for blocked mods",
    "group": "Launcher"
  },
  "MoveModsFromDownloadsDir": {
    "label": "Move blocked mods instead of copying them",
    "group": "Launcher"
  },
  "ModMetadataDisabled": {
    "label": "Keep track of mod metadata",
    "group": "Launcher"
  },
  "ModDependenciesDisabled": {
    "label": "Install dependencies automatically",
    "group": "Launcher"
  },
  "ShowModIncompat": {
    "label": "Detect and show mod incompatibilities (experimental)",
    "group": "Launcher"
  },
  "SkipModpackUpdatePrompt": {
    "label": "Suggest to update an existing instance during modpack installation",
    "group": "Launcher"
  },
  "ProxyAddr": {
    "label": "ProxyAddr",
    "group": "Proxy"
  },
  "ProxyPort": {
    "label": "ProxyPort",
    "group": "Proxy"
  },
  "ProxyUser": {
    "label": "ProxyUser",
    "group": "Proxy"
  },
  "ProxyPass": {
    "label": "ProxyPass",
    "group": "Proxy"
  }
};
