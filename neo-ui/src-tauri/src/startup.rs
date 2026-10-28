// SPDX-License-Identifier: MPL-2.0
use serde::Deserialize;
use std::{path::PathBuf, process::Command};
use tauri::{Emitter, Manager};

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct LauncherContext {
    pub data_root: PathBuf,
    pub instance_id: String,
}

pub struct Startup {
    pub launcher: Result<LauncherContext, String>,
    pub webview_data_dir: Option<PathBuf>,
}

pub fn command() -> Result<Command, String> {
    let executable = match std::env::var_os("LUNA_LAUNCHER_CLI").filter(|p| !p.is_empty()) {
        Some(path) => PathBuf::from(path),
        None => std::env::current_exe()
            .map_err(|e| e.to_string())?
            .with_file_name(if cfg!(windows) {
                "lunalauncher-cli.exe"
            } else {
                "lunalauncher"
            }),
    };
    let mut command = Command::new(executable);
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        command.creation_flags(0x08000000); // CREATE_NO_WINDOW
    }
    Ok(command)
}

fn launcher_context() -> Result<LauncherContext, String> {
    let mut command = command()?;
    if let Some(path) = std::env::var_os("LUNA_LAUNCHER_DATA").filter(|p| !p.is_empty()) {
        command.arg("--dir").arg(path);
    }
    let output = command
        .args(["--cli", "--print-startup-context"])
        .output()
        .map_err(|e| format!("webui.error.launch_backend|{e}"))?;
    if !output.status.success() {
        // Keep the Web UI usable with an older installed CLI. The context
        // query was added after some packaged launchers were built; those
        // binaries reject the new option before doing any useful work.
        let stderr = String::from_utf8_lossy(&output.stderr);
        if stderr.contains("Unknown option") || stderr.contains("unknown option") {
            let data_root = std::env::var_os("LUNA_LAUNCHER_DATA")
                .map(PathBuf::from)
                .or_else(|| dirs::data_dir().map(|path| path.join("LunaLauncher")))
                .ok_or_else(|| "webui.error.data_directory".to_string())?;
            return Ok(LauncherContext {
                data_root,
                instance_id: "luna-launcher".into(),
            });
        }
        return Err(format!(
            "webui.error.startup_context|{}|{}",
            output.status,
            stderr.trim()
        ));
    }
    let context: LauncherContext =
        serde_json::from_slice(&output.stdout).map_err(|e| format!("webui.error.invalid_context|{e}"))?;
    if !context.data_root.is_absolute()
        || context.instance_id.is_empty()
        || !context
            .instance_id
            .bytes()
            .all(|c| c.is_ascii_alphanumeric())
    {
        return Err("webui.error.invalid_context_data".into());
    }
    Ok(context)
}

pub fn prepare(context: &mut tauri::Context<tauri::Wry>) -> Startup {
    // Scope the official single-instance plugin to the native data directory and
    // version. Resolve that ID before Tauri creates a window or starts MCP.
    let launcher = launcher_context();
    let config = context.config_mut();
    let webview_data_dir = dirs::data_local_dir().map(|root| root.join(&config.identifier));
    if let Ok(native) = &launcher {
        config.identifier = format!("{}.{}", config.identifier, native.instance_id);
    }
    // Changing the IPC namespace must not reset existing Web UI localStorage,
    // themes or login cookies. Keep Tauri's original WebView storage location.
    for window in &mut config.app.windows {
        if window.data_directory.is_none() {
            window.data_directory = webview_data_dir.clone();
        }
    }
    Startup {
        launcher,
        webview_data_dir,
    }
}

pub fn activate(app: &tauri::AppHandle) {
    let handle = app.clone();
    // Plugins can receive activation on different threads across platforms.
    // Perform window operations on Tauri's main thread.
    let _ = app.run_on_main_thread(move || {
        if let Some(window) = handle.get_webview_window("main") {
            let _ = window.show();
            let _ = window.unminimize();
            let _ = window.set_focus();
            let _ = window.emit("neo-ui-activated", ());
        }
    });
}
