use std::path::{Path, PathBuf};

use anyhow::{Context, Result};
use clap::Parser;
use luna_installer_shell::{archive, config, platform, sfx::PayloadSource};
use luna_installer_shell::version::LauncherVersion;

mod app;
mod ui;
#[cfg(windows)]
mod upgrade_prompt;

use app::{LauncherSetup, install, uninstall};

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
    /// Update only the specified existing installation, without running setup/migration.
    #[arg(long, requires = "install_dir", conflicts_with_all = ["install", "extract", "list", "uninstall"])]
    update: bool,
    /// Run the update worker without a progress window (automation/testing).
    #[arg(long, requires = "update")]
    update_headless: bool,
    /// Require the locally confirmed version to still be newer at install time.
    #[arg(long, requires = "update", hide = true)]
    upgrade_to: Option<String>,
    /// Existing launcher data directory (for update logs and restart).
    #[arg(long, requires = "update")]
    data_root: Option<PathBuf>,
    /// Restart the UI that requested the update.
    #[arg(long, requires = "update", value_parser = ["classic", "neo"])]
    restart_entry: Option<String>,
    /// Override the installation directory or select an existing copy to update.
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
    if args.update {
        let result = (|| {
            let source = payload_source(args.payload)?;
            let upgrade_to = args.upgrade_to.as_deref().map(|text| {
                LauncherVersion::parse(text).context("invalid confirmed upgrade version")
            }).transpose()?;
            let entry = args.restart_entry.as_deref().map(|value| if value == "neo" {
                config::EntryPoint::Neo
            } else { config::EntryPoint::Classic });
            if !args.update_headless {
                return ui::run_update(source, args.install_dir.clone().unwrap(), args.data_root.clone(), entry, !args.no_launch, upgrade_to);
            }
            luna_installer_shell::update::apply(
                &source, args.install_dir.as_deref().unwrap(), args.data_root.as_deref(),
                entry, !args.no_launch, upgrade_to,
            )
        })();
        if let Err(error) = &result {
            if let Some(data) = args.data_root.as_deref() {
                use std::io::Write;
                let _ = std::fs::write(data.join(".prism_launcher_update.fail"), format!("{error:#}"));
                if let Ok(mut log) = std::fs::OpenOptions::new().create(true).append(true).open(data.join("logs/prism_launcher_update.log")) {
                    let _ = writeln!(log, "Update failed: {error:#}");
                }
            }
            if !args.update_headless { platform::show_update_error(&format!("{error:#}")); }
        }
        return result;
    }
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
        && args.install_dir.is_none()
        && !args.no_launch
        && let Some(saved) = shell_install_config()?
            .filter(|saved| current_executable_is_shell_for_install_dir(&saved.install_dir))
    {
        #[cfg(windows)]
        {
            let source = PayloadSource::from_path(std::env::current_exe()?)?;
            if source.footer.is_some()
                && upgrade_prompt::offer(&source, &saved.install_dir, true, true)?
            {
                return Ok(());
            }
        }
        let monitor = platform::current_cursor_monitor();
        if saved.entry_point == config::EntryPoint::Both {
            ui::run_launch_chooser(saved.install_dir, monitor, payload_source(None)?)?;
        } else {
            platform::launch_with_monitor(&saved.install_dir, saved.entry_point, monitor)?;
        }
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
            adjacent_setup: None,
            entry_point: config::EntryPoint::Classic,
            desktop_shortcuts: false,
            start_menu_shortcuts: false,
            keep_user_data: true,
            instance_layout: config::InstanceLayoutMode::Mmc,
            portable_mode: false,
            data_dir: None,
            register_instance_store: false,
            instance_store_path: None,
            instance_store_format: config::InstanceStoreFormat::Prism,
        });
        if let Some(install_dir) = args.install_dir {
            existing.install_dir = install_dir;
        } else if existing.instance_layout == config::InstanceLayoutMode::Vanilla {
            if let Some(install_dir) = vanilla_install_directory() {
                existing.install_dir = install_dir;
            }
        }
        let setup = LauncherSetup {
            migration: None,
            instance_layout: existing.instance_layout,
            portable_mode: existing.portable_mode,
            data_dir: existing.data_dir,
            register_instance_store: existing.register_instance_store,
            instance_store_path: if existing.instance_layout == config::InstanceLayoutMode::Vanilla
            {
                existing
                    .install_dir
                    .parent()
                    .map(Path::to_path_buf)
                    .or_else(|| existing.instance_store_path.clone())
            } else {
                existing.instance_store_path.clone()
            },
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

    #[cfg(windows)]
    if let Some(target) = upgrade_prompt::find_installation(args.install_dir.as_deref())? {
        if upgrade_prompt::offer(&source, &target, false, !args.no_launch)? {
            return Ok(());
        }
    }
    ui::run_installer(source)
}

fn shell_install_config() -> Result<Option<config::InstallConfig>> {
    let executable = std::env::current_exe()?;
    if let Some(directory) = executable.parent() {
        if let Some(saved) = config::load_install(directory)? { return Ok(Some(saved)); }
        if let Some(saved) = config::load_install(&directory.join("LunaLauncher"))? { return Ok(Some(saved)); }
    }
    config::load()
}

fn current_executable_is_shell_for_install_dir(install_dir: &Path) -> bool {
    let Ok(executable) = std::env::current_exe() else {
        return false;
    };
    let Some(executable_directory) = executable
        .parent()
        .and_then(|directory| directory.canonicalize().ok())
    else {
        return false;
    };
    let Some(install_directory) = install_dir.canonicalize().ok() else {
        return false;
    };
    if executable_directory == install_directory {
        return true;
    }
    // In portable vanilla/HMCL/PCL mode the shell stays in the parent folder
    // while the launcher payload lives in the LunaLauncher subdirectory.
    if config::adjacent_setup_directory(&install_directory) != Some(executable_directory.as_path()) {
        return false;
    }
    PayloadSource::from_path(executable)
        .map(|source| source.footer.is_some())
        .unwrap_or(false)
}

fn vanilla_install_directory() -> Option<PathBuf> {
    let executable = std::env::current_exe().ok()?;
    let directory = executable.parent()?.to_path_buf();
    let file_name = executable.file_name()?.to_string_lossy();
    if file_name.eq_ignore_ascii_case("luna-installer-shell.exe")
        || file_name.eq_ignore_ascii_case("luna-installer-shell")
    {
        Some(directory)
    } else {
        Some(directory.join("LunaLauncher"))
    }
}

/// Return the normal per-user launcher installation directory used by the
/// platform installer.  On Windows this mirrors the original NSIS package's
/// `$LOCALAPPDATA\\Programs\\LunaLauncher` default.  It is deliberately
/// separate from the launcher's data/instance directory.
fn default_install_directory() -> PathBuf {
    directories::BaseDirs::new()
        .map(|dirs| {
            let local_data = dirs.data_local_dir();
            if cfg!(windows) {
                local_data.join("Programs").join("LunaLauncher")
            } else {
                local_data.join("LunaLauncher")
            }
        })
        .unwrap_or_else(|| PathBuf::from("LunaLauncher"))
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

fn default_install_dir() -> String {
    default_install_directory().display().to_string()
}

#[cfg(test)]
mod tests {
    use super::default_install_directory;

    #[test]
    fn default_install_directory_is_a_launcher_install_tree() {
        let directory = default_install_directory();
        assert_eq!(
            directory.file_name().and_then(|name| name.to_str()),
            Some("LunaLauncher")
        );
        if cfg!(windows) {
            assert_eq!(
                directory
                    .parent()
                    .and_then(|parent| parent.file_name())
                    .and_then(|name| name.to_str()),
                Some("Programs")
            );
        }
    }
}
