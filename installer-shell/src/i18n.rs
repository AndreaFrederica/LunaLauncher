#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Language {
    SimplifiedChinese,
    English,
}

impl Language {
    pub fn detect() -> Self {
        let locale = ["LANGUAGE", "LC_ALL", "LANG"]
            .into_iter()
            .filter_map(|name| std::env::var(name).ok())
            .next()
            .unwrap_or_default()
            .to_ascii_lowercase();
        if locale.contains("en") && !locale.contains("zh") {
            Self::English
        } else {
            // LunaLauncher targets Chinese users by default, and the bundled
            // font guarantees this fallback is usable on a clean Windows PC.
            Self::SimplifiedChinese
        }
    }

    pub fn name(self) -> &'static str {
        match self {
            Self::SimplifiedChinese => "简体中文",
            Self::English => "English",
        }
    }
}

pub fn text(language: Language, key: &str) -> &'static str {
    match (language, key) {
        (Language::SimplifiedChinese, "first.title") => "打开 LunaLauncher 安装包",
        (Language::SimplifiedChinese, "first.subtitle") => "请选择要如何处理这个安装包。",
        (Language::SimplifiedChinese, "first.intro") => {
            "推荐直接安装；也可以只解压文件，或先查看压缩包内容。"
        }
        (Language::SimplifiedChinese, "first.install") => "安装 LunaLauncher",
        (Language::SimplifiedChinese, "first.extract") => "只解压文件",
        (Language::SimplifiedChinese, "first.view") => "查看安装包内容",
        (Language::SimplifiedChinese, "first.ready") => "个文件已经准备好查看。",
        (Language::SimplifiedChinese, "first.complete") => {
            "只有安装向导成功完成后，安装才会被记录。"
        }
        (Language::SimplifiedChinese, "first.leave") => {
            "如果中途关闭，下次打开仍会回到这个选择页面。"
        }
        (Language::SimplifiedChinese, "install.title") => "安装 LunaLauncher",
        (Language::SimplifiedChinese, "install.subtitle") => {
            "跟随下面几个步骤完成安装，默认设置适合大多数用户。"
        }
        (Language::SimplifiedChinese, "step.location") => "1. 安装位置",
        (Language::SimplifiedChinese, "step.ui") => "2. 启动界面",
        (Language::SimplifiedChinese, "step.data") => "3. 数据和实例目录",
        (Language::SimplifiedChinese, "step.shortcuts") => "4. 快捷方式",
        (Language::SimplifiedChinese, "install.directory") => "安装到：",
        (Language::SimplifiedChinese, "install.shell_dir") => {
            "原版/HMCL/PCL 模式会把文件放进当前 shell 目录下的 LunaLauncher 子目录。"
        }
        (Language::SimplifiedChinese, "install.ui") => "启动时打开：",
        (Language::SimplifiedChinese, "ui.classic") => "Classic UI（经典界面）",
        (Language::SimplifiedChinese, "ui.neo") => "NeoUI（新界面）",
        (Language::SimplifiedChinese, "ui.both") => "每次启动时询问",
        (Language::SimplifiedChinese, "ui.help") => {
            "两个启动程序都会安装；选择界面决定 shell 启动时的行为。"
        }
        (Language::SimplifiedChinese, "ui.preview") => "界面预览：",
        (Language::SimplifiedChinese, "shortcuts.desktop") => "创建桌面快捷方式",
        (Language::SimplifiedChinese, "shortcuts.start") => "创建开始菜单快捷方式",
        (Language::SimplifiedChinese, "data.title") => "启动器数据和实例目录",
        (Language::SimplifiedChinese, "data.help") => {
            "这个选项决定游戏实例放在哪里，以及能否被 PCL/HMCL 识别。"
        }
        (Language::SimplifiedChinese, "mode.mmc") => "MMC / Prism",
        (Language::SimplifiedChinese, "mode.vanilla") => "原版 / HMCL / PCL",
        (Language::SimplifiedChinese, "mode.custom") => "自定义",
        (Language::SimplifiedChinese, "mode.mmc.help") => {
            "推荐给只使用 LunaLauncher 的用户：数据放在系统目录，不注册外层目录。"
        }
        (Language::SimplifiedChinese, "mode.vanilla.help") => {
            "推荐给从 PCL/HMCL 迁移的用户：便携数据放在启动器目录，并注册外层目录。"
        }
        (Language::SimplifiedChinese, "mode.custom.help") => {
            "手动选择便携模式、实例目录和兼容格式。"
        }
        (Language::SimplifiedChinese, "custom.instance") => "实例目录：",
        (Language::SimplifiedChinese, "custom.format") => "目录格式：",
        (Language::SimplifiedChinese, "custom.register") => "把这个目录注册给启动器",
        (Language::SimplifiedChinese, "portable") => "开启便携模式（UserData 放在启动器目录）",
        (Language::SimplifiedChinese, "install.button") => "开始安装",
        (Language::SimplifiedChinese, "install.archive") => "查看安装包",
        (Language::SimplifiedChinese, "install.back") => "返回",
        (Language::SimplifiedChinese, "status.ready") => "准备安装",
        (Language::SimplifiedChinese, "archive.title") => "安装包查看器",
        (Language::SimplifiedChinese, "archive.filter") => "筛选：",
        (Language::SimplifiedChinese, "archive.extract") => "将全部文件解压到普通文件夹：",
        (Language::SimplifiedChinese, "archive.destination") => "目标文件夹：",
        (Language::SimplifiedChinese, "archive.extract_all") => "解压全部",
        (Language::SimplifiedChinese, "archive.install") => "返回安装",
        (Language::SimplifiedChinese, "archive.back") => "返回",
        (Language::SimplifiedChinese, "progress.installing") => "正在安装 LunaLauncher...",
        (Language::SimplifiedChinese, "language") => "语言",

        (Language::English, "first.title") => "Open LunaLauncher package",
        (Language::English, "first.subtitle") => "Choose what to do with this installer package.",
        (Language::English, "first.intro") => {
            "Install it directly, extract the files manually, or inspect the archive first."
        }
        (Language::English, "first.install") => "Install LunaLauncher",
        (Language::English, "first.extract") => "Extract files only",
        (Language::English, "first.view") => "View package contents",
        (Language::English, "first.ready") => "archive entries are ready to inspect.",
        (Language::English, "first.complete") => {
            "The installation is recorded only after the wizard completes successfully."
        }
        (Language::English, "first.leave") => {
            "If you close this window, the choice page will be shown again next time."
        }
        (Language::English, "install.title") => "Install LunaLauncher",
        (Language::English, "install.subtitle") => {
            "Follow these steps. The recommended defaults work for most users."
        }
        (Language::English, "step.location") => "1. Install location",
        (Language::English, "step.ui") => "2. Launcher UI",
        (Language::English, "step.data") => "3. Data and instance folders",
        (Language::English, "step.shortcuts") => "4. Shortcuts",
        (Language::English, "install.directory") => "Install to:",
        (Language::English, "install.shell_dir") => {
            "Vanilla/HMCL/PCL mode puts the files in a LunaLauncher subfolder beside this shell."
        }
        (Language::English, "install.ui") => "Open on launch:",
        (Language::English, "ui.classic") => "Classic UI",
        (Language::English, "ui.neo") => "NeoUI",
        (Language::English, "ui.both") => "Ask every time",
        (Language::English, "ui.help") => {
            "Both launchers are installed; this choice controls shell startup behavior."
        }
        (Language::English, "ui.preview") => "UI preview:",
        (Language::English, "shortcuts.desktop") => "Create desktop shortcut",
        (Language::English, "shortcuts.start") => "Create Start Menu shortcut",
        (Language::English, "data.title") => "Launcher data and instance folders",
        (Language::English, "data.help") => {
            "This controls where game instances are stored and whether PCL/HMCL can discover them."
        }
        (Language::English, "mode.mmc") => "MMC / Prism",
        (Language::English, "mode.vanilla") => "Vanilla / HMCL / PCL",
        (Language::English, "mode.custom") => "Custom",
        (Language::English, "mode.mmc.help") => {
            "For LunaLauncher-only setups: keep data in the system folder and do not register the outer folder."
        }
        (Language::English, "mode.vanilla.help") => {
            "Recommended for PCL/HMCL users: keep portable data beside the launcher and register the outer folder."
        }
        (Language::English, "mode.custom.help") => {
            "Choose portable mode, the instance folder, and its compatibility format yourself."
        }
        (Language::English, "custom.instance") => "Instance folder:",
        (Language::English, "custom.format") => "Folder format:",
        (Language::English, "custom.register") => "Register this folder with the launcher",
        (Language::English, "portable") => {
            "Enable portable mode (put UserData beside the launcher)"
        }
        (Language::English, "install.button") => "Install",
        (Language::English, "install.archive") => "View package",
        (Language::English, "install.back") => "Back",
        (Language::English, "status.ready") => "Ready to install",
        (Language::English, "archive.title") => "Package viewer",
        (Language::English, "archive.filter") => "Filter:",
        (Language::English, "archive.extract") => "Extract all files to a normal folder:",
        (Language::English, "archive.destination") => "Destination:",
        (Language::English, "archive.extract_all") => "Extract all",
        (Language::English, "archive.install") => "Install...",
        (Language::English, "archive.back") => "Back",
        (Language::English, "progress.installing") => "Installing LunaLauncher...",
        (Language::English, "language") => "Language",
        _ => "?",
    }
}
