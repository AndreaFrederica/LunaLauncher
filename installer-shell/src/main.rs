use std::path::{Path, PathBuf};
use std::sync::{Arc, mpsc};
use std::time::Duration;

use anyhow::{Context, Result};
use clap::Parser;
use eframe::egui;

use luna_installer_shell::{archive, config, platform, sfx::PayloadSource};

#[derive(Debug, Parser)]
#[command(
    name = "luna-installer-shell",
    version,
    about = "LunaLauncher installer and SFX shell"
)]
struct Args {
    /// Use an external ZIP instead of the ZIP appended to this executable.
    #[arg(long)]
    payload: Option<PathBuf>,
    /// Extract the payload into this directory and exit.
    #[arg(long)]
    extract: Option<PathBuf>,
    /// List payload entries and exit.
    #[arg(long)]
    list: bool,
    /// Install using the saved configuration or the platform default and exit.
    #[arg(long)]
    install: bool,
    /// Override the installation directory for --install.
    #[arg(long, value_name = "DIR")]
    install_dir: Option<PathBuf>,
    /// Do not launch the selected entry point after installation.
    #[arg(long)]
    no_launch: bool,
    /// Reset installer configuration and exit.
    #[arg(long)]
    reset_config: bool,
    /// Uninstall an existing installation and exit.
    #[arg(long, value_name = "DIR")]
    uninstall: Option<PathBuf>,
    /// Remove the user data directory during uninstall.
    #[arg(long)]
    purge: bool,
}

fn main() -> Result<()> {
    let args = Args::parse();
    if args.reset_config {
        config::reset()?;
        return Ok(());
    }
    if let Some(install_dir) = args.uninstall {
        uninstall(&install_dir, args.purge)?;
        return Ok(());
    }
    // The shell copied into the installation directory becomes the user's
    // launcher entry point after the first install. Opening that copy again
    // should launch the selected UI directly instead of reopening the wizard.
    if args.payload.is_none()
        && args.extract.is_none()
        && !args.list
        && !args.install
        && !args.no_launch
        && let Some(saved) = config::load()?.filter(|saved| {
            current_executable_is_in_install_dir(&saved.install_dir)
        })
    {
        platform::launch(&saved.install_dir, saved.entry_point)?;
        return Ok(());
    }
    let source = payload_source(args.payload)?;
    if args.list {
        for name in archive::list(&source)? {
            println!("{name}");
        }
        return Ok(());
    }
    if let Some(destination) = args.extract {
        let count = archive::extract(&source, &destination)?;
        println!("Extracted {count} files to {}", destination.display());
        return Ok(());
    }
    if args.install {
        let mut existing = config::load()?.unwrap_or_else(|| config::InstallConfig {
            install_dir: PathBuf::from(default_install_dir()),
            installed_entries: Vec::new(),
            entry_point: config::EntryPoint::Classic,
            desktop_shortcuts: false,
            start_menu_shortcuts: false,
            keep_user_data: true,
            instance_layout: config::InstanceLayoutMode::Mmc,
            portable_mode: false,
            register_instance_store: false,
            instance_store_path: None,
            instance_store_format: config::InstanceStoreFormat::Prism,
        });
        if let Some(install_dir) = args.install_dir {
            existing.install_dir = install_dir;
        }
        let setup = LauncherSetup {
            instance_layout: existing.instance_layout,
            portable_mode: existing.portable_mode,
            register_instance_store: existing.register_instance_store,
            instance_store_path: existing.instance_store_path.clone(),
            instance_store_format: existing.instance_store_format,
        };
        let count = install(
            &source,
            &existing.install_dir,
            existing.entry_point,
            existing.desktop_shortcuts,
            existing.start_menu_shortcuts,
            setup,
            !args.no_launch,
        )?;
        println!(
            "Installed {count} files to {}",
            existing.install_dir.display()
        );
        return Ok(());
    }

    let installer_font = archive::read_entry(&source, "resource/fonts/SarasaUiSC-Regular.ttf").ok();
    let native_options = eframe::NativeOptions::default();
    eframe::run_native(
        "LunaLauncher Installer",
        native_options,
        Box::new(move |cc| {
            install_shell_font(&cc.egui_ctx, installer_font);
            Ok(Box::new(InstallerApp::new(source)))
        }),
    )
    .map_err(|error| anyhow::anyhow!("run installer UI: {error}"))
}

fn install_shell_font(context: &egui::Context, font: Option<Vec<u8>>) {
    let Some(font) = font else {
        return;
    };
    let mut definitions = egui::FontDefinitions::default();
    let name = "LunaLauncherSarasa".to_owned();
    definitions
        .font_data
        .insert(name.clone(), Arc::new(egui::FontData::from_owned(font)));
    for family in [egui::FontFamily::Proportional, egui::FontFamily::Monospace] {
        if let Some(fonts) = definitions.families.get_mut(&family) {
            fonts.insert(0, name.clone());
        }
    }
    context.set_fonts(definitions);
}

fn current_executable_is_in_install_dir(install_dir: &Path) -> bool {
    let Ok(executable) = std::env::current_exe() else {
        return false;
    };
    executable
        .parent()
        .and_then(|directory| directory.canonicalize().ok())
        == install_dir.canonicalize().ok()
}

fn current_executable_directory() -> Option<PathBuf> {
    std::env::current_exe()
        .ok()
        .and_then(|path| path.parent().map(Path::to_path_buf))
}

fn payload_source(external: Option<PathBuf>) -> Result<PayloadSource> {
    if let Some(path) = external {
        return PayloadSource::from_path(path);
    }
    let executable = std::env::current_exe().context("locate installer executable")?;
    let source = PayloadSource::from_path(executable)?;
    if source.footer.is_none() {
        anyhow::bail!("this executable has no appended SFX payload; pass --payload <zip>");
    }
    Ok(source)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum InstallerScreen {
    FirstRun,
    Install,
    ArchiveViewer,
}

struct InstallerApp {
    source: PayloadSource,
    entries: Vec<String>,
    entries_error: Option<String>,
    screen: InstallerScreen,
    first_run: bool,
    destination: String,
    extract_destination: String,
    search: String,
    selected_entry: Option<usize>,
    status: String,
    entry_point: config::EntryPoint,
    desktop_shortcuts: bool,
    start_menu_shortcuts: bool,
    instance_layout: config::InstanceLayoutMode,
    portable_mode: bool,
    register_instance_store: bool,
    instance_store_path: String,
    instance_store_format: config::InstanceStoreFormat,
    install_receiver: Option<mpsc::Receiver<InstallMessage>>,
    installing: bool,
    install_progress: f32,
    install_progress_text: String,
}

impl InstallerApp {
    fn new(source: PayloadSource) -> Self {
        let existing = config::load().ok().flatten();
        let first_run = existing.is_none();
        let destination = existing
            .as_ref()
            .map(|config| config.install_dir.display().to_string())
            .unwrap_or_else(default_install_dir);
        let extract_destination = format!("{}-extracted", destination);
        let default_instance_store_path = destination.clone();
        let (entries, entries_error) = match archive::list(&source) {
            Ok(entries) => (entries, None),
            Err(error) => (Vec::new(), Some(format!("Cannot read archive: {error:#}"))),
        };
        Self {
            source,
            entries,
            entries_error,
            screen: if first_run {
                InstallerScreen::FirstRun
            } else {
                InstallerScreen::Install
            },
            first_run,
            destination,
            extract_destination,
            search: String::new(),
            selected_entry: None,
            status: "Ready to install".to_owned(),
            entry_point: existing
                .as_ref()
                .map(|config| config.entry_point)
                .unwrap_or_default(),
            desktop_shortcuts: existing
                .as_ref()
                .is_some_and(|config| config.desktop_shortcuts),
            start_menu_shortcuts: existing
                .as_ref()
                .is_some_and(|config| config.start_menu_shortcuts),
            instance_layout: existing
                .as_ref()
                .map(|config| config.instance_layout)
                .unwrap_or_default(),
            portable_mode: existing.as_ref().is_some_and(|config| config.portable_mode),
            register_instance_store: existing
                .as_ref()
                .is_some_and(|config| config.register_instance_store),
            instance_store_path: existing
                .as_ref()
                .and_then(|config| config.instance_store_path.as_ref())
                .map(|path| path.display().to_string())
                .unwrap_or(default_instance_store_path),
            instance_store_format: existing
                .as_ref()
                .map(|config| config.instance_store_format)
                .unwrap_or_default(),
            install_receiver: None,
            installing: false,
            install_progress: 0.0,
            install_progress_text: String::new(),
        }
    }
}

impl eframe::App for InstallerApp {
    fn update(&mut self, context: &egui::Context, _frame: &mut eframe::Frame) {
        self.poll_install(context);
        egui::CentralPanel::default().show(context, |ui| match self.screen {
            InstallerScreen::FirstRun => self.show_first_run(ui),
            InstallerScreen::Install => self.show_install(ui),
            InstallerScreen::ArchiveViewer => self.show_archive_viewer(ui),
        });
    }
}

impl InstallerApp {
    fn poll_install(&mut self, context: &egui::Context) {
        let Some(receiver) = self.install_receiver.take() else {
            return;
        };
        let mut keep_receiver = true;
        for message in receiver.try_iter() {
            match message {
                InstallMessage::Progress { fraction, text } => {
                    self.install_progress = fraction.clamp(0.0, 1.0);
                    self.install_progress_text = text;
                }
                InstallMessage::Finished(result) => {
                    self.installing = false;
                    keep_receiver = false;
                    match result {
                        Ok(count) => {
                            self.first_run = false;
                            self.status = format!("Installed {count} files successfully");
                            self.install_progress = 1.0;
                            self.install_progress_text = "Installation complete".to_owned();
                        }
                        Err(error) => {
                            self.status = format!("Install failed: {error}");
                            self.install_progress_text = "Installation failed".to_owned();
                        }
                    }
                }
            }
        }
        if keep_receiver {
            self.install_receiver = Some(receiver);
            context.request_repaint_after(Duration::from_millis(50));
        }
    }

    fn show_first_run(&mut self, ui: &mut egui::Ui) {
        ui.heading("Open LunaLauncher package");
        ui.label("Choose what to do with this package before any installation settings are saved.");
        ui.label("You can install it, extract its files manually, or inspect the archive first.");
        ui.separator();
        if ui.button("Install LunaLauncher").clicked() {
            self.screen = InstallerScreen::Install;
        }
        if ui.button("Extract files only").clicked() {
            self.screen = InstallerScreen::ArchiveViewer;
            self.status = "Choose a destination, then extract the complete archive.".to_owned();
        }
        if ui.button("View archive contents").clicked() {
            self.screen = InstallerScreen::ArchiveViewer;
            self.status = "Archive opened in read-only mode.".to_owned();
        }
        if let Some(error) = &self.entries_error {
            ui.separator();
            ui.colored_label(egui::Color32::RED, error);
        } else {
            ui.separator();
            ui.label(format!(
                "{} archive entries are ready to inspect.",
                self.entries.len()
            ));
        }
        ui.label("Installation is considered complete only after the Install button succeeds.");
        ui.label("Leaving this window before then will show this choice screen again next time.");
    }

    fn show_install(&mut self, ui: &mut egui::Ui) {
        ui.heading("LunaLauncher Installer");
        ui.label("The existing launcher files will be extracted without changing their layout.");
        if self.installing {
            ui.separator();
            ui.label("Installing LunaLauncher...");
            ui.add(egui::ProgressBar::new(self.install_progress).show_percentage());
            ui.label(&self.install_progress_text);
            return;
        }
        ui.separator();
        ui.horizontal(|ui| {
            ui.label("Install directory:");
            if self.instance_layout == config::InstanceLayoutMode::Vanilla {
                ui.label(&self.destination);
            } else {
                ui.text_edit_singleline(&mut self.destination);
            }
        });
        ui.horizontal(|ui| {
            ui.label("Default UI:");
            ui.radio_value(
                &mut self.entry_point,
                config::EntryPoint::Classic,
                "Classic UI",
            );
            ui.radio_value(&mut self.entry_point, config::EntryPoint::Neo, "NeoUI");
            ui.radio_value(&mut self.entry_point, config::EntryPoint::Both, "Both");
        });
        ui.checkbox(&mut self.desktop_shortcuts, "Create desktop shortcut(s)");
        ui.checkbox(
            &mut self.start_menu_shortcuts,
            "Create Start Menu shortcut(s)",
        );
        ui.separator();
        ui.label("Launcher data and instance directory");
        ui.label("Choose how the directory containing the installed shell is handled.");
        let previous_layout = self.instance_layout;
        ui.horizontal(|ui| {
            ui.radio_value(
                &mut self.instance_layout,
                config::InstanceLayoutMode::Mmc,
                "MMC",
            );
            ui.radio_value(
                &mut self.instance_layout,
                config::InstanceLayoutMode::Vanilla,
                "原版 / HMCL / PCL",
            );
            ui.radio_value(
                &mut self.instance_layout,
                config::InstanceLayoutMode::Custom,
                "自定义",
            );
        });
        if self.instance_layout != previous_layout {
            self.apply_instance_layout_defaults();
        }
        match self.instance_layout {
            config::InstanceLayoutMode::Mmc => {
                ui.label("MMC：非便携模式，不注册当前安装目录为实例目录。");
            }
            config::InstanceLayoutMode::Vanilla => {
                ui.label("原版：便携模式，并注册当前安装目录为原版/HMCL/PCL 实例目录。");
            }
            config::InstanceLayoutMode::Custom => {
                ui.horizontal(|ui| {
                    ui.label("实例目录：");
                    ui.text_edit_singleline(&mut self.instance_store_path);
                });
                ui.horizontal(|ui| {
                    ui.label("实例格式：");
                    ui.radio_value(
                        &mut self.instance_store_format,
                        config::InstanceStoreFormat::Prism,
                        "MMC / Prism",
                    );
                    ui.radio_value(
                        &mut self.instance_store_format,
                        config::InstanceStoreFormat::VanillaCompatible,
                        "原版 / HMCL / PCL",
                    );
                });
                ui.checkbox(&mut self.register_instance_store, "注册这个实例目录");
            }
        }
        if ui
            .checkbox(
                &mut self.portable_mode,
                "开启便携模式（UserData 放在启动器目录）",
            )
            .changed()
            && self.instance_layout != config::InstanceLayoutMode::Custom
        {
            // Presets provide the requested defaults. Editing this choice turns
            // the selection into a custom setup so it is preserved explicitly.
            self.instance_layout = config::InstanceLayoutMode::Custom;
        }
        if ui.button("Install").clicked() {
            let destination = PathBuf::from(self.destination.trim());
            let setup = self.launcher_setup(&destination);
            self.start_install(destination, setup);
        }
        ui.separator();
        ui.label(&self.status);
        ui.horizontal(|ui| {
            if self.first_run && ui.button("Back").clicked() {
                self.screen = InstallerScreen::FirstRun;
            }
            if ui.button("View archive").clicked() {
                self.screen = InstallerScreen::ArchiveViewer;
            }
        });
    }

    fn start_install(&mut self, destination: PathBuf, setup: LauncherSetup) {
        if self.installing {
            return;
        }
        let (sender, receiver) = mpsc::channel();
        let source = self.source.clone();
        let entry_point = self.entry_point;
        let desktop_shortcuts = self.desktop_shortcuts;
        let start_menu_shortcuts = self.start_menu_shortcuts;
        self.install_receiver = Some(receiver);
        self.installing = true;
        self.install_progress = 0.0;
        self.install_progress_text = "Preparing installation...".to_owned();
        self.status = "Installation in progress".to_owned();
        std::thread::spawn(move || {
            let result = {
                let mut report = |fraction: f32, text: &str| {
                    let _ = sender.send(InstallMessage::Progress {
                        fraction,
                        text: text.to_owned(),
                    });
                };
                install_with_progress(
                    &source,
                    &destination,
                    entry_point,
                    desktop_shortcuts,
                    start_menu_shortcuts,
                    setup,
                    true,
                    &mut report,
                )
                .map_err(|error| format!("{error:#}"))
            };
            let _ = sender.send(InstallMessage::Finished(result));
        });
    }

    fn apply_instance_layout_defaults(&mut self) {
        match self.instance_layout {
            config::InstanceLayoutMode::Mmc => {
                self.portable_mode = false;
                self.register_instance_store = false;
                self.instance_store_format = config::InstanceStoreFormat::Prism;
            }
            config::InstanceLayoutMode::Vanilla => {
                self.portable_mode = true;
                self.register_instance_store = true;
                self.instance_store_format = config::InstanceStoreFormat::VanillaCompatible;
                if let Some(shell_directory) = current_executable_directory() {
                    self.destination = shell_directory.display().to_string();
                    self.extract_destination = format!("{}-extracted", self.destination);
                }
                self.instance_store_path = self.destination.clone();
            }
            config::InstanceLayoutMode::Custom => {}
        }
    }

    fn launcher_setup(&self, destination: &Path) -> LauncherSetup {
        match self.instance_layout {
            config::InstanceLayoutMode::Mmc => LauncherSetup {
                instance_layout: config::InstanceLayoutMode::Mmc,
                portable_mode: false,
                register_instance_store: false,
                instance_store_path: None,
                instance_store_format: config::InstanceStoreFormat::Prism,
            },
            config::InstanceLayoutMode::Vanilla => LauncherSetup {
                instance_layout: config::InstanceLayoutMode::Vanilla,
                portable_mode: true,
                register_instance_store: true,
                instance_store_path: Some(destination.to_path_buf()),
                instance_store_format: config::InstanceStoreFormat::VanillaCompatible,
            },
            config::InstanceLayoutMode::Custom => LauncherSetup {
                instance_layout: config::InstanceLayoutMode::Custom,
                portable_mode: self.portable_mode,
                register_instance_store: self.register_instance_store,
                instance_store_path: if self.register_instance_store {
                    Some(PathBuf::from(self.instance_store_path.trim()))
                } else {
                    None
                },
                instance_store_format: self.instance_store_format,
            },
        }
    }

    fn show_archive_viewer(&mut self, ui: &mut egui::Ui) {
        ui.heading("Archive viewer");
        if let Some(error) = &self.entries_error {
            ui.colored_label(egui::Color32::RED, error);
        }
        ui.label(format!("{} entries", self.entries.len()));
        ui.horizontal(|ui| {
            ui.label("Filter:");
            ui.text_edit_singleline(&mut self.search);
        });
        ui.separator();
        let search = self.search.to_lowercase();
        egui::ScrollArea::vertical()
            .id_salt("archive-entries")
            .max_height(320.0)
            .show(ui, |ui| {
                for (index, name) in self.entries.iter().enumerate() {
                    if !search.is_empty() && !name.to_lowercase().contains(&search) {
                        continue;
                    }
                    let label = if name.ends_with('/') {
                        format!("📁 {name}")
                    } else {
                        format!("📄 {name}")
                    };
                    if ui
                        .selectable_label(self.selected_entry == Some(index), label)
                        .clicked()
                    {
                        self.selected_entry = Some(index);
                    }
                }
            });
        ui.separator();
        ui.label("Extract a normal directory that you can browse or edit manually:");
        ui.horizontal(|ui| {
            ui.label("Destination:");
            ui.text_edit_singleline(&mut self.extract_destination);
            if ui.button("Extract all").clicked() {
                let destination = PathBuf::from(self.extract_destination.trim());
                match archive::extract(&self.source, &destination) {
                    Ok(count) => {
                        self.status =
                            format!("Extracted {count} files to {}", destination.display())
                    }
                    Err(error) => self.status = format!("Extraction failed: {error:#}"),
                }
            }
        });
        ui.label(&self.status);
        ui.horizontal(|ui| {
            if ui.button("Install...").clicked() {
                self.screen = InstallerScreen::Install;
            }
            if ui.button("Back").clicked() {
                self.screen = if self.first_run {
                    InstallerScreen::FirstRun
                } else {
                    InstallerScreen::Install
                };
            }
        });
    }
}

#[derive(Debug, Clone)]
struct LauncherSetup {
    instance_layout: config::InstanceLayoutMode,
    portable_mode: bool,
    register_instance_store: bool,
    instance_store_path: Option<PathBuf>,
    instance_store_format: config::InstanceStoreFormat,
}

#[derive(Debug)]
enum InstallMessage {
    Progress { fraction: f32, text: String },
    Finished(Result<usize, String>),
}

fn install(
    source: &PayloadSource,
    destination: &Path,
    entry_point: config::EntryPoint,
    desktop_shortcuts: bool,
    start_menu_shortcuts: bool,
    setup: LauncherSetup,
    launch_after: bool,
) -> Result<usize> {
    let mut no_progress = |_: f32, _: &str| {};
    install_with_progress(
        source,
        destination,
        entry_point,
        desktop_shortcuts,
        start_menu_shortcuts,
        setup,
        launch_after,
        &mut no_progress,
    )
}

fn install_with_progress(
    source: &PayloadSource,
    destination: &Path,
    entry_point: config::EntryPoint,
    desktop_shortcuts: bool,
    start_menu_shortcuts: bool,
    setup: LauncherSetup,
    launch_after: bool,
    progress: &mut dyn FnMut(f32, &str),
) -> Result<usize> {
    progress(0.02, "Validating package...");
    let installed_entries = archive::list(source)?;
    for (_, target) in platform::entry_targets(destination, entry_point) {
        let expected_name = target
            .file_name()
            .context("launcher entry point has no file name")?
            .to_string_lossy();
        if !installed_entries.iter().any(|entry| entry == expected_name.as_ref()) {
            anyhow::bail!(
                "package is missing the selected launcher entry point: {expected_name}"
            );
        }
    }
    progress(0.05, "Extracting launcher files...");
    // A self-extracting shell can be placed directly in the directory it is
    // installing into. Windows cannot rename a directory containing the
    // currently running executable, so use a verified in-place extraction in
    // that one case; all other installs keep the atomic replacement path.
    let mut extraction_progress = |done: usize, total: usize| {
        let fraction = 0.05 + 0.73 * (done as f32 / total.max(1) as f32);
        progress(
            fraction,
            &format!("Extracting launcher files ({done}/{total})..."),
        );
    };
    let count = if current_executable_directory().is_some_and(|directory| {
        directory.canonicalize().ok() == destination.canonicalize().ok()
    }) {
        archive::extract_with_progress(source, destination, &mut extraction_progress)?
    } else {
        archive::extract_atomic_with_progress(
            source,
            destination,
            true,
            &mut extraction_progress,
        )?
    };
    progress(0.80, "Installing shell and shortcuts...");
    install_uninstaller_copy(destination)?;
    platform::create_shortcuts(
        destination,
        entry_point,
        desktop_shortcuts,
        start_menu_shortcuts,
    )?;
    platform::create_uninstall_shortcut(destination, desktop_shortcuts, start_menu_shortcuts)?;
    progress(0.90, "Configuring launcher data directories...");
    platform::configure_launcher(
        destination,
        setup.portable_mode,
        setup.register_instance_store,
        setup.instance_store_path.as_deref(),
        setup.instance_store_format,
    )?;
    if launch_after {
        progress(0.96, "Starting selected launcher UI...");
        platform::launch(destination, entry_point)?;
    }
    // Persist the completed wizard only after extraction, shortcuts, and the
    // optional launch all succeed. A cancelled or failed first run therefore
    // remains on the first-run choice screen next time.
    config::save(&config::InstallConfig {
        install_dir: destination.to_path_buf(),
        installed_entries,
        entry_point,
        desktop_shortcuts,
        start_menu_shortcuts,
        keep_user_data: true,
        instance_layout: setup.instance_layout,
        portable_mode: setup.portable_mode,
        register_instance_store: setup.register_instance_store,
        instance_store_path: setup.instance_store_path,
        instance_store_format: setup.instance_store_format,
    })?;
    progress(1.0, "Installation complete");
    Ok(count)
}

fn install_uninstaller_copy(destination: &Path) -> Result<()> {
    let source = std::env::current_exe().context("locate installer shell for uninstall copy")?;
    let target = destination.join(if cfg!(windows) {
        "luna-installer-shell.exe"
    } else {
        "luna-installer-shell"
    });
    if source.canonicalize().ok().as_deref() == target.canonicalize().ok().as_deref() {
        return Ok(());
    }
    std::fs::copy(&source, &target).with_context(|| {
        format!(
            "copy installer shell to uninstall path: {}",
            target.display()
        )
    })?;
    Ok(())
}

fn uninstall(install_dir: &Path, purge: bool) -> Result<()> {
    let install_dir = install_dir
        .canonicalize()
        .with_context(|| format!("resolve installation directory: {}", install_dir.display()))?;
    if install_dir.parent().is_none() || install_dir == install_dir.parent().unwrap() {
        anyhow::bail!("refusing to uninstall a filesystem root");
    }
    let saved = config::load()?.filter(|config| {
        config::is_within_install_dir(&install_dir, &config.install_dir)
            && config::is_within_install_dir(&config.install_dir, &install_dir)
    });
    if let Some(config) = saved {
        platform::remove_shortcuts(
            &install_dir,
            config.entry_point,
            config.desktop_shortcuts,
            config.start_menu_shortcuts,
        )?;
    }
    config::reset()?;
    platform::schedule_remove_installation(&install_dir, !purge)?;
    println!("Uninstall scheduled for {}", install_dir.display());
    Ok(())
}

fn default_install_dir() -> String {
    directories::BaseDirs::new()
        .map(|dirs| {
            dirs.data_local_dir()
                .join("LunaLauncher")
                .display()
                .to_string()
        })
        .unwrap_or_else(|| "LunaLauncher".to_owned())
}
