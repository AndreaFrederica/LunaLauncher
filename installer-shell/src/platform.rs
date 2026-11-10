use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use anyhow::{Context, Result, bail};
use serde_json::{Value, json};
use sha2::{Digest, Sha256};

use crate::config::{EntryPoint, InstanceStoreFormat};

pub fn entry_targets(install_dir: &Path, entry_point: EntryPoint) -> Vec<(&'static str, PathBuf)> {
    let mut targets = Vec::new();
    if matches!(entry_point, EntryPoint::Classic | EntryPoint::Both) {
        targets.push((
            "LunaLauncher (Classic)",
            launcher_path(install_dir, "lunalauncher"),
        ));
    }
    if matches!(entry_point, EntryPoint::Neo | EntryPoint::Both) {
        targets.push((
            "LunaLauncher (NeoUI)",
            launcher_path(install_dir, "luna-neo-ui"),
        ));
    }
    targets
}

pub fn launch(install_dir: &Path, entry_point: EntryPoint) -> Result<()> {
    let (_, target) = entry_targets(install_dir, entry_point)
        .into_iter()
        .next()
        .context("no launcher entry point selected")?;
    if !target.is_file() {
        bail!("launcher entry point does not exist: {}", target.display());
    }
    Command::new(&target)
        .current_dir(install_dir)
        .spawn()
        .with_context(|| format!("launch {}", target.display()))?;
    Ok(())
}

/// Apply the launcher's existing portable-data and instance-store conventions.
/// The launcher reads these files from its working directory, which is also the
/// installation directory used by shortcuts and `launch` above.
pub fn configure_launcher(
    install_dir: &Path,
    portable: bool,
    register_instance_store: bool,
    instance_store_path: Option<&Path>,
    instance_store_format: InstanceStoreFormat,
) -> Result<()> {
    let portable_marker = install_dir.join("portable.txt");
    let system_marker = install_dir.join(".luna-use-system-data");
    if portable {
        fs::write(&portable_marker, "LunaLauncher portable mode\n")?;
        remove_if_exists(system_marker)?;
    } else {
        fs::write(
            &system_marker,
            "Use the user data directory; ignore portable UserData.\n",
        )?;
        remove_if_exists(portable_marker)?;
    }

    let root_registry_path = install_dir.join("instance-stores.json");
    let portable_registry_path = install_dir.join("UserData").join("instance-stores.json");
    if portable {
        // Portable launcher startup changes its working directory to UserData
        // before loading the instance registry. Keep the registry beside the
        // data files so the launcher reads the shell's registration.
        fs::create_dir_all(install_dir.join("UserData"))?;
        remove_if_exists(root_registry_path.clone())?;
    } else {
        // A system-data installation must not retain a registry from a prior
        // portable selection in the directory that is no longer read.
        remove_if_exists(portable_registry_path.clone())?;
    }
    let registry_path = if portable {
        portable_registry_path
    } else {
        root_registry_path
    };
    let mut registry = if registry_path.exists() {
        let contents = fs::read_to_string(&registry_path).with_context(|| {
            format!("read instance store registry: {}", registry_path.display())
        })?;
        serde_json::from_str::<Value>(&contents).with_context(|| {
            format!("parse instance store registry: {}", registry_path.display())
        })?
    } else {
        json!({ "formatVersion": 1, "InstanceStores": [] })
    };
    let root = registry
        .as_object_mut()
        .context("instance store registry must be a JSON object")?;
    root.insert("formatVersion".to_owned(), json!(1));
    let existing_stores = root
        .remove("InstanceStores")
        .or_else(|| root.remove("stores"))
        .unwrap_or_else(|| Value::Array(Vec::new()));
    let stores = root
        .entry("InstanceStores".to_owned())
        .or_insert(existing_stores);
    let stores = stores
        .as_array_mut()
        .context("instance store registry InstanceStores must be an array")?;
    let install_key = normalized_path(install_dir);
    let registered_path = if register_instance_store {
        let path = instance_store_path.context("registered instance store path is missing")?;
        if path.as_os_str().is_empty() {
            bail!("registered instance store path is empty");
        }
        let path = normalized_store_path(install_dir, path);
        if path.is_empty() {
            bail!("registered instance store path is empty");
        }
        Some(path)
    } else {
        None
    };
    stores.retain(|store| {
        let path = store
            .get("path")
            .and_then(Value::as_str)
            .map(|path| normalized_store_path(install_dir, Path::new(path)));
        let is_shell_registration = store
            .get("id")
            .and_then(Value::as_str)
            .is_some_and(|id| id.starts_with("installer-"));
        let is_under_install_dir = path.as_ref().is_some_and(|path| {
            path == &install_key || path.starts_with(&format!("{install_key}/"))
        });
        let is_replaced_registration = registered_path
            .as_ref()
            .zip(path.as_ref())
            .is_some_and(|(new_path, old_path)| new_path == old_path);
        !(is_shell_registration && (is_under_install_dir || is_replaced_registration))
    });
    if let Some(path) = registered_path {
        let mut hasher = Sha256::new();
        hasher.update(path.as_bytes());
        let id = format!("installer-{}", hex_lower(&hasher.finalize()));
        stores.push(json!({
            "id": id,
            "name": "LunaLauncher directory",
            "path": path,
            "format": match instance_store_format {
                InstanceStoreFormat::Prism => "prism",
                InstanceStoreFormat::VanillaCompatible => "vanilla-compatible",
            },
            "origin": "user",
            "enabled": true,
            "writable": true,
            "defaultForNewInstances": true,
            "autoEnableWhenPortable": false,
        }));
    }
    if stores.is_empty() {
        remove_if_exists(registry_path)?;
    } else {
        let contents = serde_json::to_string_pretty(&registry)?;
        fs::write(&registry_path, contents).with_context(|| {
            format!("write instance store registry: {}", registry_path.display())
        })?;
    }
    Ok(())
}

pub fn create_shortcuts(
    install_dir: &Path,
    entry_point: EntryPoint,
    desktop: bool,
    start_menu: bool,
) -> Result<Vec<PathBuf>> {
    let targets = entry_targets(install_dir, entry_point);
    let mut created = Vec::new();
    for (name, target) in targets {
        if !target.is_file() {
            bail!(
                "cannot create shortcut; launcher entry point does not exist: {}",
                target.display()
            );
        }
        if desktop {
            let path = desktop_dir()?.join(format!("{name}.lnk"));
            create_shortcut(&target, &path, install_dir, name)?;
            created.push(path);
        }
        if start_menu {
            let path = start_menu_dir()?.join(format!("{name}.lnk"));
            if let Some(parent) = path.parent() {
                std::fs::create_dir_all(parent)?;
            }
            create_shortcut(&target, &path, install_dir, name)?;
            created.push(path);
        }
    }
    Ok(created)
}

pub fn create_uninstall_shortcut(
    install_dir: &Path,
    desktop: bool,
    start_menu: bool,
) -> Result<Vec<PathBuf>> {
    let target = launcher_path(install_dir, "luna-installer-shell");
    if !target.is_file() {
        bail!(
            "cannot create uninstall shortcut; installer shell does not exist: {}",
            target.display()
        );
    }
    let arguments = format!("--uninstall \"{}\"", install_dir.display());
    let mut created = Vec::new();
    if desktop {
        let path = desktop_dir()?.join("Uninstall LunaLauncher.lnk");
        create_shortcut_with_args(
            &target,
            &path,
            install_dir,
            "Uninstall LunaLauncher",
            &arguments,
        )?;
        created.push(path);
    }
    if start_menu {
        let path = start_menu_dir()?.join("Uninstall LunaLauncher.lnk");
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent)?;
        }
        create_shortcut_with_args(
            &target,
            &path,
            install_dir,
            "Uninstall LunaLauncher",
            &arguments,
        )?;
        created.push(path);
    }
    Ok(created)
}

pub fn remove_shortcuts(
    install_dir: &Path,
    entry_point: EntryPoint,
    desktop: bool,
    start_menu: bool,
) -> Result<()> {
    for (name, _) in entry_targets(install_dir, entry_point) {
        if desktop {
            remove_if_exists(desktop_dir()?.join(format!("{name}.lnk")))?;
        }
        if start_menu {
            remove_if_exists(start_menu_dir()?.join(format!("{name}.lnk")))?;
        }
    }
    if desktop {
        remove_if_exists(desktop_dir()?.join("Uninstall LunaLauncher.lnk"))?;
    }
    if start_menu {
        remove_if_exists(start_menu_dir()?.join("Uninstall LunaLauncher.lnk"))?;
    }
    Ok(())
}

pub fn schedule_delete(path: &Path) -> Result<()> {
    let path = path.canonicalize().unwrap_or_else(|_| path.to_path_buf());
    #[cfg(windows)]
    {
        let path = windows_script_path(&path);
        let script = format!(
            "Start-Sleep -Milliseconds 700; Remove-Item -LiteralPath '{}' -Recurse -Force -ErrorAction SilentlyContinue",
            powershell_quote(&path)
        );
        Command::new("powershell.exe")
            .args([
                "-NoProfile",
                "-NonInteractive",
                "-ExecutionPolicy",
                "Bypass",
                "-Command",
                &script,
            ])
            .spawn()
            .context("schedule Windows installation removal")?;
        return Ok(());
    }
    #[cfg(not(windows))]
    {
        let script = format!(
            "sleep 1; rm -rf -- '{}'",
            shell_quote(&path.display().to_string())
        );
        Command::new("sh")
            .args(["-c", &script])
            .spawn()
            .context("schedule installation removal")?;
        Ok(())
    }
}

pub fn schedule_remove_installation(path: &Path, preserve_user_data: bool) -> Result<()> {
    let path = path.canonicalize().unwrap_or_else(|_| path.to_path_buf());
    #[cfg(windows)]
    {
        let root = powershell_quote(&windows_script_path(&path));
        let script = if preserve_user_data {
            format!(
                "Start-Sleep -Milliseconds 700; if (Test-Path -LiteralPath '{root}') {{ Get-ChildItem -LiteralPath '{root}' -Force | Where-Object {{ $_.Name -ne 'UserData' }} | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue }}"
            )
        } else {
            format!(
                "Start-Sleep -Milliseconds 700; Remove-Item -LiteralPath '{root}' -Recurse -Force -ErrorAction SilentlyContinue"
            )
        };
        Command::new("powershell.exe")
            .args([
                "-NoProfile",
                "-NonInteractive",
                "-ExecutionPolicy",
                "Bypass",
                "-Command",
                &script,
            ])
            .spawn()
            .context("schedule Windows installation removal")?;
        return Ok(());
    }
    #[cfg(not(windows))]
    {
        if preserve_user_data {
            let script = format!(
                "sleep 1; find '{}' -mindepth 1 -maxdepth 1 ! -name UserData -exec rm -rf -- {{}} +",
                shell_quote(&path.display().to_string())
            );
            Command::new("sh")
                .args(["-c", &script])
                .spawn()
                .context("schedule installation cleanup")?;
        } else {
            schedule_delete(&path)?;
        }
        Ok(())
    }
}

fn remove_if_exists(path: PathBuf) -> Result<()> {
    if path.exists() {
        std::fs::remove_file(path)?;
    }
    Ok(())
}

fn launcher_path(install_dir: &Path, stem: &str) -> PathBuf {
    #[cfg(windows)]
    {
        install_dir.join(format!("{stem}.exe"))
    }
    #[cfg(not(windows))]
    {
        install_dir.join(stem)
    }
}

fn normalized_path(path: &Path) -> String {
    path.canonicalize()
        .unwrap_or_else(|_| path.to_path_buf())
        .to_string_lossy()
        .replace('\\', "/")
        .trim_end_matches('/')
        .to_ascii_lowercase()
}

fn normalized_store_path(install_dir: &Path, path: &Path) -> String {
    if path.is_absolute() {
        normalized_path(path)
    } else {
        normalized_path(&install_dir.join(path))
    }
}

fn hex_lower(bytes: &[u8]) -> String {
    const HEX: &[u8; 16] = b"0123456789abcdef";
    let mut output = String::with_capacity(bytes.len() * 2);
    for &byte in bytes {
        output.push(HEX[(byte >> 4) as usize] as char);
        output.push(HEX[(byte & 0x0f) as usize] as char);
    }
    output
}

fn desktop_dir() -> Result<PathBuf> {
    #[cfg(windows)]
    {
        return std::env::var_os("USERPROFILE")
            .map(|path| PathBuf::from(path).join("Desktop"))
            .context("USERPROFILE is not set");
    }
    #[cfg(not(windows))]
    {
        dirs_fallback("Desktop")
    }
}

fn start_menu_dir() -> Result<PathBuf> {
    #[cfg(windows)]
    {
        return std::env::var_os("APPDATA")
            .map(|path| {
                PathBuf::from(path).join("Microsoft/Windows/Start Menu/Programs/LunaLauncher")
            })
            .context("APPDATA is not set");
    }
    #[cfg(not(windows))]
    {
        dirs_fallback(".local/share/applications")
    }
}

#[cfg(windows)]
fn create_shortcut(
    target: &Path,
    shortcut: &Path,
    working_dir: &Path,
    description: &str,
) -> Result<()> {
    create_shortcut_with_args(target, shortcut, working_dir, description, "")
}

#[cfg(windows)]
fn create_shortcut_with_args(
    target: &Path,
    shortcut: &Path,
    working_dir: &Path,
    description: &str,
    arguments: &str,
) -> Result<()> {
    if let Some(parent) = shortcut.parent() {
        std::fs::create_dir_all(parent)?;
    }
    let script = format!(
        "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut('{}'); $s.TargetPath = '{}'; $s.WorkingDirectory = '{}'; $s.Description = '{}'; $s.Arguments = '{}'; $s.Save()",
        powershell_quote(&shortcut.display().to_string()),
        powershell_quote(&target.display().to_string()),
        powershell_quote(&working_dir.display().to_string()),
        powershell_quote(description),
        powershell_quote(arguments),
    );
    let status = Command::new("powershell.exe")
        .args([
            "-NoProfile",
            "-NonInteractive",
            "-ExecutionPolicy",
            "Bypass",
            "-Command",
            &script,
        ])
        .status()
        .context("create Windows shortcut with PowerShell")?;
    if !status.success() {
        bail!(
            "PowerShell failed to create shortcut: {}",
            shortcut.display()
        );
    }
    Ok(())
}

#[cfg(not(windows))]
fn create_shortcut(
    target: &Path,
    shortcut: &Path,
    working_dir: &Path,
    description: &str,
) -> Result<()> {
    create_shortcut_with_args(target, shortcut, working_dir, description, "")
}

#[cfg(not(windows))]
fn create_shortcut_with_args(
    target: &Path,
    shortcut: &Path,
    working_dir: &Path,
    description: &str,
    arguments: &str,
) -> Result<()> {
    if let Some(parent) = shortcut.parent() {
        std::fs::create_dir_all(parent)?;
    }
    let contents = format!(
        "[Desktop Entry]\nType=Application\nName={description}\nExec={} {}\nPath={}\nTerminal=false\n",
        target.display(),
        arguments,
        working_dir.display()
    );
    std::fs::write(shortcut, contents)?;
    Ok(())
}

#[cfg(windows)]
fn powershell_quote(value: &str) -> String {
    value.replace('\'', "''")
}

#[cfg(windows)]
fn windows_script_path(path: &Path) -> String {
    let value = path.display().to_string();
    if let Some(unc) = value.strip_prefix(r"\\?\UNC\") {
        format!(r"\\{unc}")
    } else {
        value.strip_prefix(r"\\?\").unwrap_or(&value).to_owned()
    }
}

#[cfg(not(windows))]
fn shell_quote(value: &str) -> String {
    value.replace('\'', "'\\''")
}

#[cfg(not(windows))]
fn dirs_fallback(suffix: &str) -> Result<PathBuf> {
    std::env::var_os("HOME")
        .map(|path| PathBuf::from(path).join(suffix))
        .context("HOME is not set")
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::tempdir;

    #[test]
    fn vanilla_setup_enables_portable_mode_and_registers_store() {
        let directory = tempdir().unwrap();
        configure_launcher(
            directory.path(),
            true,
            true,
            Some(directory.path()),
            InstanceStoreFormat::VanillaCompatible,
        )
        .unwrap();

        assert!(directory.path().join("portable.txt").is_file());
        assert!(!directory.path().join(".luna-use-system-data").exists());
        let registry: Value = serde_json::from_str(
            &fs::read_to_string(
                directory.path().join("UserData").join("instance-stores.json"),
            )
            .unwrap(),
        )
        .unwrap();
        assert_eq!(
            registry["InstanceStores"][0]["format"],
            "vanilla-compatible"
        );
        assert_eq!(
            registry["InstanceStores"][0]["path"],
            normalized_path(directory.path())
        );
    }

    #[test]
    fn mmc_setup_disables_portable_mode_and_removes_current_store() {
        let directory = tempdir().unwrap();
        configure_launcher(
            directory.path(),
            true,
            true,
            Some(directory.path()),
            InstanceStoreFormat::VanillaCompatible,
        )
        .unwrap();
        configure_launcher(
            directory.path(),
            false,
            false,
            None,
            InstanceStoreFormat::Prism,
        )
        .unwrap();

        assert!(!directory.path().join("portable.txt").exists());
        assert!(directory.path().join(".luna-use-system-data").is_file());
        assert!(!directory.path().join("instance-stores.json").exists());
        assert!(!directory
            .path()
            .join("UserData")
            .join("instance-stores.json")
            .exists());
    }

    #[test]
    fn vanilla_registration_replaces_child_with_shell_parent() {
        let root = tempdir().unwrap();
        let install = root.path().join("LunaLauncher");
        fs::create_dir_all(&install).unwrap();
        configure_launcher(
            &install,
            true,
            true,
            Some(&install),
            InstanceStoreFormat::VanillaCompatible,
        )
        .unwrap();
        configure_launcher(
            &install,
            true,
            true,
            Some(root.path()),
            InstanceStoreFormat::VanillaCompatible,
        )
        .unwrap();

        let registry: Value = serde_json::from_str(
            &fs::read_to_string(install.join("UserData").join("instance-stores.json")).unwrap(),
        )
        .unwrap();
        let stores = registry["InstanceStores"].as_array().unwrap();
        assert_eq!(stores.len(), 1);
        assert_eq!(stores[0]["path"], normalized_path(root.path()));
        assert_eq!(stores[0]["format"], "vanilla-compatible");
    }
}
