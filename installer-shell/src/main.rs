use std::path::{Path, PathBuf};

use anyhow::{Context, Result};
use clap::Parser;
use luna_installer_shell::{archive, config, platform, sfx::PayloadSource};

mod app;
mod ui;

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
        && let Some(saved) = config::load()?
            .filter(|saved| current_executable_is_shell_for_install_dir(&saved.install_dir))
    {
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
        } else if existing.instance_layout == config::InstanceLayoutMode::Vanilla {
            if let Some(install_dir) = vanilla_install_directory() {
                existing.install_dir = install_dir;
            }
        }
        let setup = LauncherSetup {
            migration: None,
            instance_layout: existing.instance_layout,
            portable_mode: existing.portable_mode,
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

    ui::run_installer(source)
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
    if install_directory
        .file_name()
        .is_none_or(|name| !name.eq_ignore_ascii_case("LunaLauncher"))
        || install_directory.parent() != Some(executable_directory.as_path())
    {
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
