// SPDX-License-Identifier: MPL-2.0
// Labels and numeric constraints mirrored from the Qt settings widgets.
export const settingMetadata: Record<string, {label:string;group:string;minimum?:number;maximum?:number}> = {
  "LaunchMaximized": {
    "label": "neo.schema.k2cf44bea7a",
    "group": "launch"
  },
  "MinecraftWinWidth": {
    "label": "neo.schema.kfbd8afb6b1",
    "group": "launch",
    "minimum": 1,
    "maximum": 65536
  },
  "MinecraftWinHeight": {
    "label": "neo.schema.ke2176a7c0b",
    "group": "launch",
    "minimum": 1,
    "maximum": 65536
  },
  "CloseAfterLaunch": {
    "label": "neo.schema.kebf223ab17",
    "group": "launch"
  },
  "QuitAfterGameStop": {
    "label": "neo.schema.kcedfc70ad4",
    "group": "launch"
  },
  "ShowGameTime": {
    "label": "neo.schema.kff9e4486e9",
    "group": "playtime"
  },
  "RecordGameTime": {
    "label": "neo.schema.k19241e920e",
    "group": "playtime"
  },
  "ShowConsole": {
    "label": "neo.schema.kd6dca20f52",
    "group": "console"
  },
  "AutoCloseConsole": {
    "label": "neo.schema.k7a434e1b3c",
    "group": "console"
  },
  "ShowConsoleOnError": {
    "label": "neo.schema.k1f4eac88bb",
    "group": "console"
  },
  "OnlineFixes": {
    "label": "neo.schema.k29eb9b95d0",
    "group": "compatibility"
  },
  "UseNativeGLFW": {
    "label": "neo.schema.kb6565ef62e",
    "group": "compatibility"
  },
  "CustomGLFWPath": {
    "label": "neo.schema.k5e4c8e9367",
    "group": "compatibility"
  },
  "UseNativeOpenAL": {
    "label": "neo.schema.k7377d6632f",
    "group": "compatibility"
  },
  "CustomOpenALPath": {
    "label": "neo.schema.kaca0121202",
    "group": "compatibility"
  },
  "EnableFeralGamemode": {
    "label": "neo.schema.k903512cd37",
    "group": "compatibility"
  },
  "EnableMangoHud": {
    "label": "neo.schema.kdedb43a2ff",
    "group": "compatibility"
  },
  "UseDiscreteGpu": {
    "label": "neo.schema.k505ed891dc",
    "group": "compatibility"
  },
  "UseZink": {
    "label": "neo.schema.kedb41c0c52",
    "group": "compatibility"
  },
  "JoinServerOnLaunch": {
    "label": "neo.schema.kcf39d8520b",
    "group": "launch"
  },
  "UseAccountForInstance": {
    "label": "neo.schema.k59d3735305",
    "group": "launch"
  },
  "OverrideLegacySettings": {
    "label": "Legacy Tweaks",
    "group": "launch"
  },
  "GlobalDataPacksEnabled": {
    "label": "neo.schema.ke3ac026340",
    "group": "downloads"
  },
  "GlobalDataPacksPath": {
    "label": "neo.schema.kd4b0445d28",
    "group": "downloads"
  },
  "AssetCacheExpiryDays": {
    "label": "neo.schema.k921ffa9cc7",
    "group": "downloads",
    "minimum": 1,
    "maximum": 365
  },
  "OverrideJavaLocation": {
    "label": "Java Installation",
    "group": "java"
  },
  "JavaPath": {
    "label": "neo.schema.k9476a9d703",
    "group": "java"
  },
  "IgnoreJavaCompatibility": {
    "label": "neo.schema.k60b0ba35e0",
    "group": "java"
  },
  "JvmArgs": {
    "label": "neo.schema.kb001801f13",
    "group": "java"
  },
  "IgnoreJavaWizard": {
    "label": "neo.schema.k24582745ff",
    "group": "java"
  },
  "AutomaticJavaSwitch": {
    "label": "neo.schema.ke1fa494669",
    "group": "java"
  },
  "AutomaticJavaDownload": {
    "label": "neo.schema.k5f68598ff5",
    "group": "java"
  },
  "PermGen": {
    "label": "PermGen（MiB）",
    "group": "java",
    "minimum": 4,
    "maximum": 1048576
  },
  "LowMemWarning": {
    "label": "neo.schema.k7d21c07a44",
    "group": "java"
  },
  "ShowGlobalGameTime": {
    "label": "neo.schema.k60f26bd0b9",
    "group": "playtime"
  },
  "ShowGameTimeWithoutDays": {
    "label": "neo.schema.kfad9d70eb1",
    "group": "playtime"
  },
  "MinMemAlloc": {
    "label": "neo.schema.ka6551c0898",
    "group": "java"
  },
  "MaxMemAlloc": {
    "label": "neo.schema.k14469bb314",
    "group": "java"
  },
  "PreLaunchCommand": {
    "label": "neo.schema.k7e026da625",
    "group": "launch"
  },
  "WrapperCommand": {
    "label": "neo.schema.k49a71399fa",
    "group": "launch"
  },
  "PostExitCommand": {
    "label": "neo.schema.kfa4f7c58eb",
    "group": "launch"
  },
  "Env": {
    "label": "neo.schema.k84f9fa0400",
    "group": "launch"
  },
  "JoinServerOnLaunchAddress": {
    "label": "neo.schema.k2eb291f695",
    "group": "launch"
  },
  "JoinWorldOnLaunch": {
    "label": "neo.schema.k5a09d081b2",
    "group": "launch"
  },
  "InstanceAccountId": {
    "label": "neo.schema.kda883dd5ca",
    "group": "launch"
  },
  "OverrideModDownloadLoaders": {
    "label": "neo.schema.k5415101e28",
    "group": "downloads"
  },
  "ModDownloadLoaders": {
    "label": "neo.schema.k99ec1d1a85",
    "group": "downloads"
  },
  "AssetVerificationMode": {
    "label": "neo.schema.k402717b964",
    "group": "downloads"
  },
  "AssetIntegrityTrusted": {
    "label": "neo.schema.k88cd2c41dc",
    "group": "downloads"
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
