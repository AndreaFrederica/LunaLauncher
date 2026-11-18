use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use anyhow::{Context, Result, bail};
use raw_window_handle::RawWindowHandle;
use serde_json::{Value, json};
use sha2::{Digest, Sha256};

use crate::config::{EntryPoint, InstanceStoreFormat};

pub fn show_update_error(message: &str) {
    #[cfg(windows)]
    unsafe {
        use windows_sys::Win32::UI::WindowsAndMessaging::{MessageBoxW, MB_OK, MB_ICONERROR};
        let body: Vec<u16> = message.encode_utf16().chain(Some(0)).collect();
        let title: Vec<u16> = "LunaLauncher 更新失败".encode_utf16().chain(Some(0)).collect();
        MessageBoxW(std::ptr::null_mut(), body.as_ptr(), title.as_ptr(), MB_OK | MB_ICONERROR);
    }
    #[cfg(not(windows))]
    eprintln!("LunaLauncher update failed: {message}");
}

/// Wait only for processes loaded from this installation. Never kill other
/// portable copies, or a running Java game that uses external data.
pub fn wait_for_installation_exit(target: &Path) -> Result<()> {
    #[cfg(windows)]
    {
        use windows_sys::Win32::{Foundation::{CloseHandle, INVALID_HANDLE_VALUE},
            System::{Diagnostics::ToolHelp::{CreateToolhelp32Snapshot, Process32FirstW, Process32NextW, PROCESSENTRY32W, TH32CS_SNAPPROCESS},
                Threading::{OpenProcess, QueryFullProcessImageNameW, PROCESS_QUERY_LIMITED_INFORMATION}}};
        let deadline = std::time::Instant::now() + std::time::Duration::from_secs(120);
        loop {
            let mut running = false;
            unsafe {
                let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                if snapshot == INVALID_HANDLE_VALUE { return Err(std::io::Error::last_os_error().into()); }
                let mut info: PROCESSENTRY32W = std::mem::zeroed();
                info.dwSize = std::mem::size_of_val(&info) as u32;
                let mut more = Process32FirstW(snapshot, &mut info);
                while more != 0 {
                    let process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, info.th32ProcessID);
                    if !process.is_null() {
                        let mut buffer = vec![0u16; 32768]; let mut length = buffer.len() as u32;
                        if QueryFullProcessImageNameW(process, 0, buffer.as_mut_ptr(), &mut length) != 0 {
                            let image = PathBuf::from(String::from_utf16_lossy(&buffer[..length as usize]));
                            if image.parent().and_then(|p| p.canonicalize().ok()).as_deref() == Some(target) { running = true; }
                        }
                        CloseHandle(process);
                    }
                    more = Process32NextW(snapshot, &mut info);
                }
                CloseHandle(snapshot);
            }
            if !running { return Ok(()); }
            if std::time::Instant::now() >= deadline { bail!("请关闭当前安装的所有启动器窗口后重试更新（等待超时，未修改文件）"); }
            std::thread::sleep(std::time::Duration::from_millis(200));
        }
    }
    #[cfg(not(windows))]
    { let _ = target; bail!("Automatic installation updates are currently supported only on Windows"); }
}

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
    launch_with_monitor(install_dir, entry_point, None)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct LaunchMonitor {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

/// Resolve the monitor containing the installer shell window. Windows uses
/// the work area reported by the desktop; other platforms intentionally leave
/// placement to the launcher/window manager.
pub fn monitor_for_window_handle(handle: RawWindowHandle) -> Option<LaunchMonitor> {
    #[cfg(windows)]
    {
        let RawWindowHandle::Win32(window) = handle else {
            return None;
        };
        return monitor_for_hwnd(window.hwnd.get() as _);
    }
    #[cfg(not(windows))]
    {
        let _ = handle;
        None
    }
}

/// Resolve the monitor under the pointer for the installed shell's direct
/// launch path, where there is no installer window left to inspect.
pub fn current_cursor_monitor() -> Option<LaunchMonitor> {
    #[cfg(windows)]
    {
        use windows_sys::Win32::Foundation::POINT;
        use windows_sys::Win32::Graphics::Gdi::{MONITOR_DEFAULTTONEAREST, MonitorFromPoint};
        use windows_sys::Win32::UI::WindowsAndMessaging::GetCursorPos;

        let mut point = POINT { x: 0, y: 0 };
        if unsafe { GetCursorPos(&mut point) } == 0 {
            return None;
        }
        let monitor = unsafe { MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST) };
        return monitor_from_handle(monitor);
    }
    #[cfg(not(windows))]
    {
        None
    }
}

pub fn launch_with_monitor(
    install_dir: &Path,
    entry_point: EntryPoint,
    monitor: Option<LaunchMonitor>,
) -> Result<()> {
    let (_, target) = entry_targets(install_dir, entry_point)
        .into_iter()
        .next()
        .context("no launcher entry point selected")?;
    if !target.is_file() {
        bail!("launcher entry point does not exist: {}", target.display());
    }
    let child = Command::new(&target)
        .current_dir(install_dir)
        .spawn()
        .with_context(|| format!("launch {}", target.display()))?;
    if let Some(monitor) = monitor {
        position_child_window(child.id(), monitor);
    }
    Ok(())
}

/// Open the current installer file in an archive manager when one is
/// available. The SFX is a valid executable as well as a ZIP-backed package,
/// so launching it normally would start the installer again; this explicitly
/// prefers 7-Zip/WinRAR/PeaZip and falls back to the file manager.
pub fn open_with_archive_manager(path: &Path) -> Result<()> {
    if !path.is_file() {
        bail!("installer file does not exist: {}", path.display());
    }
    #[cfg(windows)]
    {
        let mut candidates = Vec::new();
        for variable in ["ProgramFiles", "ProgramW6432", "ProgramFiles(x86)"] {
            if let Ok(root) = std::env::var(variable) {
                let root = PathBuf::from(root);
                candidates.extend([
                    root.join(r"7-Zip\7zFM.exe"),
                    root.join(r"WinRAR\WinRAR.exe"),
                    root.join(r"PeaZip\peazip.exe"),
                    root.join(r"Bandizip\Bandizip.exe"),
                    root.join(r"Bandizip\Bandizip64.exe"),
                ]);
            }
        }
        if let Ok(local_app_data) = std::env::var("LOCALAPPDATA") {
            let root = PathBuf::from(local_app_data);
            candidates.push(root.join(r"Programs\7-Zip\7zFM.exe"));
        }
        if let Some(manager) = candidates.into_iter().find(|candidate| candidate.is_file()) {
            Command::new(&manager)
                .arg(path)
                .spawn()
                .with_context(|| format!("open {} with {}", path.display(), manager.display()))?;
            return Ok(());
        }
        Command::new("explorer.exe")
            .arg("/select,")
            .arg(path)
            .spawn()
            .context("open installer location")?;
        return Ok(());
    }
    #[cfg(not(windows))]
    {
        Command::new("xdg-open")
            .arg(path)
            .spawn()
            .context("open installer with the system archive handler")?;
        Ok(())
    }
}

/// Stop launcher processes before replacing an existing installation. This is
/// the same update safety behavior provided by the NSIS package's TaskKill
/// step, while ignoring processes that are not running.
pub fn stop_running_launchers() -> Result<()> {
    #[cfg(windows)]
    {
        for image in [
            "lunalauncher.exe",
            "luna-neo-ui.exe",
            "lunalauncher-cli.exe",
            "lunalauncher_updater.exe",
        ] {
            let status = Command::new("taskkill")
                .args(["/IM", image, "/F", "/T"])
                .status()
                .with_context(|| format!("stop running {image}"))?;
            if !status.success() {
                // taskkill returns an error when the image is not running. The
                // installer can safely continue in that case.
            }
        }
    }
    Ok(())
}

/// Register the per-user Windows integration used by the normal MMC/Prism
/// installation. Portable vanilla/HMCL/PCL installs intentionally skip this
/// so copying the directory remains self-contained.
pub fn configure_windows_installation(install_dir: &Path, portable: bool) -> Result<()> {
    #[cfg(windows)]
    {
        if !portable {
            register_windows_installation(install_dir)?;
        }
    }
    #[cfg(not(windows))]
    {
        let _ = (install_dir, portable);
    }
    Ok(())
}

/// Remove the registry integration owned by the normal installation. This is
/// called by the copied uninstaller before its own directory is removed.
pub fn remove_windows_installation(install_dir: &Path) -> Result<()> {
    #[cfg(windows)]
    {
        unregister_windows_installation(install_dir)?;
    }
    #[cfg(not(windows))]
    {
        let _ = install_dir;
    }
    Ok(())
}

#[cfg(windows)]
fn register_windows_installation(install_dir: &Path) -> Result<()> {
    use winreg::RegKey;
    use winreg::enums::HKEY_CURRENT_USER;

    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    register_windows_installation_at(install_dir, &hkcu)?;
    notify_shell_association_change();
    Ok(())
}

#[cfg(windows)]
fn register_windows_installation_at(install_dir: &Path, hkcu: &winreg::RegKey) -> Result<()> {
    let launcher = launcher_path(install_dir, "lunalauncher");
    let shell = launcher_path(install_dir, "luna-installer-shell");
    let launcher_command = format!("\"{}\" \"%1\"", launcher.display());
    let import_command = format!("\"{}\" -I \"%1\"", launcher.display());

    let (product, _) = hkcu.create_subkey("Software\\LunaLauncher")?;
    product.set_value("InstallDir", &install_dir.display().to_string())?;

    let (uninstall, _) = hkcu
        .create_subkey("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LunaLauncher")?;
    uninstall.set_value("DisplayName", &"Luna Launcher")?;
    uninstall.set_value("DisplayIcon", &launcher.display().to_string())?;
    uninstall.set_value(
        "UninstallString",
        &format!(
            "\"{}\" --uninstall \"{}\"",
            shell.display(),
            install_dir.display()
        ),
    )?;
    uninstall.set_value(
        "QuietUninstallString",
        &format!(
            "\"{}\" --uninstall \"{}\"",
            shell.display(),
            install_dir.display()
        ),
    )?;
    uninstall.set_value("InstallLocation", &install_dir.display().to_string())?;
    uninstall.set_value("Publisher", &"Luna Launcher Contributors")?;
    uninstall.set_value("Version", &env!("LUNA_LAUNCHER_VERSION"))?;
    uninstall.set_value("DisplayVersion", &env!("LUNA_LAUNCHER_VERSION"))?;
    uninstall.set_value("EstimatedSize", &estimated_size_kb(install_dir))?;
    uninstall.set_value("NoModify", &1u32)?;
    uninstall.set_value("NoRepair", &1u32)?;

    let (curseforge, _) = hkcu.create_subkey("Software\\Classes\\curseforge")?;
    curseforge.set_value("URL Protocol", &"")?;
    let (curseforge_open, _) =
        hkcu.create_subkey("Software\\Classes\\curseforge\\shell\\open\\command")?;
    curseforge_open.set_value("", &launcher_command)?;

    let (launcher_url, _) = hkcu.create_subkey("Software\\Classes\\lunalauncher")?;
    launcher_url.set_value("URL Protocol", &"")?;
    let (launcher_url_open, _) =
        hkcu.create_subkey("Software\\Classes\\lunalauncher\\shell\\open\\command")?;
    launcher_url_open.set_value("", &launcher_command)?;

    let (app, _) = hkcu.create_subkey("Software\\Classes\\LunaLauncher.App")?;
    app.set_value("", &"Luna Launcher")?;
    let (icon, _) = hkcu.create_subkey("Software\\Classes\\LunaLauncher.App\\DefaultIcon")?;
    icon.set_value("", &format!("{},0", launcher.display()))?;
    let (app_open, _) =
        hkcu.create_subkey("Software\\Classes\\LunaLauncher.App\\shell\\open\\command")?;
    app_open.set_value("", &import_command)?;
    let (application, _) =
        hkcu.create_subkey("Software\\Classes\\Applications\\lunalauncher.exe")?;
    application.set_value("FriendlyAppName", &"Luna Launcher")?;
    let (application_open, _) = hkcu
        .create_subkey("Software\\Classes\\Applications\\lunalauncher.exe\\shell\\open\\command")?;
    application_open.set_value("", &import_command)?;

    let (capabilities, _) =
        hkcu.create_subkey("Software\\Classes\\Applications\\lunalauncher.exe\\Capabilities")?;
    capabilities.set_value("ApplicationDescription", &"Luna Launcher")?;
    let (associations, _) = hkcu.create_subkey(
        "Software\\Classes\\Applications\\lunalauncher.exe\\Capabilities\\FileAssociations",
    )?;
    associations.set_value(".mrpack", &"LunaLauncher.App")?;
    let registered = hkcu.create_subkey("Software\\RegisteredApplications")?.0;
    registered.set_value(
        "Luna Launcher",
        &"Software\\Classes\\Applications\\lunalauncher.exe\\Capabilities",
    )?;

    register_extension(&hkcu, ".mrpack", true)?;
    register_extension(&hkcu, ".zip", false)?;

    Ok(())
}

#[cfg(windows)]
fn register_extension(hkcu: &winreg::RegKey, extension: &str, replace_default: bool) -> Result<()> {
    let (key, _) = hkcu.create_subkey(format!("Software\\Classes\\{extension}"))?;
    if replace_default {
        let current = key.get_value::<String, _>("").ok();
        if let Some(current) =
            current.filter(|value| !value.is_empty() && value != "LunaLauncher.App")
        {
            key.set_value("LunaLauncher.App_backup", &current)?;
        }
        key.set_value("", &"LunaLauncher.App")?;
    }
    let (open_with, _) =
        hkcu.create_subkey(format!("Software\\Classes\\{extension}\\OpenWithList"))?;
    open_with.set_value("lunalauncher.exe", &"")?;
    let (progids, _) =
        hkcu.create_subkey(format!("Software\\Classes\\{extension}\\OpenWithProgids"))?;
    progids.set_value("LunaLauncher.App", &"")?;
    Ok(())
}

#[cfg(windows)]
fn unregister_windows_installation(install_dir: &Path) -> Result<()> {
    use winreg::RegKey;
    use winreg::enums::HKEY_CURRENT_USER;

    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    unregister_windows_installation_at(install_dir, &hkcu)?;
    notify_shell_association_change();
    Ok(())
}

#[cfg(windows)]
fn unregister_windows_installation_at(install_dir: &Path, hkcu: &winreg::RegKey) -> Result<()> {
    let registered_dir = hkcu
        .open_subkey("Software\\LunaLauncher")
        .ok()
        .and_then(|key| key.get_value::<String, _>("InstallDir").ok());
    if registered_dir.as_deref().map(normalized_path_string)
        != Some(normalized_path_string(&install_dir.display().to_string()))
    {
        // A portable install, or a second installation, must not remove the
        // per-user integration owned by another install tree.
        return Ok(());
    }
    for key in [
        "Software\\LunaLauncher",
        "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LunaLauncher",
        "Software\\Classes\\curseforge",
        "Software\\Classes\\lunalauncher",
        "Software\\Classes\\LunaLauncher.App",
        "Software\\Classes\\Applications\\lunalauncher.exe",
    ] {
        delete_registry_tree(&hkcu, key)?;
    }
    delete_registry_value(&hkcu, "Software\\RegisteredApplications", "Luna Launcher")?;
    unregister_extension(&hkcu, ".mrpack", true)?;
    unregister_extension(&hkcu, ".zip", false)?;
    Ok(())
}

#[cfg(windows)]
fn normalized_path_string(path: &str) -> String {
    path.replace('/', "\\")
        .trim_end_matches('\\')
        .to_ascii_lowercase()
}

#[cfg(windows)]
fn unregister_extension(
    hkcu: &winreg::RegKey,
    extension: &str,
    restore_default: bool,
) -> Result<()> {
    let path = format!("Software\\Classes\\{extension}");
    if let Ok(key) = hkcu.open_subkey_with_flags(&path, winreg::enums::KEY_READ | winreg::enums::KEY_WRITE) {
        if restore_default {
            if let Ok(previous) = key.get_value::<String, _>("LunaLauncher.App_backup") {
                key.set_value("", &previous)?;
                let _ = key.delete_value("LunaLauncher.App_backup");
            } else if key.get_value::<String, _>("").ok().as_deref() == Some("LunaLauncher.App") {
                let _ = key.delete_value("");
            }
        }
    }
    if let Ok(key) =
        hkcu.open_subkey_with_flags(format!("{path}\\OpenWithList"), winreg::enums::KEY_WRITE)
    {
        let _ = key.delete_value("lunalauncher.exe");
    }
    if let Ok(key) =
        hkcu.open_subkey_with_flags(format!("{path}\\OpenWithProgids"), winreg::enums::KEY_WRITE)
    {
        let _ = key.delete_value("LunaLauncher.App");
    }
    Ok(())
}

#[cfg(windows)]
fn notify_shell_association_change() {
    use windows_sys::Win32::UI::Shell::{SHCNE_ASSOCCHANGED, SHCNF_FLUSH, SHChangeNotify};

    unsafe {
        SHChangeNotify(
            SHCNE_ASSOCCHANGED as i32,
            SHCNF_FLUSH,
            std::ptr::null(),
            std::ptr::null(),
        );
    }
}

#[cfg(windows)]
fn delete_registry_tree(hkcu: &winreg::RegKey, path: &str) -> Result<()> {
    match hkcu.delete_subkey_all(path) {
        Ok(()) => Ok(()),
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(()),
        Err(error) => Err(error.into()),
    }
}

#[cfg(windows)]
fn delete_registry_value(hkcu: &winreg::RegKey, path: &str, name: &str) -> Result<()> {
    let key = match hkcu.open_subkey_with_flags(path, winreg::enums::KEY_WRITE) {
        Ok(key) => key,
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(()),
        Err(error) => return Err(error.into()),
    };
    match key.delete_value(name) {
        Ok(()) => Ok(()),
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(()),
        Err(error) => Err(error.into()),
    }
}

#[cfg(windows)]
fn estimated_size_kb(path: &Path) -> u32 {
    fn size(path: &Path) -> u64 {
        let Ok(metadata) = fs::metadata(path) else {
            return 0;
        };
        if metadata.is_file() {
            return metadata.len();
        }
        fs::read_dir(path)
            .into_iter()
            .flatten()
            .filter_map(|entry| entry.ok())
            .map(|entry| size(&entry.path()))
            .sum()
    }
    (size(path).saturating_add(1023) / 1024).min(u32::MAX as u64) as u32
}

#[cfg(windows)]
fn monitor_for_hwnd(hwnd: windows_sys::Win32::Foundation::HWND) -> Option<LaunchMonitor> {
    use windows_sys::Win32::Graphics::Gdi::{MONITOR_DEFAULTTONEAREST, MonitorFromWindow};

    let monitor = unsafe { MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) };
    monitor_from_handle(monitor)
}

#[cfg(windows)]
fn monitor_from_handle(
    monitor: windows_sys::Win32::Graphics::Gdi::HMONITOR,
) -> Option<LaunchMonitor> {
    use windows_sys::Win32::Graphics::Gdi::{GetMonitorInfoW, MONITORINFO};

    if monitor.is_null() {
        return None;
    }
    let mut info = MONITORINFO {
        cbSize: std::mem::size_of::<MONITORINFO>() as u32,
        ..Default::default()
    };
    if unsafe { GetMonitorInfoW(monitor, &mut info) } == 0 {
        return None;
    }
    Some(LaunchMonitor {
        x: info.rcWork.left,
        y: info.rcWork.top,
        width: info.rcWork.right.saturating_sub(info.rcWork.left),
        height: info.rcWork.bottom.saturating_sub(info.rcWork.top),
    })
}

#[cfg(not(windows))]
fn position_child_window(_pid: u32, _monitor: LaunchMonitor) {}

#[cfg(windows)]
fn position_child_window(pid: u32, monitor: LaunchMonitor) {
    // Qt/Tauri may create the top-level window a little after the process is
    // spawned, so poll briefly instead of racing the window creation.
    std::thread::spawn(move || {
        for _ in 0..100 {
            if let Some(hwnd) = find_visible_window(pid) {
                use windows_sys::Win32::UI::WindowsAndMessaging::{
                    HWND_TOP, SWP_NOACTIVATE, SWP_NOSIZE, SWP_NOZORDER, SetWindowPos,
                };
                let x = monitor.x.saturating_add(32);
                let y = monitor.y.saturating_add(32);
                unsafe {
                    SetWindowPos(
                        hwnd,
                        HWND_TOP,
                        x,
                        y,
                        0,
                        0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE,
                    );
                }
                break;
            }
            std::thread::sleep(std::time::Duration::from_millis(50));
        }
    });
}

#[cfg(windows)]
fn find_visible_window(pid: u32) -> Option<windows_sys::Win32::Foundation::HWND> {
    use windows_sys::Win32::Foundation::{HWND, LPARAM};
    use windows_sys::Win32::UI::WindowsAndMessaging::{
        EnumWindows, GetWindowThreadProcessId, IsWindowVisible,
    };
    use windows_sys::core::BOOL;

    struct Search {
        pid: u32,
        hwnd: Option<HWND>,
    }

    unsafe extern "system" fn callback(hwnd: HWND, lparam: LPARAM) -> BOOL {
        let search = unsafe { &mut *(lparam as *mut Search) };
        if unsafe { IsWindowVisible(hwnd) } == 0 {
            return 1;
        }
        let mut window_pid = 0;
        unsafe { GetWindowThreadProcessId(hwnd, &mut window_pid) };
        if window_pid == search.pid {
            search.hwnd = Some(hwnd);
            return 0;
        }
        1
    }

    let mut search = Search { pid, hwnd: None };
    unsafe {
        EnumWindows(Some(callback), &mut search as *mut Search as LPARAM);
    }
    search.hwnd
}

pub fn default_data_directory() -> Result<PathBuf> {
    Ok(directories::BaseDirs::new()
        .context("locate user config directory")?
        .config_dir()
        .join("LunaLauncher"))
}

pub fn validate_data_directory(path: &Path, install_dir: &Path) -> Result<()> {
    if !path.is_absolute() || path.parent().is_none() {
        bail!("请选择绝对路径的数据文件夹（不能是磁盘根目录）。 / Choose an absolute data folder, not a filesystem root.");
    }
    if path.components().any(|part| matches!(part, std::path::Component::ParentDir)) {
        bail!("数据目录不能包含 .. / The data folder cannot contain ..");
    }
    if path.to_str().is_none_or(|text| text.contains(['\r', '\n'])) {
        bail!("数据目录包含无效字符。 / The data folder contains invalid characters.");
    }
    // Ordinary uninstall removes the program tree. Keep custom data outside it;
    // portable mode already has an explicit preserved UserData directory.
    let data_key = normalized_path(path);
    let install_key = normalized_path(install_dir);
    // canonicalize adds the Windows extended-path prefix only for paths that
    // already exist. Compare new and existing directories in the same form.
    let data_key = data_key.strip_prefix("//?/").unwrap_or(&data_key);
    let install_key = install_key.strip_prefix("//?/").unwrap_or(&install_key);
    if data_key == install_key || data_key.starts_with(&format!("{install_key}/")) {
        bail!("数据目录须位于程序安装目录之外；如需随程序保存，请选择便携模式。 / Keep data outside the install folder, or use portable mode.");
    }
    Ok(())
}

pub fn prepare_data_directory(path: &Path, install_dir: &Path) -> Result<()> {
    validate_data_directory(path, install_dir)?;
    fs::create_dir_all(path).with_context(|| format!("create data folder: {}", path.display()))?;
    validate_data_directory(path, install_dir)?;
    tempfile::NamedTempFile::new_in(path)
        .with_context(|| format!("data folder is not writable: {}", path.display()))?;
    Ok(())
}

/// Apply the data location and register instances in that same data directory.
/// The per-install marker is also read when launching directly or through a URL.
pub fn configure_launcher(
    install_dir: &Path,
    portable: bool,
    register_instance_store: bool,
    instance_store_path: Option<&Path>,
    instance_store_format: InstanceStoreFormat,
    data_dir: Option<&Path>,
) -> Result<()> {
    let registry_path = if portable {
        install_dir.join("UserData").join("instance-stores.json")
    } else {
        let directory = match data_dir {
            Some(path) => {
                prepare_data_directory(path, install_dir)?;
                path.to_path_buf()
            }
            None => default_data_directory()?,
        };
        directory.join("instance-stores.json")
    };
    configure_launcher_at(
        install_dir,
        portable,
        register_instance_store,
        instance_store_path,
        instance_store_format,
        registry_path,
        data_dir,
    )
}

fn configure_launcher_at(
    install_dir: &Path,
    portable: bool,
    register_instance_store: bool,
    instance_store_path: Option<&Path>,
    instance_store_format: InstanceStoreFormat,
    registry_path: PathBuf,
    data_dir: Option<&Path>,
) -> Result<()> {
    let portable_marker = install_dir.join("portable.txt");
    let system_marker = install_dir.join(".luna-use-system-data");
    let data_marker = install_dir.join(".luna-data-dir");
    if portable {
        remove_if_exists(data_marker)?;
        fs::write(&portable_marker, "LunaLauncher portable mode\n")?;
        remove_if_exists(system_marker)?;
    } else {
        if let Some(path) = data_dir {
            fs::write(&data_marker, path.to_str().context("data folder must be UTF-8")?)?;
        } else {
            remove_if_exists(data_marker)?;
        }
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
        if let Some(parent) = registry_path.parent() {
            fs::create_dir_all(parent)?;
        }
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

    #[cfg(windows)]
    #[test]
    fn windows_registration_and_uninstall_restore_file_associations() {
        use winreg::{RegKey, enums::HKEY_CURRENT_USER};
        struct RegistryCleanup(String);
        impl Drop for RegistryCleanup {
            fn drop(&mut self) {
                let _ = RegKey::predef(HKEY_CURRENT_USER).delete_subkey_all(&self.0);
            }
        }
        let directory = tempdir().unwrap();
        let key_path = format!("Software\\LunaLauncherInstallerTests\\{}", directory.path().file_name().unwrap().to_string_lossy());
        let cleanup = RegistryCleanup(key_path.clone());
        let root = RegKey::predef(HKEY_CURRENT_USER).create_subkey(&key_path).unwrap().0;
        let install = directory.path().join("Program with spaces");
        fs::create_dir_all(&install).unwrap();
        for extension in [".mrpack", ".zip"] {
            root.create_subkey(format!("Software\\Classes\\{extension}")).unwrap().0
                .set_value("", &"Previous.Handler").unwrap();
        }
        register_windows_installation_at(&install, &root).unwrap();
        let value = |path: &str, name: &str| -> String { root.open_subkey(path).unwrap().get_value(name).unwrap() };
        let uninstall = value("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LunaLauncher", "UninstallString");
        assert_eq!(uninstall, format!("\"{}\" --uninstall \"{}\"", launcher_path(&install, "luna-installer-shell").display(), install.display()));
        for protocol in ["curseforge", "lunalauncher"] {
            assert_eq!(value(&format!("Software\\Classes\\{protocol}"), "URL Protocol"), "");
            assert_eq!(value(&format!("Software\\Classes\\{protocol}\\shell\\open\\command"), ""), format!("\"{}\" \"%1\"", launcher_path(&install, "lunalauncher").display()));
        }
        assert_eq!(value("Software\\Classes\\.zip", ""), "Previous.Handler");
        assert_eq!(value("Software\\Classes\\.mrpack", ""), "LunaLauncher.App");
        unregister_windows_installation_at(&directory.path().join("another"), &root).unwrap();
        assert!(root.open_subkey("Software\\LunaLauncher").is_ok());
        unregister_windows_installation_at(&install, &root).unwrap();
        assert!(root.open_subkey("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LunaLauncher").is_err());
        assert!(root.open_subkey("Software\\Classes\\lunalauncher").is_err());
        assert_eq!(value("Software\\Classes\\.mrpack", ""), "Previous.Handler");
        assert_eq!(value("Software\\Classes\\.zip", ""), "Previous.Handler");
        drop(root);
        drop(cleanup);
    }

    #[test]
    fn custom_data_directory_owns_registry_and_portable_switch_clears_override() {
        let root = tempdir().unwrap();
        let install = root.path().join("program");
        let data = root.path().join("数据 with spaces");
        let game = root.path().join("game");
        fs::create_dir_all(&install).unwrap();
        fs::create_dir_all(&data).unwrap();
        fs::write(data.join("accounts.json"), "keep existing accounts").unwrap();
        fs::create_dir_all(install.join("UserData")).unwrap();
        configure_launcher(&install, false, true, Some(&game), InstanceStoreFormat::VanillaCompatible, Some(&data)).unwrap();
        assert_eq!(fs::read_to_string(install.join(".luna-data-dir")).unwrap(), data.to_str().unwrap());
        let registry: Value = serde_json::from_str(&fs::read_to_string(data.join("instance-stores.json")).unwrap()).unwrap();
        assert_eq!(registry["InstanceStores"][0]["path"], normalized_path(&game));
        assert_eq!(fs::read_to_string(data.join("accounts.json")).unwrap(), "keep existing accounts");
        configure_launcher(&install, true, false, None, InstanceStoreFormat::Prism, Some(&data)).unwrap();
        assert!(!install.join(".luna-data-dir").exists());
        assert!(install.join("portable.txt").exists());
        assert!(data.join("instance-stores.json").is_file());
    }

    #[test]
    fn data_directory_validation_rejects_invalid_targets() {
        let root = tempdir().unwrap();
        let install = root.path().join("program");
        fs::create_dir_all(&install).unwrap();
        assert!(validate_data_directory(Path::new("relative"), &install).is_err());
        assert!(validate_data_directory(&install.join("data"), &install).is_err());
        assert!(validate_data_directory(&root.path().join(".."), &install).is_err());
        let file = root.path().join("file");
        fs::write(&file, "existing").unwrap();
        assert!(prepare_data_directory(&file, &install).is_err());
        assert_eq!(fs::read_to_string(file).unwrap(), "existing");
    }

    #[test]
    fn vanilla_setup_enables_portable_mode_and_registers_store() {
        let directory = tempdir().unwrap();
        configure_launcher_at(
            directory.path(),
            true,
            true,
            Some(directory.path()),
            InstanceStoreFormat::VanillaCompatible,
            directory
                .path()
                .join("UserData")
                .join("instance-stores.json"),
            None,
        )
        .unwrap();

        assert!(directory.path().join("portable.txt").is_file());
        assert!(!directory.path().join(".luna-use-system-data").exists());
        let registry: Value = serde_json::from_str(
            &fs::read_to_string(
                directory
                    .path()
                    .join("UserData")
                    .join("instance-stores.json"),
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
        configure_launcher_at(
            directory.path(),
            true,
            true,
            Some(directory.path()),
            InstanceStoreFormat::VanillaCompatible,
            directory.path().join("test-registry.json"),
            None,
        )
        .unwrap();
        configure_launcher_at(
            directory.path(),
            false,
            false,
            None,
            InstanceStoreFormat::Prism,
            directory.path().join("test-registry.json"),
            None,
        )
        .unwrap();

        assert!(!directory.path().join("portable.txt").exists());
        assert!(directory.path().join(".luna-use-system-data").is_file());
        assert!(!directory.path().join("test-registry.json").exists());
        assert!(
            !directory
                .path()
                .join("UserData")
                .join("instance-stores.json")
                .exists()
        );
    }

    #[test]
    fn vanilla_registration_replaces_child_with_shell_parent() {
        let root = tempdir().unwrap();
        let install = root.path().join("LunaLauncher");
        fs::create_dir_all(&install).unwrap();
        configure_launcher_at(
            &install,
            true,
            true,
            Some(&install),
            InstanceStoreFormat::VanillaCompatible,
            install.join("UserData").join("instance-stores.json"),
            None,
        )
        .unwrap();
        configure_launcher_at(
            &install,
            true,
            true,
            Some(root.path()),
            InstanceStoreFormat::VanillaCompatible,
            install.join("UserData").join("instance-stores.json"),
            None,
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
