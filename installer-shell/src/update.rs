//! Update an existing Windows installation without running first-install setup.
use crate::{
    archive,
    config::{self, EntryPoint, InstallConfig, InstanceLayoutMode, InstanceStoreFormat},
    platform,
    sfx::PayloadSource,
};
use anyhow::{Context, Result, bail};
use std::{
    collections::HashSet,
    fs,
    path::{Path, PathBuf},
    process::Command,
};

fn existing_config(target: &Path) -> Result<InstallConfig> {
    let local = config::load_install(target)?;
    let legacy = if local.is_none() {
        config::load()?
            .filter(|saved| saved.install_dir.canonicalize().ok().as_deref() == Some(target))
    } else {
        None
    };
    let mut saved = local.or(legacy).unwrap_or(InstallConfig {
        install_dir: target.to_path_buf(),
        installed_entries: Vec::new(),
        entry_point: EntryPoint::Classic,
        desktop_shortcuts: false,
        start_menu_shortcuts: false,
        keep_user_data: true,
        instance_layout: InstanceLayoutMode::Custom,
        portable_mode: false,
        data_dir: None,
        register_instance_store: false,
        instance_store_path: None,
        instance_store_format: InstanceStoreFormat::Prism,
    });
    // Settings may have changed since the last installer run. Markers win.
    let marker = target.join(".luna-data-dir");
    saved.data_dir = if marker.is_file() {
        Some(PathBuf::from(fs::read_to_string(marker)?.trim()))
    } else {
        None
    };
    saved.portable_mode = saved.data_dir.is_none()
        && !target.join(".luna-use-system-data").exists()
        && (target.join("portable.txt").exists() || target.join("UserData").is_dir());
    Ok(saved)
}

fn protected(name: &str) -> bool {
    let normalized = name.replace('\\', "/");
    let first = normalized
        .split('/')
        .find(|part| !part.is_empty() && *part != ".")
        .unwrap_or("")
        .to_ascii_lowercase();
    matches!(
        first.as_str(),
        "userdata"
            | "portable.txt"
            | "portable.txt.disabled"
            | ".luna-data-dir"
            | ".luna-use-system-data"
            | ".luna-install.json"
            | ".luna-update.json"
            | ".luna-update.lock"
            | "lunalauncher.cfg"
            | "accounts.json"
            | "instances"
            | "assets"
            | "libraries"
            | "cache"
            | "meta"
    )
}

fn preserve(
    old: &Path,
    new: &Path,
    relative: &Path,
    owned: &HashSet<PathBuf>,
    moved: &mut Vec<PathBuf>,
) -> Result<()> {
    let source = old.join(relative);
    let destination = new.join(relative);
    let metadata = fs::symlink_metadata(&source)?;
    // Records are replaced during finalization; leave the originals in the
    // backup so rollback never restores a rewritten record.
    if matches!(
        relative.to_str(),
        Some(
            ".luna-install.json"
                | ".luna-update.json"
                | ".luna-update.lock"
                | "luna-installer-shell.exe"
        )
    ) {
        return Ok(());
    }
    if metadata.is_dir()
        && !metadata.file_type().is_symlink()
        && relative != Path::new("UserData")
        && (destination.exists() || owned.iter().any(|path| path.starts_with(relative)))
    {
        for item in fs::read_dir(source)? {
            preserve(old, new, &relative.join(item?.file_name()), owned, moved)?;
        }
    } else if !owned.contains(relative) && !destination.exists() {
        if let Some(parent) = destination.parent() {
            fs::create_dir_all(parent)?;
        }
        fs::rename(&source, &destination)
            .with_context(|| format!("preserve {}", source.display()))?;
        moved.push(relative.to_path_buf());
    }
    Ok(())
}

fn restore_preserved(from: &Path, backup: &Path, moved: &[PathBuf]) -> Result<()> {
    for relative in moved.iter().rev() {
        let destination = backup.join(relative);
        if let Some(parent) = destination.parent() {
            fs::create_dir_all(parent)?;
        }
        fs::rename(from.join(relative), destination)?;
    }
    Ok(())
}

/// The old program directory remains as a sibling backup after success.
/// On activation/finalization failure the original files and runtime state return.
pub fn replace<F>(
    source: &PayloadSource,
    target: &Path,
    old_entries: &[String],
    finalize: F,
) -> Result<PathBuf>
where
    F: FnOnce(&Path, &[String]) -> Result<()>,
{
    replace_with_progress(source, target, old_entries, finalize, &mut |_, _| {})
}

fn replace_with_progress<F>(
    source: &PayloadSource,
    target: &Path,
    old_entries: &[String],
    finalize: F,
    progress: &mut dyn FnMut(f32, &str),
) -> Result<PathBuf>
where
    F: FnOnce(&Path, &[String]) -> Result<()>,
{
    progress(0.12, "正在校验更新内容…");
    let entries = archive::list(source)?;
    if entries.iter().any(|entry| protected(entry)) {
        bail!("update payload contains installation state or UserData");
    }
    let mut unique = HashSet::new();
    for entry in &entries {
        let normalized = entry.replace('\\', "/");
        if normalized.split('/').any(|part| {
            part != "." && (part.ends_with('.') || part.ends_with(' ') || part.contains(':'))
        }) {
            bail!("update payload contains an ambiguous Windows path: {entry}");
        }
        if !unique.insert(normalized.to_lowercase()) {
            bail!("update payload contains duplicate Windows paths");
        }
    }
    let parent = target
        .parent()
        .context("installation has no parent directory")?;
    let staging = tempfile::Builder::new()
        .prefix(".luna-update-stage-")
        .tempdir_in(parent)?;
    archive::extract_with_progress(source, staging.path(), &mut |done, total| {
        progress(
            0.15 + 0.55 * done as f32 / total.max(1) as f32,
            "正在解压更新文件…",
        );
    })?;
    let backup = tempfile::Builder::new()
        .prefix(".luna-update-backup-")
        .tempdir_in(parent)?
        .keep();
    fs::remove_dir(&backup)?;
    fs::rename(target, &backup)
        .context("close all windows belonging to this installation before updating")?;
    let owned: HashSet<PathBuf> = old_entries
        .iter()
        .chain(entries.iter())
        .filter(|name| !name.ends_with('/') && !protected(name))
        .map(PathBuf::from)
        .collect();
    let mut moved = Vec::new();
    let mut activated = false;
    let result = (|| {
        progress(0.75, "正在保留数据并替换程序…");
        for item in fs::read_dir(&backup)? {
            preserve(
                &backup,
                staging.path(),
                Path::new(&item?.file_name()),
                &owned,
                &mut moved,
            )?;
        }
        fs::rename(staging.path(), target)?;
        activated = true;
        progress(0.9, "正在验证程序并更新系统注册…");
        finalize(target, &entries)
    })();
    if let Err(error) = result {
        progress(0.9, "更新失败，正在恢复原来的程序…");
        let current = if activated { target } else { staging.path() };
        restore_preserved(current, &backup, &moved).with_context(|| {
            format!(
                "update failed ({error:#}); recover backup at {}",
                backup.display()
            )
        })?;
        if activated {
            fs::remove_dir_all(target)?;
        }
        fs::rename(&backup, target).context("restore previous installation")?;
        return Err(error.context("update failed; previous installation restored"));
    }
    Ok(backup)
}

pub fn apply(
    source: &PayloadSource,
    directory: &Path,
    data_root: Option<&Path>,
    restart: Option<EntryPoint>,
    launch: bool,
) -> Result<()> {
    apply_with_progress(
        source,
        directory,
        data_root,
        restart,
        launch,
        &mut |_, _| {},
    )
}

pub fn apply_with_progress(
    source: &PayloadSource,
    directory: &Path,
    data_root: Option<&Path>,
    restart: Option<EntryPoint>,
    launch: bool,
    progress: &mut dyn FnMut(f32, &str),
) -> Result<()> {
    use std::io::Write;
    if data_root.is_some_and(|path| !path.is_absolute() || !path.is_dir()) {
        bail!("update data root must be an existing absolute directory");
    }
    let mut last_stage = String::new();
    let mut report = |fraction, text: &str| {
        if text != last_stage {
            if let Some(data) = data_root {
                if let Ok(mut log) = fs::OpenOptions::new()
                    .create(true)
                    .append(true)
                    .open(data.join("logs/prism_launcher_update.log"))
                {
                    let _ = writeln!(log, "{text}");
                }
            }
            last_stage = text.to_owned();
        }
        progress(fraction, text);
    };
    let progress: &mut dyn FnMut(f32, &str) = &mut report;
    let target = directory
        .canonicalize()
        .context("resolve existing installation")?;
    if target.parent().is_none() || target.file_name().is_none() {
        bail!("refusing to update a filesystem root");
    }
    if !target.join("lunalauncher.exe").is_file() {
        bail!("target is not a Windows Luna installation");
    }
    if std::env::current_exe()?
        .parent()
        .and_then(|p| p.canonicalize().ok())
        .as_deref()
        == Some(&target)
    {
        bail!("run the update package from outside the installation directory");
    }
    let mut saved = existing_config(&target)?;
    let entry = restart.unwrap_or(saved.entry_point);
    let entries = archive::list(source)?;
    if !entries.iter().any(|name| name == "lunalauncher.exe") {
        bail!("update package is missing the launcher executable");
    }
    if (target.join("luna-neo-ui.exe").is_file()
        || matches!(saved.entry_point, EntryPoint::Neo | EntryPoint::Both))
        && !entries.iter().any(|name| name == "luna-neo-ui.exe")
    {
        bail!("update package would remove the installed Neo UI");
    }
    for (_, exe) in platform::entry_targets(&target, entry) {
        if !entries
            .iter()
            .any(|name| Some(name.as_str()) == exe.file_name().and_then(|n| n.to_str()))
        {
            bail!("update package does not include the selected UI");
        }
    }
    progress(0.05, "等待当前安装的启动器退出（最长 120 秒）…");
    platform::wait_for_installation_exit(&target)?;
    // Keep the lock outside the directory being renamed (Windows open-file semantics).
    let lock_path = target.with_file_name(format!(
        ".{}.luna-update.lock",
        target.file_name().unwrap().to_string_lossy()
    ));
    let _lock = fs::OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(&lock_path)
        .context("an update is already active; inspect .luna-update.lock before retrying")?;
    let result = (|| {
        let old_entries = saved.installed_entries.clone();
        let backup = replace_with_progress(
            source,
            &target,
            &old_entries,
            |new, entries| {
                let shell = new.join("luna-installer-shell.exe");
                fs::copy(std::env::current_exe()?, shell)?;
                // Keep setup choices; updating never calls configure_launcher or migration.
                saved.installed_entries = entries.to_vec();
                config::save_install(&saved)?;
                let mut child = Command::new(new.join("lunalauncher.exe"))
                    .arg("--version")
                    .current_dir(new)
                    .spawn()
                    .context("check updated launcher executable")?;
                let deadline = std::time::Instant::now() + std::time::Duration::from_secs(15);
                loop {
                    if let Some(status) = child.try_wait()? {
                        if !status.success() {
                            bail!("updated launcher failed its version check");
                        }
                        break;
                    }
                    if std::time::Instant::now() >= deadline {
                        let _ = child.kill();
                        let _ = child.wait();
                        bail!("updated launcher version check timed out");
                    }
                    std::thread::sleep(std::time::Duration::from_millis(100));
                }
                platform::configure_windows_installation(new, saved.portable_mode)?;
                fs::write(
                    new.join(".luna-update.json"),
                    serde_json::to_vec_pretty(&serde_json::json!({
                        "status": "installed", "restartEntry": entry,
                    }))?,
                )?;
                Ok(())
            },
            progress,
        )?;
        eprintln!("Previous installation retained at {}", backup.display());
        let _ = fs::write(
            target.join(".luna-update.json"),
            serde_json::to_vec_pretty(&serde_json::json!({
                "backupDirectory": backup, "status": "installed", "restartEntry": entry,
            }))?,
        );
        if let Some(data) = data_root {
            if let Ok(mut log) = fs::OpenOptions::new()
                .create(true)
                .append(true)
                .open(data.join("logs/prism_launcher_update.log"))
            {
                let _ = writeln!(
                    log,
                    "Previous installation retained at {}",
                    backup.display()
                );
            }
        }
        if let Some(data) = data_root {
            let _ = fs::remove_file(data.join(".prism_launcher_update.fail"));
            let _ = fs::write(
                data.join(".lunalauncher_update.success"),
                "Windows installation updated\n",
            );
        }
        if launch {
            progress(0.98, "更新完成，正在重新启动…");
            let executable = platform::entry_targets(
                &target,
                if entry == EntryPoint::Both {
                    EntryPoint::Classic
                } else {
                    entry
                },
            )[0]
            .1
            .clone();
            let mut command = Command::new(executable);
            command.current_dir(&target);
            if let Some(data) = data_root {
                if entry == EntryPoint::Neo {
                    command.env("LUNA_LAUNCHER_DATA", data);
                } else {
                    command.arg("--dir").arg(data);
                }
            }
            command
                .spawn()
                .context("restart updated launcher; installation backup was retained")?;
        }
        Ok(())
    })();
    drop(_lock);
    let _ = fs::remove_file(&lock_path);
    if result.is_ok() {
        progress(1.0, "更新完成");
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn update_preserves_runtime_state_removes_obsolete_files_and_retains_backup() {
        let root = tempfile::tempdir().unwrap();
        let target = root.path().join("程序 (2)");
        let payload = root.path().join("payload");
        fs::create_dir_all(target.join("UserData/instances")).unwrap();
        fs::create_dir(&payload).unwrap();
        for (name, data) in [
            ("old.dll", "old"),
            ("launcher.exe", "v1"),
            (".luna-data-dir", "D:/custom"),
            ("UserData/instances/world", "save"),
            ("custom.json5", "overlay"),
        ] {
            fs::write(target.join(name), data).unwrap();
        }
        fs::write(payload.join("launcher.exe"), "v2").unwrap();
        let zip = root.path().join("new.zip");
        archive::create_zip(&payload, &zip).unwrap();
        let backup = replace(
            &PayloadSource::from_path(zip).unwrap(),
            &target,
            &["old.dll".into(), "launcher.exe".into()],
            |_, _| Ok(()),
        )
        .unwrap();
        assert_eq!(
            fs::read_to_string(target.join("launcher.exe")).unwrap(),
            "v2"
        );
        assert!(!target.join("old.dll").exists());
        assert!(backup.join("old.dll").exists());
        assert_eq!(
            fs::read_to_string(target.join("UserData/instances/world")).unwrap(),
            "save"
        );
        assert_eq!(
            fs::read_to_string(target.join(".luna-data-dir")).unwrap(),
            "D:/custom"
        );
        assert!(target.join("custom.json5").exists());
    }
    #[test]
    fn finalization_failure_restores_old_program_and_userdata() {
        let root = tempfile::tempdir().unwrap();
        let target = root.path().join("installed");
        let payload = root.path().join("payload");
        fs::create_dir_all(target.join("UserData")).unwrap();
        fs::create_dir(&payload).unwrap();
        fs::write(target.join("app.exe"), "v1").unwrap();
        fs::write(target.join("UserData/world"), "save").unwrap();
        fs::write(target.join(".luna-install.json"), "original-record").unwrap();
        fs::write(target.join("luna-installer-shell.exe"), "old-shell").unwrap();
        fs::write(payload.join("app.exe"), "v2").unwrap();
        let zip = root.path().join("new.zip");
        archive::create_zip(&payload, &zip).unwrap();
        assert!(
            replace(
                &PayloadSource::from_path(zip).unwrap(),
                &target,
                &["app.exe".into()],
                |new, _| {
                    fs::write(new.join(".luna-install.json"), "new-record")?;
                    fs::write(new.join("luna-installer-shell.exe"), "new-shell")?;
                    bail!("simulated registration failure")
                }
            )
            .is_err()
        );
        assert_eq!(fs::read_to_string(target.join("app.exe")).unwrap(), "v1");
        assert_eq!(
            fs::read_to_string(target.join("UserData/world")).unwrap(),
            "save"
        );
        assert_eq!(
            fs::read_to_string(target.join(".luna-install.json")).unwrap(),
            "original-record"
        );
        assert_eq!(
            fs::read_to_string(target.join("luna-installer-shell.exe")).unwrap(),
            "old-shell"
        );
    }

    #[test]
    fn rejects_state_payload_and_corrupt_zip_without_touching_installation() {
        use std::io::Write;
        let root = tempfile::tempdir().unwrap();
        let target = root.path().join("installed");
        fs::create_dir(&target).unwrap();
        fs::write(target.join("app.exe"), "v1").unwrap();
        let zip = root.path().join("state.zip");
        for name in [
            "UserData/world",
            "userdata/world",
            r".\UserData\world",
            ".luna-install.json",
        ] {
            let mut writer = zip::ZipWriter::new(fs::File::create(&zip).unwrap());
            writer
                .start_file(name, zip::write::SimpleFileOptions::default())
                .unwrap();
            writer.write_all(b"must not overwrite").unwrap();
            writer.finish().unwrap();
            assert!(
                replace(
                    &PayloadSource::from_path(&zip).unwrap(),
                    &target,
                    &[],
                    |_, _| Ok(())
                )
                .is_err()
            );
        }
        fs::write(&zip, "not a ZIP archive").unwrap();
        assert!(
            replace(
                &PayloadSource::from_path(&zip).unwrap(),
                &target,
                &[],
                |_, _| Ok(())
            )
            .is_err()
        );
        assert_eq!(fs::read_to_string(target.join("app.exe")).unwrap(), "v1");
    }

    #[test]
    fn moved_installation_uses_own_record_and_current_data_markers() {
        let root = tempfile::tempdir().unwrap();
        let target = root.path().canonicalize().unwrap();
        let mut saved: InstallConfig = serde_json::from_str(
            r#"{"install_dir":"old/location","entry_point":"neo","portable_mode":false}"#,
        )
        .unwrap();
        saved.install_dir = target.clone();
        config::save_install(&saved).unwrap();
        fs::create_dir(target.join("UserData")).unwrap();
        let detected = existing_config(&target).unwrap();
        assert_eq!(detected.entry_point, EntryPoint::Neo);
        assert!(detected.portable_mode);
        fs::write(target.join(".luna-use-system-data"), "").unwrap();
        assert!(!existing_config(&target).unwrap().portable_mode);
        fs::write(target.join(".luna-data-dir"), "D:/external data").unwrap();
        assert_eq!(
            existing_config(&target).unwrap().data_dir,
            Some(PathBuf::from("D:/external data"))
        );
    }
}
