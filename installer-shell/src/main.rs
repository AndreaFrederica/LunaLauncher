use std::path::{Path, PathBuf};

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
        });
        if let Some(install_dir) = args.install_dir {
            existing.install_dir = install_dir;
        }
        let count = install(
            &source,
            &existing.install_dir,
            existing.entry_point,
            existing.desktop_shortcuts,
            existing.start_menu_shortcuts,
            !args.no_launch,
        )?;
        println!(
            "Installed {count} files to {}",
            existing.install_dir.display()
        );
        return Ok(());
    }

    let native_options = eframe::NativeOptions::default();
    eframe::run_native(
        "LunaLauncher Installer",
        native_options,
        Box::new(move |_cc| Ok(Box::new(InstallerApp::new(source)))),
    )
    .map_err(|error| anyhow::anyhow!("run installer UI: {error}"))
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
        }
    }
}

impl eframe::App for InstallerApp {
    fn update(&mut self, context: &egui::Context, _frame: &mut eframe::Frame) {
        egui::CentralPanel::default().show(context, |ui| match self.screen {
            InstallerScreen::FirstRun => self.show_first_run(ui),
            InstallerScreen::Install => self.show_install(ui),
            InstallerScreen::ArchiveViewer => self.show_archive_viewer(ui),
        });
    }
}

impl InstallerApp {
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
        ui.separator();
        ui.horizontal(|ui| {
            ui.label("Install directory:");
            ui.text_edit_singleline(&mut self.destination);
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
        if ui.button("Install").clicked() {
            let destination = PathBuf::from(self.destination.trim());
            match install(
                &self.source,
                &destination,
                self.entry_point,
                self.desktop_shortcuts,
                self.start_menu_shortcuts,
                true,
            ) {
                Ok(count) => {
                    self.first_run = false;
                    self.status = format!("Installed {count} files to {}", destination.display());
                }
                Err(error) => self.status = format!("Install failed: {error:#}"),
            }
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

fn install(
    source: &PayloadSource,
    destination: &Path,
    entry_point: config::EntryPoint,
    desktop_shortcuts: bool,
    start_menu_shortcuts: bool,
    launch_after: bool,
) -> Result<usize> {
    let installed_entries = archive::list(source)?;
    let count = archive::extract_atomic(source, destination, true)?;
    install_uninstaller_copy(destination)?;
    platform::create_shortcuts(
        destination,
        entry_point,
        desktop_shortcuts,
        start_menu_shortcuts,
    )?;
    platform::create_uninstall_shortcut(destination, desktop_shortcuts, start_menu_shortcuts)?;
    if launch_after {
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
    })?;
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
