use std::{
    fs,
    path::{Path, PathBuf},
    process::Command,
};

use anyhow::{Context, Result};
use luna_installer_shell::{
    config,
    i18n::Language,
    platform,
    sfx::PayloadSource,
    version::{LauncherVersion, package_version, probe_launcher},
};
use rfd::{MessageButtons, MessageDialog, MessageDialogResult, MessageLevel};

pub(crate) fn find_installation(explicit: Option<&Path>) -> Result<Option<PathBuf>> {
    if let Some(target) = explicit {
        return Ok(Some(
            installation_at(target).context("the selected directory has no lunalauncher.exe")?,
        ));
    }
    let executable = std::env::current_exe()?;
    let folder = executable.parent().context("locate Setup directory")?;
    let candidates = [
        adjacent_installation(folder),
        adjacent_installation(&folder.join("LunaLauncher")),
        config::load().ok().flatten().map(|saved| saved.install_dir),
        platform::registered_install_directory(),
        Some(crate::default_install_directory()),
    ];
    Ok(candidates
        .into_iter()
        .flatten()
        .find_map(|path| installation_at(&path)))
}

fn adjacent_installation(path: &Path) -> Option<PathBuf> {
    [
        config::INSTALL_RECORD,
        "portable.txt",
        ".luna-data-dir",
        ".luna-use-system-data",
        "UserData",
    ]
    .iter()
    .any(|marker| path.join(marker).exists())
    .then(|| path.to_path_buf())
}

fn installation_at(path: &Path) -> Option<PathBuf> {
    let path = path.canonicalize().ok()?;
    (path.parent().is_some() && path.join("lunalauncher.exe").is_file()).then_some(path)
}

/// Return true if the interaction consumed this invocation. The installed
/// launcher entry may keep launching normally when its own copy is current.
pub(crate) fn offer(
    source: &PayloadSource,
    target: &Path,
    installed_shell: bool,
    launch: bool,
) -> Result<bool> {
    let Some(available) = package_version(source)? else {
        return Ok(false);
    };
    let chinese = Language::detect() == Language::SimplifiedChinese;
    let dialog = || {
        MessageDialog::new().set_title(if chinese {
            "LunaLauncher 更新"
        } else {
            "LunaLauncher Update"
        })
    };
    let installed = match probe_launcher(&target.join("lunalauncher.exe")) {
        Ok(version) => version,
        Err(error) if installed_shell => {
            eprintln!("Could not check installed launcher version: {error:#}");
            return Ok(false);
        }
        Err(error) => {
            let description = if chinese {
                format!(
                    "无法识别这个安装的版本。\n{}\n\n{error:#}",
                    target.display()
                )
            } else {
                format!(
                    "Could not read the installed version.\n{}\n\n{error:#}",
                    target.display()
                )
            };
            dialog()
                .set_level(MessageLevel::Error)
                .set_description(description)
                .show();
            return Ok(true);
        }
    };

    if installed >= available {
        if installed_shell {
            return Ok(false);
        }
        let description = if chinese {
            format!(
                "当前版本 {installed}，安装包版本 {available}。\n{}\n\n此安装无需更新。",
                target.display()
            )
        } else {
            format!(
                "Installed version {installed}. Package version {available}.\n{}\n\nThis installation is up to date.",
                target.display()
            )
        };
        dialog().set_description(description).show();
        return Ok(true);
    }

    let description = if chinese {
        format!(
            "将 LunaLauncher 从 {installed} 更新到 {available}？\n{}\n\n请先退出该启动器及其游戏。实例和设置会保留。",
            target.display()
        )
    } else {
        format!(
            "Update LunaLauncher from {installed} to {available}?\n{}\n\nClose this launcher and its games first. Your instances and settings will be kept.",
            target.display()
        )
    };
    if dialog()
        .set_description(description)
        .set_buttons(MessageButtons::YesNo)
        .show()
        == MessageDialogResult::Yes
    {
        let target = target.canonicalize()?;
        let result = start_update(source, &target, available, launch);
        if let Err(error) = &result {
            platform::show_update_error(&format!("{error:#}"));
        }
        result?;
    }
    Ok(true)
}

fn start_update(
    source: &PayloadSource,
    target: &Path,
    available: LauncherVersion,
    launch: bool,
) -> Result<()> {
    let executable = std::env::current_exe()?.canonicalize()?;
    let source_path = source.path.canonicalize()?;
    if !executable.starts_with(target) && !source_path.starts_with(target) {
        return crate::ui::run_update(
            source.clone(),
            target.to_path_buf(),
            None,
            None,
            launch,
            Some(available),
        );
    }

    // A running Setup inside the tree would lock the very directory being
    // replaced. Launch its copy outside the installation and let this process exit.
    let work = tempfile::Builder::new()
        .prefix("luna-local-update-")
        .tempdir()?;
    let worker = work.path().join("luna-installer-shell.exe");
    fs::copy(&executable, &worker)?;
    let mut command = Command::new(worker);
    command
        .current_dir(work.path())
        .arg("--update")
        .arg("--install-dir")
        .arg(target)
        .arg("--upgrade-to")
        .arg(available.to_string());
    if !launch {
        command.arg("--no-launch");
    }
    if source_path != executable {
        let payload = work.path().join("payload.zip");
        fs::copy(source_path, &payload)?;
        command.arg("--payload").arg(payload);
    }
    command.spawn().context("open update progress window")?;
    let _ = work.keep();
    Ok(())
}
