use std::path::{Path, PathBuf};
use std::process::Command;

use anyhow::{Context, Result, bail};

use crate::config::EntryPoint;

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
