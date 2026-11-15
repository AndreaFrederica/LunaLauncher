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
        (Language::SimplifiedChinese, "browse") => "浏览…",
        (Language::SimplifiedChinese, "install.shell_dir") => {
            "原版/HMCL/PCL 模式会把文件放进当前 shell 目录下的 LunaLauncher 子目录。"
        }
        (Language::SimplifiedChinese, "wizard.ui") => "界面",
        (Language::SimplifiedChinese, "wizard.method") => "安装方式",
        (Language::SimplifiedChinese, "wizard.location") => "路径",
        (Language::SimplifiedChinese, "wizard.data") => "数据",
        (Language::SimplifiedChinese, "wizard.shortcuts") => "快捷方式",
        (Language::SimplifiedChinese, "wizard.review") => "确认",
        (Language::SimplifiedChinese, "wizard.extract") => "解压",
        (Language::SimplifiedChinese, "wizard.ui_title") => "选择启动界面",
        (Language::SimplifiedChinese, "wizard.ui_help") => {
            "先选择安装后默认打开的界面；两个界面都会随安装包一起安装。"
        }
        (Language::SimplifiedChinese, "wizard.method_title") => "选择安装方式",
        (Language::SimplifiedChinese, "wizard.method_help") => {
            "选择启动器数据和实例目录的布局。之后仍可返回修改。"
        }
        (Language::SimplifiedChinese, "wizard.location_title") => "选择安装路径",
        (Language::SimplifiedChinese, "wizard.location_help") => {
            "程序文件会安装到下面的目录。用户数据和实例目录由下一步决定。"
        }
        (Language::SimplifiedChinese, "wizard.data_title") => "配置数据和实例目录",
        (Language::SimplifiedChinese, "wizard.shortcuts_title") => "选择快捷方式",
        (Language::SimplifiedChinese, "wizard.shortcuts_help") => {
            "安装完成后可以从桌面或开始菜单启动 LunaLauncher。"
        }
        (Language::SimplifiedChinese, "wizard.review_title") => "确认安装选项",
        (Language::SimplifiedChinese, "wizard.shortcut_summary") => "快捷方式（桌面 / 开始菜单）：",
        (Language::SimplifiedChinese, "wizard.back") => "上一步",
        (Language::SimplifiedChinese, "wizard.next") => "下一步",
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
        (Language::SimplifiedChinese, "data.system_path") => "系统数据目录：",
        (Language::SimplifiedChinese, "data.path") => "启动器数据目录：",
        (Language::SimplifiedChinese, "data.reset") => "恢复默认",
        (Language::SimplifiedChinese, "data.path_help") => "保存设置、账号、缓存及默认实例。可选择已有数据目录；修改路径不会自动搬迁旧数据。",
        (Language::SimplifiedChinese, "data.portable_path") => "便携数据目录：",
        (Language::SimplifiedChinese, "data.extra_store") => "额外注册一个实例目录",
        (Language::SimplifiedChinese, "data.help") => {
            "这个选项决定游戏实例放在哪里，以及能否被 PCL/HMCL 识别。"
        }
        (Language::SimplifiedChinese, "mode.mmc") => "MMC / Prism",
        (Language::SimplifiedChinese, "mode.vanilla") => "原版 / HMCL / PCL",
        (Language::SimplifiedChinese, "mode.custom") => "自定义",
        (Language::SimplifiedChinese, "mode.extract") => "自解压",
        (Language::SimplifiedChinese, "mode.mmc.help") => {
            "标准安装模式：程序与数据分开存放，数据目录可修改，不把程序目录注册为实例目录；同时提供卸载、协议和文件关联。"
        }
        (Language::SimplifiedChinese, "mode.vanilla.help") => {
            "适合从 PCL/HMCL 迁移：探测已有游戏目录，核对各版本的隔离设置后再安装。"
        }
        (Language::SimplifiedChinese, "mode.custom.help") => {
            "手动选择便携模式、实例目录和兼容格式。"
        }
        (Language::SimplifiedChinese, "mode.extract.help") => {
            "只解压安装包，不登记为已安装程序；可以选择便携模式和快捷方式。"
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
        (Language::SimplifiedChinese, "progress.extracting") => "正在解压 LunaLauncher...",
        (Language::SimplifiedChinese, "progress.details") => "详细日志：",
        (Language::SimplifiedChinese, "extract.title") => "正在解压安装包",
        (Language::SimplifiedChinese, "extract.help") => {
            "正在将文件解压到你选择的目录，不会登记安装状态。"
        }
        (Language::SimplifiedChinese, "extract.options") => {
            "自解压选项：便携模式和快捷方式将在解压后应用。"
        }
        (Language::SimplifiedChinese, "extract.button") => "开始解压",
        (Language::SimplifiedChinese, "language") => "语言",
        (Language::SimplifiedChinese, "footer.view") => "查看安装包内容",
        (Language::SimplifiedChinese, "footer.open_archive") => "用压缩软件打开当前文件",
        (Language::SimplifiedChinese, "footer.opened") => "已打开压缩软件。",
        (Language::SimplifiedChinese, "footer.open_failed") => "无法打开压缩软件",

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
        (Language::English, "browse") => "Browse…",
        (Language::English, "install.shell_dir") => {
            "Vanilla/HMCL/PCL mode puts the files in a LunaLauncher subfolder beside this shell."
        }
        (Language::English, "wizard.ui") => "UI",
        (Language::English, "wizard.method") => "Install mode",
        (Language::English, "wizard.location") => "Location",
        (Language::English, "wizard.data") => "Data",
        (Language::English, "wizard.shortcuts") => "Shortcuts",
        (Language::English, "wizard.review") => "Confirm",
        (Language::English, "wizard.extract") => "Extract",
        (Language::English, "wizard.ui_title") => "Choose the launcher UI",
        (Language::English, "wizard.ui_help") => {
            "Choose which interface opens by default. Both interfaces are installed with the package."
        }
        (Language::English, "wizard.method_title") => "Choose an install mode",
        (Language::English, "wizard.method_help") => {
            "Choose how launcher data and instance folders are laid out. You can go back and change it later."
        }
        (Language::English, "wizard.location_title") => "Choose an install location",
        (Language::English, "wizard.location_help") => {
            "Program files go in this folder. The next step controls user data and instances."
        }
        (Language::English, "wizard.data_title") => "Configure data and instance folders",
        (Language::English, "wizard.shortcuts_title") => "Choose shortcuts",
        (Language::English, "wizard.shortcuts_help") => {
            "After installation, launch LunaLauncher from the desktop or Start menu."
        }
        (Language::English, "wizard.review_title") => "Confirm installation options",
        (Language::English, "wizard.shortcut_summary") => "Shortcuts (desktop / Start menu):",
        (Language::English, "wizard.back") => "Back",
        (Language::English, "wizard.next") => "Next",
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
        (Language::English, "data.system_path") => "System data folder:",
        (Language::English, "data.path") => "Launcher data folder:",
        (Language::English, "data.reset") => "Reset to default",
        (Language::English, "data.path_help") => "Stores settings, accounts, cache and default instances. Existing data can be used here; changing the path does not move old data automatically.",
        (Language::English, "data.portable_path") => "Portable data folder:",
        (Language::English, "data.extra_store") => "Register an additional instance folder",
        (Language::English, "data.help") => {
            "This controls where game instances are stored and whether PCL/HMCL can discover them."
        }
        (Language::English, "mode.mmc") => "MMC / Prism",
        (Language::English, "mode.vanilla") => "Vanilla / HMCL / PCL",
        (Language::English, "mode.custom") => "Custom",
        (Language::English, "mode.extract") => "Self-extract",
        (Language::English, "mode.mmc.help") => {
            "Standard installer mode: keep binaries and data separate, choose a data folder, and include the uninstaller, URL handlers, and file associations."
        }
        (Language::English, "mode.vanilla.help") => {
            "For PCL/HMCL migration: detect existing game folders and review each version’s directory mode before installing."
        }
        (Language::English, "mode.custom.help") => {
            "Choose portable mode, the instance folder, and its compatibility format yourself."
        }
        (Language::English, "mode.extract.help") => {
            "Extract the package without registering an installed application; portable mode and shortcuts are optional."
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
        (Language::English, "progress.extracting") => "Extracting LunaLauncher...",
        (Language::English, "progress.details") => "Detailed log:",
        (Language::English, "extract.title") => "Extracting package",
        (Language::English, "extract.help") => {
            "Files are being extracted to the selected folder without registering an installation."
        }
        (Language::English, "extract.options") => {
            "Self-extract options: portable mode and shortcuts are applied after extraction."
        }
        (Language::English, "extract.button") => "Extract",
        (Language::English, "language") => "Language",
        (Language::English, "footer.view") => "View package contents",
        (Language::English, "footer.open_archive") => "Open current file with archive manager",
        (Language::English, "footer.opened") => "Archive manager opened.",
        (Language::English, "footer.open_failed") => "Could not open archive manager",
        _ => "?",
    }
}
