use std::path::{Path, PathBuf};
use std::sync::{Arc, mpsc};
use std::time::Duration;

use anyhow::Result;
use eframe::egui;
use raw_window_handle::HasWindowHandle;
use rfd::FileDialog;

use crate::app::{
    InstallMessage, LauncherSetup, extract_only_with_progress, install_with_progress,
};
use crate::{default_install_dir, vanilla_install_directory};
use luna_installer_shell::{archive, config, i18n, platform, sfx::PayloadSource};

pub(crate) fn run_installer(source: PayloadSource) -> Result<()> {
    let installer_font = archive::read_entry(&source, "resource/fonts/SarasaUiSC-Regular.ttf").ok();
    eframe::run_native(
        "LunaLauncher Installer",
        shell_native_options(),
        Box::new(move |cc| {
            install_shell_font(&cc.egui_ctx, installer_font);
            Ok(Box::new(InstallerApp::new(source, &cc.egui_ctx)))
        }),
    )
    .map_err(|error| anyhow::anyhow!("run installer UI: {error}"))
}

fn shell_native_options() -> eframe::NativeOptions {
    let mut viewport = egui::ViewportBuilder::default()
        .with_inner_size([760.0, 680.0])
        .with_min_inner_size([620.0, 520.0])
        .with_max_inner_size([1100.0, 900.0])
        .with_resizable(true)
        .with_maximize_button(true);
    if let Ok(icon) = eframe::icon_data::from_png_bytes(include_bytes!(
        "../../launcher/exresources/icons/lunalauncher.png"
    )) {
        viewport = viewport.with_icon(icon);
    }
    eframe::NativeOptions {
        viewport,
        ..Default::default()
    }
}

pub(crate) fn run_launch_chooser(
    install_dir: PathBuf,
    monitor: Option<platform::LaunchMonitor>,
    source: PayloadSource,
) -> Result<()> {
    let installer_font = archive::read_entry(&source, "resource/fonts/SarasaUiSC-Regular.ttf").ok();
    let language = i18n::Language::detect();
    eframe::run_native(
        "LunaLauncher",
        launch_chooser_native_options(),
        Box::new(move |cc| {
            install_shell_font(&cc.egui_ctx, installer_font);
            Ok(Box::new(LaunchChooserApp::new(
                install_dir,
                monitor,
                language,
            )))
        }),
    )
    .map_err(|error| anyhow::anyhow!("run launcher chooser: {error}"))
}

fn launch_chooser_native_options() -> eframe::NativeOptions {
    let mut viewport = egui::ViewportBuilder::default()
        .with_inner_size([480.0, 220.0])
        .with_min_inner_size([380.0, 180.0])
        .with_max_inner_size([640.0, 320.0])
        .with_resizable(false)
        .with_maximize_button(false);
    if let Ok(icon) = eframe::icon_data::from_png_bytes(include_bytes!(
        "../../launcher/exresources/icons/lunalauncher.png"
    )) {
        viewport = viewport.with_icon(icon);
    }
    eframe::NativeOptions {
        viewport,
        persist_window: false,
        ..Default::default()
    }
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

fn pick_folder(current: &str, title: &str) -> Option<PathBuf> {
    let mut dialog = FileDialog::new().set_title(title);
    let current = Path::new(current.trim());
    let directory = if current.is_dir() {
        Some(current)
    } else {
        current.parent().filter(|path| path.is_dir())
    };
    if let Some(directory) = directory {
        dialog = dialog.set_directory(directory);
    }
    dialog.pick_folder()
}

fn system_data_directory() -> String {
    directories::BaseDirs::new()
        .map(|dirs| dirs.config_dir().join("LunaLauncher").display().to_string())
        .unwrap_or_else(|| "LunaLauncher data".to_owned())
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum InstallerScreen {
    WizardUi,
    WizardMethod,
    WizardLocation,
    WizardData,
    WizardShortcuts,
    WizardReview,
    Extracting,
    ArchiveViewer,
    LaunchChoice,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum InstallMethod {
    Mmc,
    Vanilla,
    Custom,
    ExtractOnly,
}

struct InstallerApp {
    source: PayloadSource,
    entries: Vec<String>,
    entries_error: Option<String>,
    screen: InstallerScreen,
    install_method: InstallMethod,
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
    data_directory: String,
    register_instance_store: bool,
    instance_store_path: String,
    migration: Option<luna_installer_shell::migration::Plan>,
    migration_source: String,
    migration_confirmed: bool,
    instance_store_format: config::InstanceStoreFormat,
    install_receiver: Option<mpsc::Receiver<InstallMessage>>,
    installing: bool,
    extracting_only: bool,
    install_progress: f32,
    install_progress_text: String,
    install_log: String,
    language: i18n::Language,
    launch_monitor: Option<platform::LaunchMonitor>,
    classic_preview: egui::TextureHandle,
    neo_preview: egui::TextureHandle,
}

struct LaunchChooserApp {
    install_dir: PathBuf,
    launch_monitor: Option<platform::LaunchMonitor>,
    language: i18n::Language,
    status: String,
}

impl LaunchChooserApp {
    fn new(
        install_dir: PathBuf,
        launch_monitor: Option<platform::LaunchMonitor>,
        language: i18n::Language,
    ) -> Self {
        Self {
            install_dir,
            launch_monitor,
            language,
            status: String::new(),
        }
    }
}

impl eframe::App for LaunchChooserApp {
    fn update(&mut self, context: &egui::Context, frame: &mut eframe::Frame) {
        self.launch_monitor = frame
            .window_handle()
            .ok()
            .and_then(|handle| platform::monitor_for_window_handle(handle.as_raw()));
        egui::CentralPanel::default().show(context, |ui| {
            ui.heading(match self.language {
                i18n::Language::SimplifiedChinese => "选择启动界面",
                i18n::Language::English => "Choose launcher UI",
            });
            ui.label(match self.language {
                i18n::Language::SimplifiedChinese => "两个界面都已安装。请选择这次要打开的界面。",
                i18n::Language::English => "Both UIs are installed. Choose which one to open now.",
            });
            ui.separator();
            ui.horizontal(|ui| {
                if ui.button(i18n::text(self.language, "ui.classic")).clicked() {
                    self.launch(ui, config::EntryPoint::Classic);
                }
                if ui.button(i18n::text(self.language, "ui.neo")).clicked() {
                    self.launch(ui, config::EntryPoint::Neo);
                }
            });
            if !self.status.is_empty() {
                ui.label(&self.status);
            }
        });
    }
}

impl LaunchChooserApp {
    fn launch(&mut self, ui: &egui::Ui, entry_point: config::EntryPoint) {
        match platform::launch_with_monitor(&self.install_dir, entry_point, self.launch_monitor) {
            Ok(()) => ui.ctx().send_viewport_cmd(egui::ViewportCommand::Close),
            Err(error) => self.status = format!("Launch failed: {error:#}"),
        }
    }
}

impl InstallerApp {
    fn new(source: PayloadSource, context: &egui::Context) -> Self {
        let existing = config::load().ok().flatten();
        let destination = existing
            .as_ref()
            .and_then(|config| {
                if config.instance_layout == config::InstanceLayoutMode::Vanilla {
                    vanilla_install_directory()
                } else {
                    Some(config.install_dir.clone())
                }
            })
            .map(|path| path.display().to_string())
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
            screen: InstallerScreen::WizardUi,
            install_method: match existing.as_ref().map(|config| config.instance_layout) {
                Some(config::InstanceLayoutMode::Vanilla) => InstallMethod::Vanilla,
                Some(config::InstanceLayoutMode::Custom) => InstallMethod::Custom,
                _ => InstallMethod::Mmc,
            },
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
            data_directory: existing
                .as_ref()
                .and_then(|config| config.data_dir.as_ref())
                .map(|path| path.display().to_string())
                .unwrap_or_else(system_data_directory),
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
            migration: None,
            migration_source: String::new(),
            migration_confirmed: false,
            install_receiver: None,
            installing: false,
            extracting_only: false,
            install_progress: 0.0,
            install_progress_text: String::new(),
            install_log: String::new(),
            language: i18n::Language::detect(),
            launch_monitor: None,
            classic_preview: load_preview(
                context,
                "luna-installer-preview-classic",
                include_bytes!("../resources/preview-classicui.png"),
            ),
            neo_preview: load_preview(
                context,
                "luna-installer-preview-neo",
                include_bytes!("../resources/preview-neoui.png"),
            ),
        }
    }
}

fn load_preview(context: &egui::Context, name: &str, bytes: &[u8]) -> egui::TextureHandle {
    let image = image::load_from_memory(bytes)
        .expect("bundled installer preview must be a valid PNG")
        .into_rgba8();
    let size = [image.width() as usize, image.height() as usize];
    let color_image = egui::ColorImage::from_rgba_unmultiplied(size, image.as_raw());
    context.load_texture(name, color_image, egui::TextureOptions::LINEAR)
}

impl eframe::App for InstallerApp {
    fn update(&mut self, context: &egui::Context, frame: &mut eframe::Frame) {
        self.launch_monitor = frame
            .window_handle()
            .ok()
            .and_then(|handle| platform::monitor_for_window_handle(handle.as_raw()));
        self.poll_install(context);
        egui::TopBottomPanel::bottom("installer-footer")
            .resizable(false)
            .show(context, |ui| self.show_footer(ui));
        egui::TopBottomPanel::bottom("wizard-navigation")
            .resizable(false)
            .show(context, |ui| {
                if !self.installing {
                    self.show_wizard_navigation(ui);
                }
            });
        egui::CentralPanel::default().show(context, |ui| {
            egui::ScrollArea::vertical()
                .auto_shrink([false, false])
                .show(ui, |ui| match self.screen {
                    InstallerScreen::WizardUi
                    | InstallerScreen::WizardMethod
                    | InstallerScreen::WizardLocation
                    | InstallerScreen::WizardData
                    | InstallerScreen::WizardShortcuts
                    | InstallerScreen::WizardReview
                    | InstallerScreen::Extracting => self.show_install(ui),
                    InstallerScreen::ArchiveViewer => self.show_archive_viewer(ui),
                    InstallerScreen::LaunchChoice => self.show_launch_choice(ui),
                });
        });
    }
}

impl InstallerApp {
    fn show_footer(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            if ui
                .button(i18n::text(self.language, "footer.view"))
                .clicked()
            {
                self.screen = InstallerScreen::ArchiveViewer;
            }
            if ui
                .button(i18n::text(self.language, "footer.open_archive"))
                .clicked()
            {
                match platform::open_with_archive_manager(&self.source.path) {
                    Ok(()) => self.status = i18n::text(self.language, "footer.opened").to_owned(),
                    Err(error) => {
                        self.status = format!(
                            "{}: {error:#}",
                            i18n::text(self.language, "footer.open_failed")
                        )
                    }
                }
            }
        });
    }

    fn show_language_picker(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                egui::ComboBox::from_id_salt("installer-language")
                    .selected_text(self.language.name())
                    .show_ui(ui, |ui| {
                        ui.selectable_value(
                            &mut self.language,
                            i18n::Language::SimplifiedChinese,
                            "简体中文",
                        );
                        ui.selectable_value(&mut self.language, i18n::Language::English, "English");
                    });
                ui.label(i18n::text(self.language, "language"));
            });
        });
    }

    fn poll_install(&mut self, context: &egui::Context) {
        let Some(receiver) = self.install_receiver.take() else {
            return;
        };
        let mut keep_receiver = true;
        for message in receiver.try_iter() {
            match message {
                InstallMessage::Progress { fraction, text } => {
                    self.install_progress = fraction.clamp(0.0, 1.0);
                    self.install_progress_text = text.clone();
                    if !self.install_log.is_empty() {
                        self.install_log.push('\n');
                    }
                    self.install_log.push_str(&text);
                }
                InstallMessage::Finished(result) => {
                    self.installing = false;
                    keep_receiver = false;
                    match result {
                        Ok(count) => {
                            if self.extracting_only {
                                self.status = format!(
                                    "Extracted {count} files successfully; no installation was registered"
                                );
                                self.install_progress = 1.0;
                                self.install_progress_text = "Extraction complete".to_owned();
                            } else if self.entry_point == config::EntryPoint::Both {
                                self.screen = InstallerScreen::LaunchChoice;
                                self.status = format!(
                                    "Installed {count} files successfully; choose a launcher UI"
                                );
                            } else {
                                self.status = format!("Installed {count} files successfully");
                            }
                            self.install_progress = 1.0;
                            self.install_progress_text = "Installation complete".to_owned();
                        }
                        Err(error) => {
                            self.status = if self.extracting_only {
                                format!("Extraction failed: {error}")
                            } else {
                                format!("Install failed: {error}")
                            };
                            self.install_progress_text = if self.extracting_only {
                                "Extraction failed".to_owned()
                            } else {
                                "Installation failed".to_owned()
                            };
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

    fn show_launch_choice(&mut self, ui: &mut egui::Ui) {
        self.show_language_picker(ui);
        ui.heading(match self.language {
            i18n::Language::SimplifiedChinese => "选择启动界面",
            i18n::Language::English => "Choose launcher UI",
        });
        ui.label(match self.language {
            i18n::Language::SimplifiedChinese => "两个界面都已安装。请选择这次要打开的界面。",
            i18n::Language::English => "Both UIs are installed. Choose which one to open now.",
        });
        ui.separator();
        ui.horizontal(|ui| {
            if ui.button(i18n::text(self.language, "ui.classic")).clicked() {
                self.launch_selected_ui(ui, config::EntryPoint::Classic);
            }
            if ui.button(i18n::text(self.language, "ui.neo")).clicked() {
                self.launch_selected_ui(ui, config::EntryPoint::Neo);
            }
        });
        ui.label(&self.status);
    }

    fn launch_selected_ui(&mut self, ui: &egui::Ui, entry_point: config::EntryPoint) {
        match platform::launch_with_monitor(
            Path::new(&self.destination),
            entry_point,
            self.launch_monitor,
        ) {
            Ok(()) => ui.ctx().send_viewport_cmd(egui::ViewportCommand::Close),
            Err(error) => self.status = format!("Launch failed: {error:#}"),
        }
    }

    fn show_install(&mut self, ui: &mut egui::Ui) {
        self.show_language_picker(ui);
        ui.heading(i18n::text(self.language, "install.title"));
        ui.label(i18n::text(self.language, "install.subtitle"));
        self.show_step_indicator(ui);
        ui.separator();
        if self.installing {
            self.show_progress_panel(ui);
            return;
        }
        match self.screen {
            InstallerScreen::WizardUi => self.show_wizard_ui(ui),
            InstallerScreen::WizardMethod => self.show_wizard_method(ui),
            InstallerScreen::WizardLocation => self.show_wizard_location(ui),
            InstallerScreen::WizardData => self.show_wizard_data(ui),
            InstallerScreen::WizardShortcuts => self.show_wizard_shortcuts(ui),
            InstallerScreen::WizardReview => self.show_wizard_review(ui),
            InstallerScreen::Extracting => self.show_extracting(ui),
            _ => {}
        }
    }

    fn show_extracting(&mut self, ui: &mut egui::Ui) {
        ui.heading(i18n::text(self.language, "extract.title"));
        ui.label(i18n::text(self.language, "extract.help"));
        self.show_progress_panel(ui);
        if !self.installing {
            ui.separator();
            ui.label(&self.status);
        }
    }

    fn show_progress_panel(&mut self, ui: &mut egui::Ui) {
        ui.label(if self.extracting_only {
            i18n::text(self.language, "progress.extracting")
        } else {
            i18n::text(self.language, "progress.installing")
        });
        ui.add(egui::ProgressBar::new(self.install_progress).show_percentage());
        ui.label(&self.install_progress_text);
        ui.label(i18n::text(self.language, "progress.details"));
        egui::ScrollArea::vertical()
            .max_height(180.0)
            .stick_to_bottom(true)
            .show(ui, |ui| {
                ui.add(
                    egui::TextEdit::multiline(&mut self.install_log)
                        .desired_width(f32::INFINITY)
                        .desired_rows(8)
                        .interactive(false),
                );
            });
    }

    fn show_step_indicator(&self, ui: &mut egui::Ui) {
        let steps = if self.install_method == InstallMethod::ExtractOnly {
            vec![
                (
                    InstallerScreen::WizardUi,
                    i18n::text(self.language, "wizard.ui"),
                ),
                (
                    InstallerScreen::WizardMethod,
                    i18n::text(self.language, "wizard.method"),
                ),
                (
                    InstallerScreen::WizardLocation,
                    i18n::text(self.language, "wizard.location"),
                ),
                (
                    InstallerScreen::Extracting,
                    i18n::text(self.language, "wizard.extract"),
                ),
            ]
        } else {
            vec![
                (
                    InstallerScreen::WizardUi,
                    i18n::text(self.language, "wizard.ui"),
                ),
                (
                    InstallerScreen::WizardMethod,
                    i18n::text(self.language, "wizard.method"),
                ),
                (
                    InstallerScreen::WizardLocation,
                    i18n::text(self.language, "wizard.location"),
                ),
                (
                    InstallerScreen::WizardData,
                    i18n::text(self.language, "wizard.data"),
                ),
                (
                    InstallerScreen::WizardShortcuts,
                    i18n::text(self.language, "wizard.shortcuts"),
                ),
                (
                    InstallerScreen::WizardReview,
                    i18n::text(self.language, "wizard.review"),
                ),
            ]
        };
        let step_count = steps.len();
        ui.horizontal_wrapped(|ui| {
            for (index, (screen, label)) in steps.into_iter().enumerate() {
                let selected = self.screen == screen;
                if ui
                    .add(egui::Button::new(format!("{}  {}", index + 1, label)).selected(selected))
                    .clicked()
                {
                    // The indicator is intentionally read-only. Navigation is
                    // kept on the Back/Next buttons so a partially configured
                    // page cannot be skipped accidentally.
                }
                if index + 1 < step_count {
                    ui.label("›");
                }
            }
        });
    }

    fn show_wizard_navigation(&mut self, ui: &mut egui::Ui) {
        let (back, next) = match self.screen {
            InstallerScreen::WizardUi => (None, Some(InstallerScreen::WizardMethod)),
            InstallerScreen::WizardMethod => (
                Some(InstallerScreen::WizardUi),
                Some(InstallerScreen::WizardLocation),
            ),
            InstallerScreen::WizardLocation => (
                Some(InstallerScreen::WizardMethod),
                if self.install_method == InstallMethod::ExtractOnly {
                    Some(InstallerScreen::Extracting)
                } else {
                    Some(InstallerScreen::WizardData)
                },
            ),
            InstallerScreen::WizardData => (
                Some(InstallerScreen::WizardLocation),
                Some(InstallerScreen::WizardShortcuts),
            ),
            InstallerScreen::WizardShortcuts => (
                Some(InstallerScreen::WizardData),
                Some(InstallerScreen::WizardReview),
            ),
            InstallerScreen::WizardReview => (Some(InstallerScreen::WizardShortcuts), None),
            InstallerScreen::Extracting
            | InstallerScreen::ArchiveViewer
            | InstallerScreen::LaunchChoice => (None, None),
        };
        if back.is_none() && next.is_none() {
            return;
        }
        ui.separator();
        ui.horizontal(|ui| {
            if let Some(back) = back
                && ui
                    .button(i18n::text(self.language, "wizard.back"))
                    .clicked()
            {
                self.screen = back;
            }
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                let next_label = if next == Some(InstallerScreen::Extracting) {
                    i18n::text(self.language, "extract.button")
                } else {
                    i18n::text(self.language, "wizard.next")
                };
                if let Some(next) = next
                    && ui.add_enabled(
                        self.screen != InstallerScreen::WizardData || self.data_directory_error().is_none(),
                        egui::Button::new(next_label),
                    ).clicked()
                {
                    if next == InstallerScreen::Extracting {
                        self.start_extract_only(PathBuf::from(self.destination.trim()));
                    } else {
                        self.screen = next;
                    }
                }
            });
        });
    }

    fn show_wizard_ui(&mut self, ui: &mut egui::Ui) {
        ui.heading(i18n::text(self.language, "wizard.ui_title"));
        ui.label(i18n::text(self.language, "wizard.ui_help"));
        ui.horizontal_wrapped(|ui| {
            ui.radio_value(
                &mut self.entry_point,
                config::EntryPoint::Classic,
                i18n::text(self.language, "ui.classic"),
            );
            ui.radio_value(
                &mut self.entry_point,
                config::EntryPoint::Neo,
                i18n::text(self.language, "ui.neo"),
            );
            ui.radio_value(
                &mut self.entry_point,
                config::EntryPoint::Both,
                i18n::text(self.language, "ui.both"),
            );
        });
        self.show_ui_preview(ui);
    }

    fn show_wizard_method(&mut self, ui: &mut egui::Ui) {
        ui.heading(i18n::text(self.language, "wizard.method_title"));
        ui.label(i18n::text(self.language, "wizard.method_help"));
        let previous = self.install_method;
        ui.vertical(|ui| {
            ui.radio_value(
                &mut self.install_method,
                InstallMethod::Mmc,
                i18n::text(self.language, "mode.mmc"),
            );
            ui.radio_value(
                &mut self.install_method,
                InstallMethod::Vanilla,
                i18n::text(self.language, "mode.vanilla"),
            );
            ui.radio_value(
                &mut self.install_method,
                InstallMethod::Custom,
                i18n::text(self.language, "mode.custom"),
            );
            ui.radio_value(
                &mut self.install_method,
                InstallMethod::ExtractOnly,
                i18n::text(self.language, "mode.extract"),
            );
        });
        if previous != self.install_method {
            self.instance_layout = match self.install_method {
                InstallMethod::Mmc => config::InstanceLayoutMode::Mmc,
                InstallMethod::Vanilla => config::InstanceLayoutMode::Vanilla,
                InstallMethod::Custom => config::InstanceLayoutMode::Custom,
                InstallMethod::ExtractOnly => config::InstanceLayoutMode::Mmc,
            };
            self.apply_instance_layout_defaults();
            if self.install_method == InstallMethod::ExtractOnly {
                self.destination = format!("{}-extracted", default_install_dir());
                self.extract_destination = self.destination.clone();
                self.portable_mode = false;
                self.register_instance_store = false;
            }
        }
        let help = match self.install_method {
            InstallMethod::Mmc => "mode.mmc.help",
            InstallMethod::Vanilla => "mode.vanilla.help",
            InstallMethod::Custom => "mode.custom.help",
            InstallMethod::ExtractOnly => "mode.extract.help",
        };
        ui.label(i18n::text(self.language, help));
    }

    fn show_wizard_location(&mut self, ui: &mut egui::Ui) {
        ui.heading(i18n::text(self.language, "wizard.location_title"));
        ui.label(i18n::text(self.language, "wizard.location_help"));
        if self.install_method == InstallMethod::ExtractOnly {
            ui.label(i18n::text(self.language, "extract.options"));
        }
        ui.horizontal(|ui| {
            ui.label(i18n::text(self.language, "install.directory"));
            if self.instance_layout == config::InstanceLayoutMode::Vanilla
                && self.install_method != InstallMethod::ExtractOnly
            {
                ui.label(&self.destination);
            } else {
                ui.text_edit_singleline(&mut self.destination);
                if ui.button(i18n::text(self.language, "browse")).clicked()
                    && let Some(path) = pick_folder(
                        &self.destination,
                        i18n::text(self.language, "install.directory"),
                    )
                {
                    self.destination = path.display().to_string();
                    self.extract_destination = format!("{}-extracted", self.destination);
                }
            }
        });
        if self.instance_layout == config::InstanceLayoutMode::Vanilla {
            ui.small(i18n::text(self.language, "install.shell_dir"));
        }
        if self.install_method == InstallMethod::ExtractOnly {
            ui.checkbox(
                &mut self.portable_mode,
                i18n::text(self.language, "portable"),
            );
            ui.checkbox(
                &mut self.desktop_shortcuts,
                i18n::text(self.language, "shortcuts.desktop"),
            );
            ui.checkbox(
                &mut self.start_menu_shortcuts,
                i18n::text(self.language, "shortcuts.start"),
            );
        }
    }

    fn show_wizard_data(&mut self, ui: &mut egui::Ui) {
        ui.heading(i18n::text(self.language, "wizard.data_title"));
        ui.label(i18n::text(self.language, "data.help"));
        match self.install_method {
            InstallMethod::Mmc => {
                ui.label(i18n::text(self.language, "mode.mmc.help"));
                self.show_data_directory_editor(ui);
                ui.checkbox(
                    &mut self.register_instance_store,
                    i18n::text(self.language, "data.extra_store"),
                );
                if self.register_instance_store {
                    ui.horizontal(|ui| {
                        ui.label(i18n::text(self.language, "custom.instance"));
                        ui.text_edit_singleline(&mut self.instance_store_path);
                        if ui.button(i18n::text(self.language, "browse")).clicked()
                            && let Some(path) = pick_folder(
                                &self.instance_store_path,
                                i18n::text(self.language, "custom.instance"),
                            )
                        {
                            self.instance_store_path = path.display().to_string();
                        }
                    });
                    ui.horizontal_wrapped(|ui| {
                        ui.label(i18n::text(self.language, "custom.format"));
                        ui.radio_value(
                            &mut self.instance_store_format,
                            config::InstanceStoreFormat::Prism,
                            i18n::text(self.language, "mode.mmc"),
                        );
                        ui.radio_value(
                            &mut self.instance_store_format,
                            config::InstanceStoreFormat::VanillaCompatible,
                            i18n::text(self.language, "mode.vanilla"),
                        );
                    });
                }
            }
            InstallMethod::Vanilla => {
                ui.label(i18n::text(self.language, "mode.vanilla.help"));
                ui.label("已有游戏目录 / Existing game directory");
                ui.text_edit_singleline(&mut self.instance_store_path);
                if ui.button("浏览游戏目录 / Browse game folder").clicked() {
                    if let Some(path) = FileDialog::new().pick_folder() {self.instance_store_path = path.display().to_string();}
                }
                for path in luna_installer_shell::migration::candidates(Path::new(&self.destination).parent().unwrap_or(Path::new(&self.instance_store_path))) {
                    if ui.button(format!("使用 / Use {}", path.display())).clicked() {self.instance_store_path = path.display().to_string();}
                }
                ui.horizontal(|ui| {
                    ui.label(i18n::text(self.language, "data.portable_path"));
                    ui.monospace(
                        Path::new(&self.destination)
                            .join("UserData")
                            .display()
                            .to_string(),
                    );
                });
            }
            InstallMethod::Custom => {
                ui.horizontal(|ui| {
                    ui.label(i18n::text(self.language, "custom.instance"));
                    ui.text_edit_singleline(&mut self.instance_store_path);
                    if ui.button(i18n::text(self.language, "browse")).clicked()
                        && let Some(path) = pick_folder(
                            &self.instance_store_path,
                            i18n::text(self.language, "custom.instance"),
                        )
                    {
                        self.instance_store_path = path.display().to_string();
                    }
                });
                ui.horizontal_wrapped(|ui| {
                    ui.label(i18n::text(self.language, "custom.format"));
                    ui.radio_value(
                        &mut self.instance_store_format,
                        config::InstanceStoreFormat::Prism,
                        i18n::text(self.language, "mode.mmc"),
                    );
                    ui.radio_value(
                        &mut self.instance_store_format,
                        config::InstanceStoreFormat::VanillaCompatible,
                        i18n::text(self.language, "mode.vanilla"),
                    );
                });
                ui.checkbox(
                    &mut self.register_instance_store,
                    i18n::text(self.language, "custom.register"),
                );
                ui.checkbox(
                    &mut self.portable_mode,
                    i18n::text(self.language, "portable"),
                );
                if !self.portable_mode {
                    self.show_data_directory_editor(ui);
                } else {
                    ui.label(format!("{} {}", i18n::text(self.language, "data.portable_path"), self.effective_data_directory().display()));
                }
            }
            InstallMethod::ExtractOnly => {
                ui.label(i18n::text(self.language, "mode.extract.help"));
            }
        }
    }

    fn show_wizard_shortcuts(&mut self, ui: &mut egui::Ui) {
        ui.heading(i18n::text(self.language, "wizard.shortcuts_title"));
        ui.checkbox(
            &mut self.desktop_shortcuts,
            i18n::text(self.language, "shortcuts.desktop"),
        );
        ui.checkbox(
            &mut self.start_menu_shortcuts,
            i18n::text(self.language, "shortcuts.start"),
        );
        ui.label(i18n::text(self.language, "wizard.shortcuts_help"));
    }

    fn effective_data_directory(&self) -> PathBuf {
        if self.instance_layout == config::InstanceLayoutMode::Vanilla || self.portable_mode {
            Path::new(self.destination.trim()).join("UserData")
        } else {
            PathBuf::from(self.data_directory.trim())
        }
    }

    fn custom_data_directory(&self) -> Option<PathBuf> {
        if self.instance_layout == config::InstanceLayoutMode::Vanilla
            || self.portable_mode
            || self.data_directory.trim() == system_data_directory()
        {
            None
        } else {
            Some(PathBuf::from(self.data_directory.trim()))
        }
    }

    fn data_directory_error(&self) -> Option<String> {
        self.custom_data_directory().and_then(|path| {
            platform::validate_data_directory(&path, Path::new(self.destination.trim()))
                .err().map(|error| error.to_string())
        })
    }

    fn show_data_directory_editor(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            ui.label(i18n::text(self.language, "data.path"));
            ui.add(egui::TextEdit::singleline(&mut self.data_directory)
                .desired_width((ui.available_width() - 180.0).max(100.0)));
            if ui.button(i18n::text(self.language, "browse")).clicked()
                && let Some(path) = pick_folder(&self.data_directory, i18n::text(self.language, "data.path"))
            {
                self.data_directory = path.display().to_string();
            }
            if ui.button(i18n::text(self.language, "data.reset")).clicked() {
                self.data_directory = system_data_directory();
            }
        });
        ui.small(i18n::text(self.language, "data.path_help"));
        if let Some(error) = self.data_directory_error() {
            ui.colored_label(egui::Color32::RED, error);
        }
    }

    fn show_wizard_review(&mut self, ui: &mut egui::Ui) {
        ui.heading(i18n::text(self.language, "wizard.review_title"));
        ui.label(format!(
            "{} {}",
            i18n::text(self.language, "install.ui"),
            self.entry_point_label()
        ));
        ui.label(format!(
            "{} {}",
            i18n::text(self.language, "install.directory"),
            self.destination
        ));
        ui.label(format!(
            "{} {}",
            i18n::text(self.language, "data.title"),
            self.install_method_label()
        ));
        ui.label(format!("{} {}", i18n::text(self.language, "data.path"), self.effective_data_directory().display()));
        let data_error = self.data_directory_error();
        if let Some(error) = &data_error {
            ui.colored_label(egui::Color32::RED, error);
        }
        ui.label(format!(
            "{} {}, {}",
            i18n::text(self.language, "wizard.shortcut_summary"),
            self.desktop_shortcuts,
            self.start_menu_shortcuts
        ));
        let migrating = self.instance_layout == config::InstanceLayoutMode::Vanilla;
        if migrating {
            if self.migration_source != self.instance_store_path || self.migration.is_none() {
                self.migration = Some(luna_installer_shell::migration::scan_from(Path::new(self.instance_store_path.trim()), Path::new(&self.destination).parent().unwrap_or(Path::new(&self.destination))));
                self.migration_source = self.instance_store_path.clone();
                self.migration_confirmed = false;
            }
            ui.separator();
            ui.label("迁移预览 / Migration preview — 新实例默认隔离 / New instances default to isolation");
            if let Some(plan) = &mut self.migration {
                ui.monospace(plan.root.display().to_string());
                ui.label("仅保存确认后的 Luna 目录设置，不移动游戏文件，不删除原启动器。 / Save Luna directory choices only; keep existing files and launchers.");
                egui::ScrollArea::vertical().max_height(180.0).show(ui, |ui| {
                    for p in &mut plan.profiles {
                        ui.label(format!("{} · {}", p.name, p.evidence));
                        let old = p.mode.clone();
                        egui::ComboBox::from_id_salt(&p.path).selected_text(&p.mode).show_ui(ui, |ui| {
                            ui.selectable_value(&mut p.mode, "isolated".into(), "隔离 / Isolated");
                            ui.selectable_value(&mut p.mode, "shared".into(), "非隔离 / Shared");

                        });
                        if old != p.mode {self.migration_confirmed = false;}
                        let game = if p.mode == "shared" {&plan.root} else {&p.path};
                        if p.mode != "auto" {ui.monospace(game.display().to_string());}
                    }
                    for warning in &plan.warnings {ui.colored_label(egui::Color32::YELLOW, warning);}
                });
                if plan.profiles.is_empty() {ui.label("未发现已有版本；新安装使用隔离模式。 / No existing profiles; new installs use isolation.");}
            }
            ui.checkbox(&mut self.migration_confirmed, "我已核对目录及隔离设置 / I confirm these directories and modes");
            if ui.button("重新探测 / Scan again").clicked() {self.migration = None; self.migration_confirmed = false;}
        }
        if !self.status.is_empty() {
            ui.label(&self.status);
        }
        if ui
            .add_enabled(data_error.is_none() && (!migrating || (self.migration_confirmed && self.migration.as_ref().is_some_and(|p| p.profiles.iter().all(|i| i.mode != "auto")))), egui::Button::new(i18n::text(self.language, "install.button")))
            .clicked()
        {
            let destination = PathBuf::from(self.destination.trim());
            let setup = self.launcher_setup(&destination);
            self.start_install(destination, setup);
        }
    }

    fn entry_point_label(&self) -> &'static str {
        match self.entry_point {
            config::EntryPoint::Classic => i18n::text(self.language, "ui.classic"),
            config::EntryPoint::Neo => i18n::text(self.language, "ui.neo"),
            config::EntryPoint::Both => i18n::text(self.language, "ui.both"),
        }
    }

    fn install_method_label(&self) -> &'static str {
        match self.install_method {
            InstallMethod::Mmc => i18n::text(self.language, "mode.mmc"),
            InstallMethod::Vanilla => i18n::text(self.language, "mode.vanilla"),
            InstallMethod::Custom => i18n::text(self.language, "mode.custom"),
            InstallMethod::ExtractOnly => i18n::text(self.language, "mode.extract"),
        }
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
        let launch_monitor = self.launch_monitor;
        let launch_after = entry_point != config::EntryPoint::Both;
        self.install_receiver = Some(receiver);
        self.installing = true;
        self.extracting_only = false;
        self.install_progress = 0.0;
        self.install_progress_text = "Preparing installation...".to_owned();
        self.install_log.clear();
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
                    launch_after,
                    launch_monitor,
                    &mut report,
                )
                .map_err(|error| format!("{error:#}"))
            };
            let _ = sender.send(InstallMessage::Finished(result));
        });
    }

    fn start_extract_only(&mut self, destination: PathBuf) {
        if self.installing {
            return;
        }
        let (sender, receiver) = mpsc::channel();
        let source = self.source.clone();
        let entry_point = self.entry_point;
        let desktop_shortcuts = self.desktop_shortcuts;
        let start_menu_shortcuts = self.start_menu_shortcuts;
        let portable_mode = self.portable_mode;
        self.screen = InstallerScreen::Extracting;
        self.install_receiver = Some(receiver);
        self.installing = true;
        self.extracting_only = true;
        self.install_progress = 0.0;
        self.install_progress_text = "Preparing extraction...".to_owned();
        self.install_log.clear();
        self.status = "Extraction in progress".to_owned();
        std::thread::spawn(move || {
            let result = {
                let mut report = |fraction: f32, text: &str| {
                    let _ = sender.send(InstallMessage::Progress {
                        fraction,
                        text: text.to_owned(),
                    });
                };
                extract_only_with_progress(
                    &source,
                    &destination,
                    entry_point,
                    desktop_shortcuts,
                    start_menu_shortcuts,
                    portable_mode,
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
                // MMC/Prism mode only selects how launcher data is managed. It
                // must not make the launcher binaries inherit the shell's
                // directory (or a Prism instance directory). Keep the
                // install tree at the platform installer location, just as
                // the original NSIS package did.
                self.destination = default_install_dir();
                self.extract_destination = format!("{}-extracted", self.destination);
            }
            config::InstanceLayoutMode::Vanilla => {
                self.portable_mode = true;
                self.register_instance_store = true;
                self.instance_store_format = config::InstanceStoreFormat::VanillaCompatible;
                if let Some(install_directory) = vanilla_install_directory() {
                    self.destination = install_directory.display().to_string();
                    self.extract_destination = format!("{}-extracted", self.destination);
                }
                self.instance_store_path = Path::new(&self.destination)
                    .parent()
                    .map(|path| path.display().to_string())
                    .unwrap_or_else(|| self.destination.clone());
                let detected: Vec<_> = luna_installer_shell::migration::candidates(Path::new(&self.instance_store_path))
                    .into_iter().filter(|p| p.join("versions").is_dir()).collect();
                if detected.len() == 1 {
                    self.instance_store_path = detected[0].display().to_string();
                }
                self.migration = None;
                self.migration_confirmed = false;
            }
            config::InstanceLayoutMode::Custom => {}
        }
    }

    fn launcher_setup(&self, _destination: &Path) -> LauncherSetup {
        match self.instance_layout {
            config::InstanceLayoutMode::Mmc => LauncherSetup {
                migration: None,
                instance_layout: config::InstanceLayoutMode::Mmc,
                portable_mode: false,
                data_dir: self.custom_data_directory(),
                register_instance_store: self.register_instance_store,
                instance_store_path: if self.register_instance_store {
                    Some(PathBuf::from(self.instance_store_path.trim()))
                } else {
                    None
                },
                instance_store_format: self.instance_store_format,
            },
            config::InstanceLayoutMode::Vanilla => LauncherSetup {
                migration: self.migration.clone(),
                instance_layout: config::InstanceLayoutMode::Vanilla,
                portable_mode: true,
                data_dir: None,
                register_instance_store: true,
                instance_store_path: Some(
                    PathBuf::from(self.instance_store_path.trim()),
                ),
                instance_store_format: config::InstanceStoreFormat::VanillaCompatible,
            },
            config::InstanceLayoutMode::Custom => LauncherSetup {
                migration: None,
                instance_layout: config::InstanceLayoutMode::Custom,
                portable_mode: self.portable_mode,
                data_dir: self.custom_data_directory(),
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
        self.show_language_picker(ui);
        ui.heading(i18n::text(self.language, "archive.title"));
        if let Some(error) = &self.entries_error {
            ui.colored_label(egui::Color32::RED, error);
        }
        ui.label(format!("{} entries", self.entries.len()));
        ui.horizontal(|ui| {
            ui.label(i18n::text(self.language, "archive.filter"));
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
        ui.label(i18n::text(self.language, "archive.extract"));
        ui.horizontal(|ui| {
            ui.label(i18n::text(self.language, "archive.destination"));
            ui.text_edit_singleline(&mut self.extract_destination);
            if ui.button(i18n::text(self.language, "browse")).clicked()
                && let Some(path) = pick_folder(
                    &self.extract_destination,
                    i18n::text(self.language, "archive.destination"),
                )
            {
                self.extract_destination = path.display().to_string();
            }
            if ui
                .button(i18n::text(self.language, "archive.extract_all"))
                .clicked()
            {
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
            if ui
                .button(i18n::text(self.language, "archive.install"))
                .clicked()
            {
                self.screen = InstallerScreen::WizardUi;
            }
            if ui
                .button(i18n::text(self.language, "archive.back"))
                .clicked()
            {
                self.screen = InstallerScreen::WizardUi;
            }
        });
    }

    fn show_ui_preview(&mut self, ui: &mut egui::Ui) {
        ui.separator();
        ui.label(i18n::text(self.language, "ui.preview"));
        let available_width = ui.available_width().max(320.0);
        match self.entry_point {
            config::EntryPoint::Classic => {
                show_preview_image(ui, &self.classic_preview, available_width.min(860.0));
            }
            config::EntryPoint::Neo => {
                show_preview_image(ui, &self.neo_preview, available_width.min(860.0));
            }
            config::EntryPoint::Both => {
                ui.horizontal_top(|ui| {
                    let width = ((ui.available_width() - 12.0) / 2.0).max(220.0);
                    show_preview_image(ui, &self.classic_preview, width);
                    show_preview_image(ui, &self.neo_preview, width);
                });
            }
        }
    }
}

fn show_preview_image(ui: &mut egui::Ui, texture: &egui::TextureHandle, width: f32) {
    let source_size = texture.size_vec2();
    let size = egui::vec2(width, width * source_size.y / source_size.x.max(1.0));
    ui.add(egui::Image::from_texture(texture).fit_to_exact_size(size));
}
