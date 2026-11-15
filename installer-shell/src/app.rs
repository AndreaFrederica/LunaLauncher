//! LunaLauncher application adapter.
//!
//! The reusable installation engine lives in [`luna_installer_shell::engine`].
//! This module supplies Luna-specific entry points, instance-layout settings,
//! shortcuts, registry integration, and persisted installer configuration.

use std::path::{Path, PathBuf};

use anyhow::{Context, Result};

use luna_installer_shell::{
    archive, config,
    engine::{self, InstallBackend, InstallRequest},
    platform,
    sfx::PayloadSource,
};

fn current_executable_directory() -> Option<PathBuf> {
    std::env::current_exe()
        .ok()
        .and_then(|path| path.parent().map(Path::to_path_buf))
}

#[derive(Debug, Clone)]
pub(crate) struct LauncherSetup {
    pub(crate) migration: Option<luna_installer_shell::migration::Plan>,
    pub(crate) instance_layout: config::InstanceLayoutMode,
    pub(crate) portable_mode: bool,
    pub(crate) register_instance_store: bool,
    pub(crate) instance_store_path: Option<PathBuf>,
    pub(crate) instance_store_format: config::InstanceStoreFormat,
}

#[derive(Debug)]
pub(crate) enum InstallMessage {
    Progress { fraction: f32, text: String },
    Finished(Result<usize, String>),
}

struct LunaInstallBackend {
    entry_point: config::EntryPoint,
    desktop_shortcuts: bool,
    start_menu_shortcuts: bool,
    setup: LauncherSetup,
    launch_after: bool,
    launch_monitor: Option<platform::LaunchMonitor>,
}

impl InstallBackend for LunaInstallBackend {
    fn before_install(&self, _destination: &Path) -> Result<()> {
        if let Some(plan) = &self.setup.migration { plan.validate()?; }
        platform::stop_running_launchers()
    }

    fn after_install(&self, destination: &Path, installed_entries: &[String]) -> Result<()> {
        install_uninstaller_copy(destination)?;
        platform::create_shortcuts(
            destination,
            self.entry_point,
            self.desktop_shortcuts,
            self.start_menu_shortcuts,
        )?;
        platform::create_uninstall_shortcut(
            destination,
            self.desktop_shortcuts,
            self.start_menu_shortcuts,
        )?;
        if let Some(plan) = &self.setup.migration { plan.apply()?; }
        platform::configure_launcher(
            destination,
            self.setup.portable_mode,
            self.setup.register_instance_store,
            self.setup.instance_store_path.as_deref(),
            self.setup.instance_store_format,
        )?;
        platform::configure_windows_installation(destination, self.setup.portable_mode)?;
        if self.launch_after && self.entry_point != config::EntryPoint::Both {
            platform::launch_with_monitor(destination, self.entry_point, self.launch_monitor)?;
        }
        config::save(&config::InstallConfig {
            install_dir: destination.to_path_buf(),
            installed_entries: installed_entries.to_vec(),
            entry_point: self.entry_point,
            desktop_shortcuts: self.desktop_shortcuts,
            start_menu_shortcuts: self.start_menu_shortcuts,
            keep_user_data: true,
            instance_layout: self.setup.instance_layout,
            portable_mode: self.setup.portable_mode,
            register_instance_store: self.setup.register_instance_store,
            instance_store_path: self.setup.instance_store_path.clone(),
            instance_store_format: self.setup.instance_store_format,
        })?;
        Ok(())
    }
}

struct LunaExtractBackend {
    entry_point: config::EntryPoint,
    desktop_shortcuts: bool,
    start_menu_shortcuts: bool,
    portable_mode: bool,
}

impl InstallBackend for LunaExtractBackend {
    fn before_install(&self, _destination: &Path) -> Result<()> {
        Ok(())
    }

    fn after_install(&self, destination: &Path, _installed_entries: &[String]) -> Result<()> {
        if self.portable_mode {
            platform::configure_launcher(
                destination,
                true,
                false,
                None,
                config::InstanceStoreFormat::Prism,
            )?;
        }
        platform::create_shortcuts(
            destination,
            self.entry_point,
            self.desktop_shortcuts,
            self.start_menu_shortcuts,
        )?;
        Ok(())
    }
}

/// Extract a Luna package without saving installer state or creating an
/// uninstaller registration. Portable markers and explicitly requested
/// shortcuts are still applied to the extracted launcher tree.
pub(crate) fn extract_only_with_progress(
    source: &PayloadSource,
    destination: &Path,
    entry_point: config::EntryPoint,
    desktop_shortcuts: bool,
    start_menu_shortcuts: bool,
    portable_mode: bool,
    progress: &mut dyn FnMut(f32, &str),
) -> Result<usize> {
    let backend = LunaExtractBackend {
        entry_point,
        desktop_shortcuts,
        start_menu_shortcuts,
        portable_mode,
    };
    engine::install(
        InstallRequest {
            source,
            destination,
            in_place: false,
            preserve_user_data: true,
        },
        &backend,
        progress,
    )
}

pub(crate) fn install(
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
        None,
        &mut no_progress,
    )
}

pub(crate) fn install_with_progress(
    source: &PayloadSource,
    destination: &Path,
    entry_point: config::EntryPoint,
    desktop_shortcuts: bool,
    start_menu_shortcuts: bool,
    setup: LauncherSetup,
    launch_after: bool,
    launch_monitor: Option<platform::LaunchMonitor>,
    progress: &mut dyn FnMut(f32, &str),
) -> Result<usize> {
    let installed_entries = archive::list(source)?;
    for (_, target) in platform::entry_targets(destination, entry_point) {
        let expected_name = target
            .file_name()
            .context("launcher entry point has no file name")?
            .to_string_lossy();
        if !installed_entries
            .iter()
            .any(|entry| entry == expected_name.as_ref())
        {
            anyhow::bail!("package is missing the selected launcher entry point: {expected_name}");
        }
    }
    let backend = LunaInstallBackend {
        entry_point,
        desktop_shortcuts,
        start_menu_shortcuts,
        setup,
        launch_after,
        launch_monitor,
    };
    engine::install(
        InstallRequest {
            source,
            destination,
            in_place: current_executable_directory().is_some_and(|directory| {
                directory.canonicalize().ok() == destination.canonicalize().ok()
            }),
            preserve_user_data: true,
        },
        &backend,
        progress,
    )
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

pub(crate) fn uninstall(install_dir: &Path, purge: bool) -> Result<()> {
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
    platform::remove_windows_installation(&install_dir)?;
    config::reset()?;
    platform::schedule_remove_installation(&install_dir, !purge)?;
    println!("Uninstall scheduled for {}", install_dir.display());
    Ok(())
}
