// SPDX-License-Identifier: MPL-2.0
//! Native window ownership for the Melody shell. Geometry is shared with Vue.
use serde::Deserialize;
use std::{collections::HashMap, sync::Mutex};
use tauri::{Emitter, Manager};
use std::{io::{Read, Write}, path::{Path, PathBuf}};

const MAX_BACKGROUND_BYTES: u64 = 32 * 1024 * 1024;
fn supported_background(path: &Path) -> bool {
    matches!(path.extension().and_then(|s| s.to_str()).unwrap_or("").to_ascii_lowercase().as_str(), "png" | "jpg" | "jpeg" | "webp" | "gif" | "bmp")
}
fn background_names(directory: &Path) -> Result<Vec<String>, String> {
    let mut names = Vec::new();
    for entry in std::fs::read_dir(directory).map_err(|e| format!("无法读取背景目录：{e}"))? {
        let Ok(entry) = entry else { continue };
        if !entry.file_type().map(|t| t.is_file()).unwrap_or(false) || !supported_background(&entry.path()) { continue; }
        if entry.metadata().map(|m| m.len() > 0 && m.len() <= MAX_BACKGROUND_BYTES).unwrap_or(false) {
            names.push(entry.file_name().to_string_lossy().into_owned());
        }
    }
    names.sort();
    Ok(names)
}
fn background_bytes(directory: &Path, name: &str) -> Result<Vec<u8>, String> {
    let relative = Path::new(name);
    if relative.components().count() != 1 || !matches!(relative.components().next(), Some(std::path::Component::Normal(_))) || !supported_background(relative) {
        return Err("无效的背景图片名称".into());
    }
    let directory = directory.canonicalize().map_err(|e| e.to_string())?;
    let path = directory.join(relative).canonicalize().map_err(|e| e.to_string())?;
    if path.parent() != Some(directory.as_path()) { return Err("图片必须位于所选背景目录内".into()); }
    let file = std::fs::File::open(path).map_err(|e| e.to_string())?;
    if !file.metadata().map_err(|e| e.to_string())?.is_file() { return Err("背景不是文件".into()); }
    let mut bytes = Vec::new();
    file.take(MAX_BACKGROUND_BYTES + 1).read_to_end(&mut bytes).map_err(|e| e.to_string())?;
    if bytes.is_empty() || bytes.len() as u64 > MAX_BACKGROUND_BYTES { return Err("背景图片为空或超过 32 MB".into()); }
    Ok(bytes)
}
#[tauri::command]
pub async fn list_melody_backgrounds(directory: String) -> Result<Vec<String>, String> {
    tauri::async_runtime::spawn_blocking(move || background_names(&PathBuf::from(directory))).await.map_err(|e| e.to_string())?
}
#[tauri::command]
pub async fn read_melody_background(directory: String, name: String) -> Result<tauri::ipc::Response, String> {
    let bytes = tauri::async_runtime::spawn_blocking(move || background_bytes(&PathBuf::from(directory), &name)).await.map_err(|e| e.to_string())??;
    Ok(tauri::ipc::Response::new(bytes))
}

fn appearance_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let startup = app.state::<crate::startup::Startup>();
    let context = startup.launcher.as_ref().map_err(Clone::clone)?;
    Ok(context.data_root.join("webui").join("melody-appearance.json"))
}

fn settings_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let startup = app.state::<crate::startup::Startup>();
    let context = startup.launcher.as_ref().map_err(Clone::clone)?;
    Ok(context.data_root.join("webui").join("melody-settings.json"))
}

fn neo_preferences_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let startup = app.state::<crate::startup::Startup>();
    let context = startup.launcher.as_ref().map_err(Clone::clone)?;
    Ok(context.data_root.join("webui").join("neo-ui-preferences.json"))
}

#[tauri::command]
pub fn get_melody_appearance(app: tauri::AppHandle) -> Result<Option<serde_json::Value>, String> {
    let path = appearance_path(&app)?;
    match std::fs::read(path) {
        Ok(bytes) => serde_json::from_slice(&bytes).map(Some).map_err(|e| e.to_string()),
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(None),
        Err(error) => Err(error.to_string()),
    }
}

#[tauri::command]
pub fn set_melody_appearance(app: tauri::AppHandle, appearance: serde_json::Value) -> Result<(), String> {
    if !appearance.is_object() { return Err("外观设置必须是对象".into()); }
    let path = appearance_path(&app)?;
    let parent = path.parent().ok_or("外观设置路径无效")?;
    std::fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    let mut file = tempfile::NamedTempFile::new_in(parent).map_err(|e| e.to_string())?;
    serde_json::to_writer(&mut file, &appearance).map_err(|e| e.to_string())?;
    file.flush().map_err(|e| e.to_string())?;
    file.persist(path).map_err(|e| e.to_string())?;
    Ok(())
}

#[tauri::command]
pub fn get_melody_settings(app: tauri::AppHandle) -> Result<Option<serde_json::Value>, String> {
    let path = settings_path(&app)?;
    match std::fs::read(path) {
        Ok(bytes) => serde_json::from_slice(&bytes).map(Some).map_err(|e| e.to_string()),
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(None),
        Err(error) => Err(error.to_string()),
    }
}

#[tauri::command]
pub fn set_melody_settings(app: tauri::AppHandle, settings: serde_json::Value) -> Result<(), String> {
    if !settings.is_object() { return Err("启动器设置必须是对象".into()); }
    let path = settings_path(&app)?;
    let parent = path.parent().ok_or("启动器设置路径无效")?;
    std::fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    let mut file = tempfile::NamedTempFile::new_in(parent).map_err(|e| e.to_string())?;
    serde_json::to_writer(&mut file, &settings).map_err(|e| e.to_string())?;
    file.flush().map_err(|e| e.to_string())?;
    file.persist(path).map_err(|e| e.to_string())?;
    Ok(())
}

#[tauri::command]
pub fn get_neo_ui_preferences(app: tauri::AppHandle) -> Result<Option<serde_json::Value>, String> {
    let path = neo_preferences_path(&app)?;
    match std::fs::read(path) {
        Ok(bytes) => serde_json::from_slice(&bytes).map(Some).map_err(|e| e.to_string()),
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(None),
        Err(error) => Err(error.to_string()),
    }
}

#[tauri::command]
pub fn set_neo_ui_preferences(app: tauri::AppHandle, preferences: serde_json::Value) -> Result<(), String> {
    if !preferences.is_object() { return Err("Neo UI 设置必须是对象".into()); }
    let path = neo_preferences_path(&app)?;
    let parent = path.parent().ok_or("Neo UI 设置路径无效")?;
    std::fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    let mut file = tempfile::NamedTempFile::new_in(parent).map_err(|e| e.to_string())?;
    serde_json::to_writer(&mut file, &preferences).map_err(|e| e.to_string())?;
    file.flush().map_err(|e| e.to_string())?;
    file.persist(path).map_err(|e| e.to_string())?;
    Ok(())
}

#[derive(Deserialize)]
struct WindowSpec {
    title: String, width: f64, height: f64,
    #[serde(rename = "maxWidth")]
    max_width: Option<f64>,
}

fn message_width(spec: &WindowSpec, requested: Option<f64>) -> f64 {
    requested.filter(|width| width.is_finite())
        .unwrap_or(spec.width).clamp(spec.width, spec.max_width.unwrap_or(spec.width))
}

fn spec(kind: &str) -> Result<WindowSpec, String> {
    let mut specs: HashMap<String, WindowSpec> =
        serde_json::from_str(include_str!("../../src/themes/melody/config/windows.json"))
            .map_err(|error| error.to_string())?;
    specs.remove(kind).ok_or_else(|| "Unknown Melody window".to_string())
}

fn window_label(kind: &str, owner: &str, instance: &str, component: &str) -> String {
    if kind == "message" { return format!("melody-message-{owner}"); }
    if kind == "client" || (kind == "games" && (!instance.is_empty() || !component.is_empty())) {
        // Include lengths before encoding: delimiters in IDs cannot cause collisions.
        let identity = format!("{}:{}:{}", instance.len(), instance, component);
        let suffix: String = identity.as_bytes().iter().map(|byte| format!("{byte:02x}")).collect();
        return format!("melody-{kind}-{suffix}");
    }
    format!("melody-{kind}")
}

#[derive(Default)]
pub struct HostState(pub Mutex<Option<(tauri::PhysicalSize<u32>, bool)>>);

#[tauri::command]
pub async fn configure_melody_host(app: tauri::AppHandle, enabled: bool) -> Result<(), String> {
    let window = app.get_webview_window("main").ok_or("Main window is unavailable")?;
    let state = app.state::<HostState>();
    let mut saved = state.0.lock().map_err(|e| e.to_string())?;
    if enabled {
        if saved.is_none() {
            *saved = Some((window.inner_size().map_err(|e| e.to_string())?, window.is_resizable().map_err(|e| e.to_string())?));
        }
        // Melody's launcher keeps its original 720x450 native default. The
        // document fills this client area; its child windows use their own
        // defaults from windows.json. On exit, configure_melody_host(false)
        // restores the Neo UI size saved above.
        let launcher = spec("launcher")?;
        window
            .set_size(tauri::LogicalSize::new(launcher.width, launcher.height))
            .map_err(|e| e.to_string())?;
        window.set_resizable(true).map_err(|e| e.to_string())?;
    } else if let Some((size, resizable)) = saved.take() {
        window.set_size(size).map_err(|e| e.to_string())?;
        window.set_resizable(resizable).map_err(|e| e.to_string())?;
    }
    Ok(())
}

pub fn close_children(app: &tauri::AppHandle) {
    for (label, window) in app.webview_windows() {
        if label.starts_with("melody-") { let _ = window.close(); }
    }
}

#[tauri::command]
pub async fn exit_melody_theme(app: tauri::AppHandle) -> Result<(), String> {
    configure_melody_host(app.clone(), false).await?;
    close_children(&app);
    if let Some(main) = app.get_webview_window("main") {
        main.navigate(crate::webui::app_url("index.html")).map_err(|e| e.to_string())?;
        main.set_focus().map_err(|e| e.to_string())?;
    }
    Ok(())
}

#[tauri::command]
pub async fn open_melody_window(app: tauri::AppHandle, window: tauri::WebviewWindow,
    kind: String, message: String, title: String, message_width: Option<f64>, instance_id: Option<String>, component_uid: Option<String>) -> Result<(), String> {
    let spec = spec(&kind)?;
    if kind == "launcher" {
        if let Some(main) = app.get_webview_window("main") { return main.set_focus().map_err(|e| e.to_string()); }
        return Err("Main window is unavailable".into());
    }
    let instance_id = instance_id.unwrap_or_default();
    let component_uid = component_uid.unwrap_or_default();
    if kind == "client" && instance_id.is_empty() { return Err("Client ID is required".into()); }
    let modal = kind == "message";
    let scale = window.scale_factor().unwrap_or(1.0).max(0.01);
    let (width, height) = if modal {
        (self::message_width(&spec, message_width), spec.height)
    } else {
        // Every Melody screen is a separate native window with its own
        // historical default size. The document itself still fills the
        // resulting client area, so resizing remains fully native.
        (spec.width, spec.height)
    };
    let label = window_label(&kind, window.label(), &instance_id, &component_uid);
    if let Some(existing) = app.get_webview_window(&label) {
        if modal {
            existing.set_size(tauri::LogicalSize::new(width, height)).map_err(|e| e.to_string())?;
            existing.emit("melody-message", serde_json::json!({ "message": message, "title": title })).map_err(|e| e.to_string())?;
        }
        existing.show().map_err(|e| e.to_string())?;
        existing.unminimize().map_err(|e| e.to_string())?;
        return existing.set_focus().map_err(|e| e.to_string());
    }
    let path = format!("index.html?theme=melody&melodyWindow={}&message={}&title={}&instance={}&component={}",
        crate::percent_encode(&kind), crate::percent_encode(&message), crate::percent_encode(&title), crate::percent_encode(&instance_id), crate::percent_encode(&component_uid));
    let owner = window.clone();
    let mut builder = tauri::WebviewWindowBuilder::new(&app, label.clone(), tauri::WebviewUrl::External(crate::webui::app_url(&path)))
        .title(if title.is_empty() { spec.title } else { title })
        .inner_size(width, height).resizable(!modal).maximizable(!modal)
        .decorations(false).transparent(true).shadow(true);
    // Center the new window on the monitor that contains the requesting
    // window. Tauri's parameterless `center()` uses the primary monitor,
    // which is wrong when the launcher is on a secondary display.
    if let (Ok(position), Ok(size)) = (owner.outer_position(), owner.outer_size()) {
        let child_width = width * scale;
        let child_height = height * scale;
        let x = position.x as f64 + (size.width as f64 - child_width) / 2.0;
        let y = position.y as f64 + (size.height as f64 - child_height) / 2.0;
        builder = builder.position(x / scale, y / scale);
    } else {
        builder = builder.center();
    }
    // Regular Melody screens are independent top-level windows. Only the
    // message window is modal and owned by the window that requested it.
    if modal {
        builder = builder.parent(&owner).map_err(|e| e.to_string())?;
    }
    if let Some(path) = &app.state::<crate::startup::Startup>().webview_data_dir {
        builder = builder.data_directory(path.clone());
    }
    let child = builder.build().map_err(|e| {
        format!(
            "Melody window build failed (kind={kind}, label={label}, url={path}): {e}"
        )
    })?;
    if modal {
        owner.set_enabled(false).map_err(|e| e.to_string())?;
        child.on_window_event(move |event| {
            if matches!(event, tauri::WindowEvent::Destroyed) {
                let _ = owner.set_enabled(true);
                let _ = owner.set_focus();
            }
        });
    }
    child.set_focus().map_err(|e| e.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn background_directory_is_rescanned_and_reads_are_confined() {
        let folder = tempfile::tempdir().unwrap();
        std::fs::write(folder.path().join("旧图.JPG"), b"image").unwrap();
        std::fs::write(folder.path().join("notes.txt"), b"text").unwrap();
        std::fs::write(folder.path().join("empty.png"), b"").unwrap();
        std::fs::create_dir(folder.path().join("nested.png")).unwrap();
        assert_eq!(background_names(folder.path()).unwrap(), vec!["旧图.JPG"]);
        std::fs::write(folder.path().join("新图.webp"), b"new image").unwrap();
        assert_eq!(background_names(folder.path()).unwrap().len(), 2);
        assert_eq!(background_bytes(folder.path(), "新图.webp").unwrap(), b"new image");
        for name in ["../outside.png", "notes.txt", "empty.png", "nested.png"] { assert!(background_bytes(folder.path(), name).is_err()); }
        std::fs::remove_file(folder.path().join("旧图.JPG")).unwrap();
        assert_eq!(background_names(folder.path()).unwrap(), vec!["新图.webp"]);
        assert!(background_names(&folder.path().join("missing")).is_err());
    }
    #[test]
    fn client_windows_keep_distinct_identities() {
        assert_ne!(window_label("client", "main", "甲", ""), window_label("client", "main", "乙", ""));
        assert_eq!(window_label("client", "main", "甲", ""), window_label("client", "melody-clients", "甲", ""));
        assert_ne!(window_label("games", "main", "a:b", "c"), window_label("games", "main", "a", "b:c"));
        assert_eq!(window_label("message", "melody-client-1", "", ""), "melody-message-melody-client-1");
        assert_eq!(window_label("preferences", "main", "", ""), "melody-preferences");
    }
    #[test]
    fn dialog_width_is_bounded_for_native_callers() {
        let spec = spec("message").unwrap();
        assert_eq!(message_width(&spec, None), 460.0);
        assert_eq!(message_width(&spec, Some(710.0)), 710.0);
        for invalid in [f64::NAN, f64::INFINITY, -10.0] {
            assert_eq!(message_width(&spec, Some(invalid)), 460.0);
        }
        assert_eq!(message_width(&spec, Some(10000.0)), 900.0);
    }
    #[test]
    fn window_registry_is_bounded_and_shared() {
        assert!(spec("../../index.html").is_err());
        for kind in ["launcher", "settings", "manager", "message", "download", "setup", "components", "preferences", "games", "clients", "client"] {
            let spec = spec(kind).unwrap();
            assert!(spec.width >= 430.0 && spec.width <= 1060.0);
            assert!(spec.height >= 200.0 && spec.height <= 655.0);
            assert!(!spec.title.is_empty());
        }
    }
}
