// SPDX-License-Identifier: GPL-3.0-only
use serde::{Deserialize, Serialize};
use std::{
    fs,
    io::Write,
    path::{Component, Path, PathBuf},
    sync::Mutex,
};
use tauri::{Manager, State};

#[derive(Clone, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
struct Manifest {
    name: Option<String>,
    version: Option<String>,
    description: Option<String>,
    api_version: Option<u32>,
    native_frame: Option<bool>,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Entry {
    pub id: String,
    pub name: String,
    version: String,
    description: String,
    source: String,
    pub path: Option<PathBuf>,
    pub error: Option<String>,
    pub native_frame: bool,
}

#[derive(Clone, Serialize)]
pub struct Root {
    source: String,
    path: PathBuf,
}

#[derive(Clone, Serialize, Deserialize, PartialEq, Debug)]
#[serde(rename_all = "camelCase", default, deny_unknown_fields)]
pub struct Settings {
    remember_last_ui: bool,
    startup_ui: String,
    scan_install_directory: bool,
    scan_data_directory: bool,
    additional_directories: Vec<PathBuf>,
    show_manager_on_start: bool,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            remember_last_ui: true,
            startup_ui: "builtin".into(),
            scan_install_directory: true,
            scan_data_directory: true,
            additional_directories: Vec::new(),
            show_manager_on_start: false,
        }
    }
}

#[derive(Serialize, Deserialize)]
#[serde(default)]
struct Preference {
    selected: String,
    settings: Settings,
}

impl Default for Preference {
    fn default() -> Self {
        Self {
            selected: "builtin".into(),
            settings: Settings::default(),
        }
    }
}

fn directory_key(path: &Path) -> String {
    let key = path.to_string_lossy().replace('\\', "/");
    let key = key.trim_end_matches('/');
    if cfg!(windows) {
        key.to_lowercase()
    } else {
        key.into()
    }
}

fn roots_for(base: &[Root], settings: &Settings) -> Vec<Root> {
    let mut roots: Vec<_> = base
        .iter()
        .filter(|root| match root.source.as_str() {
            "install" => settings.scan_install_directory,
            "data" => settings.scan_data_directory,
            _ => false,
        })
        .cloned()
        .collect();
    for path in &settings.additional_directories {
        if roots
            .iter()
            .any(|root| directory_key(&root.path) == directory_key(path))
        {
            continue;
        }
        roots.push(Root {
            // The path-derived ID is stable when directories are reordered.
            source: format!(
                "custom:{}",
                percent_encoding::utf8_percent_encode(
                    &directory_key(path),
                    percent_encoding::NON_ALPHANUMERIC
                )
            ),
            path: path.clone(),
        });
    }
    roots
}

fn validate_settings(settings: &Settings) -> Result<(), String> {
    if settings.startup_ui.is_empty() || settings.startup_ui.len() > 16384 {
        return Err("默认界面 ID 无效".into());
    }
    if settings.additional_directories.len() > 16 {
        return Err("最多添加 16 个额外扫描目录".into());
    }
    for path in &settings.additional_directories {
        if !path.is_absolute() || path.components().any(|c| matches!(c, Component::ParentDir)) {
            return Err("扫描目录必须是绝对路径，且不能包含 ..".into());
        }
        if path.to_string_lossy().len() > 4096 || path.to_string_lossy().contains('\0') {
            return Err("扫描目录路径无效".into());
        }
        if path.exists() && !path.is_dir() {
            return Err(format!("扫描路径不是目录：{}", path.display()));
        }
    }
    Ok(())
}

fn merge_settings(current: &Settings, patch: serde_json::Value) -> Result<Settings, String> {
    let patch = patch.as_object().ok_or("加载设置必须是对象")?;
    let mut value = serde_json::to_value(current).map_err(|e| e.to_string())?;
    value.as_object_mut().unwrap().extend(patch.clone());
    let settings: Settings =
        serde_json::from_value(value).map_err(|e| format!("加载设置无效：{e}"))?;
    validate_settings(&settings)?;
    Ok(settings)
}

fn save_preference(path: &Path, settings: &Settings, selected: &str) -> Result<(), String> {
    let parent = path.parent().ok_or("加载设置路径不可用")?;
    fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    let mut file = tempfile::NamedTempFile::new_in(parent).map_err(|e| e.to_string())?;
    serde_json::to_writer_pretty(
        &mut file,
        &Preference {
            selected: selected.into(),
            settings: settings.clone(),
        },
    )
    .map_err(|e| e.to_string())?;
    file.flush().map_err(|e| e.to_string())?;
    file.as_file().sync_all().map_err(|e| e.to_string())?;
    file.persist(path).map_err(|e| e.to_string())?;
    Ok(())
}

#[derive(Default)]
pub struct Registry {
    base_roots: Vec<Root>,
    roots: Vec<Root>,
    preference: PathBuf,
    settings: Settings,
    selected: String,
    pub active: Option<Entry>,
    pub requests: usize,
    pub switching: bool,
    warning: Option<String>,
}

pub struct WebUiState(pub Mutex<Registry>);

fn builtin() -> Entry {
    Entry {
        id: "builtin".into(),
        name: "内置 Neo UI".into(),
        version: env!("CARGO_PKG_VERSION").into(),
        description: "启动器自带的 Vue / Quasar 界面".into(),
        source: "builtin".into(),
        path: None,
        error: None,
        native_frame: false,
    }
}

fn scan(roots: &[Root]) -> Vec<Entry> {
    let mut entries = vec![builtin()];
    for root in roots {
        let Ok(canonical_root) = root.path.canonicalize() else {
            continue;
        };
        let Ok(children) = fs::read_dir(&root.path) else {
            continue;
        };
        for child in children.flatten() {
            if !child.path().is_dir() {
                continue;
            }
            let folder = child.file_name().to_string_lossy().into_owned();
            let mut entry = Entry {
                id: format!("{}/{}", root.source, folder),
                name: folder,
                version: String::new(),
                description: String::new(),
                source: root.source.clone(),
                path: Some(child.path()),
                error: None,
                native_frame: true,
            };
            let check = || -> Result<Manifest, String> {
                let path = child.path().canonicalize().map_err(|e| e.to_string())?;
                if !path.starts_with(&canonical_root) {
                    return Err("界面目录不能链接到 webui 目录之外".into());
                }
                safe_asset(&path, "/index.html")
                    .map_err(|_| "缺少可读取的 index.html".to_string())?;
                let manifest_path = path.join("webui.json");
                if !manifest_path.exists() {
                    return Ok(Manifest::default());
                }
                let manifest = safe_asset(&path, "/webui.json")?;
                if fs::metadata(&manifest).map_err(|e| e.to_string())?.len() > 65536 {
                    return Err("webui.json 超过 64 KiB".into());
                }
                let manifest: Manifest =
                    serde_json::from_slice(&fs::read(manifest).map_err(|e| e.to_string())?)
                        .map_err(|e| format!("webui.json 无效：{e}"))?;
                if manifest.api_version.unwrap_or(1) != 1 {
                    return Err("不支持这套界面的桥接 API 版本".into());
                }
                Ok(manifest)
            };
            match check() {
                Ok(m) => {
                    if let Some(name) = m.name.filter(|v| !v.trim().is_empty()) {
                        entry.name = name;
                    }
                    entry.version = m.version.unwrap_or_default();
                    entry.description = m.description.unwrap_or_default();
                    entry.native_frame = m.native_frame.unwrap_or(true);
                }
                Err(error) => entry.error = Some(error),
            }
            entries.push(entry);
        }
    }
    entries[1..].sort_by(|a, b| a.id.cmp(&b.id));
    entries
}

// Canonicalization also rejects symlinks/junctions that leave the selected UI.
fn safe_asset(root: &Path, uri: &str) -> Result<PathBuf, String> {
    let decoded = percent_encoding::percent_decode_str(uri)
        .decode_utf8()
        .map_err(|e| e.to_string())?;
    let relative = decoded.trim_start_matches('/');
    if relative.contains(['\\', ':', '\0']) {
        return Err("Invalid asset path".into());
    }
    let path = Path::new(if relative.is_empty() {
        "index.html"
    } else {
        relative
    });
    if path
        .components()
        .any(|c| !matches!(c, Component::Normal(_)))
    {
        return Err("Invalid asset path".into());
    }
    let root = root.canonicalize().map_err(|e| e.to_string())?;
    let file = root.join(path).canonicalize().map_err(|e| e.to_string())?;
    if !file.starts_with(&root) || !file.is_file() {
        return Err("Asset outside Web UI root".into());
    }
    Ok(file)
}

pub fn serve(
    app: &tauri::AppHandle,
    request: tauri::http::Request<Vec<u8>>,
) -> tauri::http::Response<Vec<u8>> {
    let result = (|| -> Result<(Vec<u8>, String), String> {
        if request.method() != "GET" && request.method() != "HEAD" {
            return Err("Method not allowed".into());
        }
        let root = app
            .state::<WebUiState>()
            .0
            .lock()
            .map_err(|e| e.to_string())?
            .active
            .as_ref()
            .and_then(|e| e.path.clone())
            .ok_or("No external Web UI selected")?;
        let path = safe_asset(&root, request.uri().path())?;
        let bytes = fs::read(&path).map_err(|e| e.to_string())?;
        let mime = tauri::utils::mime_type::MimeType::parse_with_fallback(
            &bytes,
            &path.to_string_lossy(),
            tauri::utils::mime_type::MimeType::OctetStream,
        );
        Ok((bytes, mime))
    })();
    let (code, body, mime) = match result {
        Ok((bytes, mime)) => (200, bytes, mime),
        Err(_) => (
            404,
            b"Web UI file unavailable. Use Ctrl+Shift+U to select the built-in UI.".to_vec(),
            "text/plain; charset=utf-8".into(),
        ),
    };
    tauri::http::Response::builder()
        .status(code)
        .header("Content-Type", mime)
        .header("Cache-Control", "no-store")
        .header("X-Content-Type-Options", "nosniff")
        .body(if request.method() == "HEAD" {
            Vec::new()
        } else {
            body
        })
        .unwrap()
}

#[derive(Serialize)]
pub struct Catalog {
    entries: Vec<Entry>,
    roots: Vec<Root>,
    active: String,
    warning: Option<String>,
    #[serde(rename = "settingsPath")]
    settings_path: PathBuf,
    #[serde(rename = "startupOverride")]
    startup_override: Option<String>,
}

#[tauri::command]
pub fn webui_list(state: State<'_, WebUiState>) -> Result<Catalog, String> {
    let s = state.0.lock().map_err(|e| e.to_string())?;
    Ok(Catalog {
        entries: scan(&s.roots),
        roots: s.roots.clone(),
        active: s
            .active
            .as_ref()
            .map(|e| e.id.clone())
            .unwrap_or_else(|| "builtin".into()),
        warning: s.warning.clone(),
        settings_path: s.preference.clone(),
        startup_override: std::env::var("LUNA_WEB_UI").ok().filter(|v| !v.is_empty()),
    })
}

#[tauri::command]
pub fn webui_settings_get(state: State<'_, WebUiState>) -> Result<Settings, String> {
    Ok(state.0.lock().map_err(|e| e.to_string())?.settings.clone())
}

#[tauri::command]
pub fn webui_settings_update(
    state: State<'_, WebUiState>,
    patch: serde_json::Value,
) -> Result<Settings, String> {
    let mut s = state.0.lock().map_err(|e| e.to_string())?;
    let settings = merge_settings(&s.settings, patch)?;
    let roots = roots_for(&s.base_roots, &settings);
    if !scan(&roots)
        .iter()
        .any(|entry| entry.id == settings.startup_ui && entry.error.is_none())
    {
        return Err("默认界面不在启用的扫描目录中；请先选择内置界面或启用对应目录。".into());
    }
    // Commit only after a successful atomic write. Changing discovery does not
    // navigate or invalidate the active bundle's resource root.
    save_preference(&s.preference, &settings, &s.selected)?;
    s.settings = settings.clone();
    s.roots = roots;
    s.warning = None;
    Ok(settings)
}

pub fn app_url(path: &str) -> tauri::Url {
    let base = if cfg!(any(windows, target_os = "android")) {
        "http://tauri.localhost/"
    } else {
        "tauri://localhost/"
    };
    tauri::Url::parse(base).unwrap().join(path).unwrap()
}

#[tauri::command]
pub fn webui_select(
    app: tauri::AppHandle,
    state: State<'_, WebUiState>,
    id: String,
) -> Result<(), String> {
    let mut s = state.0.lock().map_err(|e| e.to_string())?;
    if s.requests != 0 || s.switching {
        return Err("请等待当前请求、下载或登录完成后再切换界面。".into());
    }
    let entry = scan(&s.roots)
        .into_iter()
        .find(|e| e.id == id)
        .ok_or("未找到这套 Web UI，请刷新列表。")?;
    if let Some(error) = &entry.error {
        return Err(error.clone());
    }
    let window = app.get_webview_window("main").ok_or("主窗口不可用")?;
    let url = if entry.path.is_none() {
        app_url("index.html")
    } else {
        tauri::Url::parse(if cfg!(any(windows, target_os = "android")) {
            "http://webui.localhost/"
        } else {
            "webui://localhost/"
        })
        .unwrap()
    };
    // Navigation keeps the Tauri process and launcher sidecar alive. Protect
    // request/reply and login interactions from disappearing with the old page.
    window
        .set_decorations(entry.native_frame)
        .map_err(|e| e.to_string())?;
    let previous = s.active.replace(entry);
    s.switching = true;
    if let Err(error) = window.navigate(url) {
        s.active = previous;
        s.switching = false;
        return Err(error.to_string());
    }
    s.selected = id;
    if let Err(error) = save_preference(&s.preference, &s.settings, &s.selected) {
        s.warning = Some(format!("界面已切换，但无法保存选择：{error}"));
    }
    Ok(())
}

#[tauri::command]
pub async fn webui_initialize(app: tauri::AppHandle) -> Result<(), String> {
    // Ask the same backend used for all operations, so portable profiles and
    // LUNA_LAUNCHER_DATA resolve exactly as they do in the Qt launcher.
    let runtime = crate::launcher_request(
        app.clone(),
        app.state(),
        "launcher/execute".into(),
        serde_json::json!({"operation":"runtime.info","parameters":{}}),
    )
    .await
    .map_err(|error| error.message)?;
    let data_root = runtime
        .pointer("/data/dataRoot")
        .and_then(|v| v.as_str())
        .map(PathBuf::from);
    let selected = {
        let state = app.state::<WebUiState>();
        let mut s = state.0.lock().map_err(|e| e.to_string())?;
        if let Some(root) = data_root {
            let path = root.join("webui");
            if !s.base_roots.iter().any(|r| r.source == "data") {
                s.base_roots.push(Root {
                    source: "data".into(),
                    path: path.clone(),
                });
            }
            s.preference = path.join("selection.json");
        } else {
            s.warning = Some("未能读取启动器数据目录；目前只扫描安装目录。".into());
        }
        let saved = match fs::read(&s.preference) {
            Ok(bytes) => match serde_json::from_slice::<Preference>(&bytes) {
                Ok(saved) if validate_settings(&saved.settings).is_ok() => saved,
                _ => {
                    s.warning = Some("加载设置无效，已使用默认加载设置。".into());
                    Preference::default()
                }
            },
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => Preference::default(),
            Err(error) => {
                s.warning = Some(format!("无法读取加载设置：{error}"));
                Preference::default()
            }
        };
        s.settings = saved.settings;
        s.selected = saved.selected;
        s.roots = roots_for(&s.base_roots, &s.settings);
        let selected = std::env::var("LUNA_WEB_UI")
            .ok()
            .filter(|v| !v.is_empty())
            .unwrap_or_else(|| {
                if s.settings.remember_last_ui {
                    s.selected.clone()
                } else {
                    s.settings.startup_ui.clone()
                }
            });
        if scan(&s.roots)
            .iter()
            .any(|e| e.id == selected && e.error.is_none())
        {
            selected
        } else {
            s.warning = Some(format!("所选界面 {selected} 不可用，已恢复内置 Neo UI。"));
            "builtin".into()
        }
    };
    webui_select(app.clone(), app.state(), selected)?;
    let show_manager = app
        .state::<WebUiState>()
        .0
        .lock()
        .map_err(|e| e.to_string())?
        .settings
        .show_manager_on_start;
    if show_manager {
        webui_manager(app).await?;
    }
    Ok(())
}

#[tauri::command]
pub async fn webui_manager(app: tauri::AppHandle) -> Result<(), String> {
    if let Some(window) = app.get_webview_window("webui-manager") {
        window.show().map_err(|e| e.to_string())?;
        return window.set_focus().map_err(|e| e.to_string());
    }
    let mut builder = tauri::WebviewWindowBuilder::new(
        &app,
        "webui-manager",
        tauri::WebviewUrl::App("webui-manager.html".into()),
    )
    .title("Web UI 管理")
    .inner_size(680.0, 640.0)
    .min_inner_size(440.0, 360.0);
    if let Some(path) = &app.state::<crate::startup::Startup>().webview_data_dir {
        builder = builder.data_directory(path.clone());
    }
    builder.build().map_err(|e| e.to_string())?;
    Ok(())
}

#[tauri::command]
pub fn webui_open_folder(state: State<'_, WebUiState>, source: String) -> Result<(), String> {
    let path = state
        .0
        .lock()
        .map_err(|e| e.to_string())?
        .roots
        .iter()
        .find(|r| r.source == source)
        .map(|r| r.path.clone())
        .ok_or("目录不可用")?;
    fs::create_dir_all(&path).map_err(|e| e.to_string())?;
    tauri_plugin_opener::reveal_item_in_dir(path).map_err(|e| e.to_string())
}

pub fn setup(app: &mut tauri::App) -> Result<(), Box<dyn std::error::Error>> {
    let install = std::env::current_exe()?
        .parent()
        .ok_or("Missing executable directory")?
        .join("webui");
    let roots = vec![Root {
        source: "install".into(),
        path: install,
    }];
    *app.state::<WebUiState>().0.lock().unwrap() = Registry {
        roots: roots.clone(),
        base_roots: roots,
        selected: "builtin".into(),
        preference: app.path().app_config_dir()?.join("webui-selection.json"),
        ..Registry::default()
    };
    let manage = tauri::menu::MenuItem::with_id(
        app,
        "webui-manager",
        "Web UI 管理",
        true,
        Some("CmdOrCtrl+Shift+U"),
    )?;
    let submenu = tauri::menu::Submenu::with_items(app, "Web UI", true, &[&manage])?;
    let menu = tauri::menu::Menu::with_items(app, &[&submenu])?;
    app.set_menu(menu)?;
    #[cfg(not(target_os = "macos"))]
    if let Some(window) = app.get_webview_window("main") {
        window.hide_menu()?;
    }
    app.on_menu_event(|app, event| {
        if event.id().as_ref() == "webui-manager" {
            // WebView2 window creation must run outside the native event handler.
            let app = app.clone();
            tauri::async_runtime::spawn(async move {
                let _ = webui_manager(app).await;
            });
        }
    });
    Ok(())
}

pub struct RequestLease(tauri::AppHandle);
impl RequestLease {
    pub fn acquire(app: &tauri::AppHandle) -> Result<Self, String> {
        let state = app.state::<WebUiState>();
        let mut s = state.0.lock().map_err(|e| e.to_string())?;
        if s.switching {
            return Err("正在切换 Web UI，请稍后重试。".into());
        }
        s.requests += 1;
        Ok(Self(app.clone()))
    }
}
impl Drop for RequestLease {
    fn drop(&mut self) {
        if let Ok(mut s) = self.0.state::<WebUiState>().0.lock() {
            s.requests -= 1;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn settings_roundtrip_and_legacy_selection() {
        let legacy: Preference = serde_json::from_str(r#"{"selected":"data/old"}"#).unwrap();
        assert_eq!(legacy.selected, "data/old");
        assert!(legacy.settings.remember_last_ui);
        let dir = tempfile::tempdir().unwrap();
        let settings = merge_settings(&Settings::default(), serde_json::json!({
            "rememberLastUi": false, "startupUi": "data/example", "additionalDirectories": [dir.path()]
        })).unwrap();
        let preference = dir.path().join("config/selection.json");
        save_preference(&preference, &settings, "builtin").unwrap();
        save_preference(&preference, &settings, "data/other").unwrap();
        let saved: Preference = serde_json::from_slice(&fs::read(&preference).unwrap()).unwrap();
        assert_eq!(saved.selected, "data/other");
        assert_eq!(saved.settings, settings);
        for patch in [
            serde_json::json!({"typo":true}),
            serde_json::json!({"rememberLastUi":"false"}),
            serde_json::json!({"additionalDirectories":["relative/path"]}),
            serde_json::json!({"startupUi":""}),
        ] {
            assert!(merge_settings(&settings, patch).is_err());
        }
    }
    #[test]
    fn configured_roots_keep_stable_ids_when_reordered() {
        let dir = tempfile::tempdir().unwrap();
        let a = dir.path().join("A");
        let b = dir.path().join("B");
        let base = vec![Root {
            source: "install".into(),
            path: a.clone(),
        }];
        let mut settings = Settings {
            scan_install_directory: false,
            additional_directories: vec![a, b],
            ..Settings::default()
        };
        let first = roots_for(&base, &settings);
        settings.additional_directories.reverse();
        let second = roots_for(&base, &settings);
        assert_eq!(first[0].source, second[1].source);
        assert_eq!(first[1].source, second[0].source);
        settings.scan_install_directory = true;
        assert_eq!(roots_for(&base, &settings).len(), 2);
    }
    #[test]
    fn discovers_both_locations_and_reports_broken_bundles() {
        let dir = tempfile::tempdir().unwrap();
        let roots = ["install", "data"].map(|source| Root {
            source: source.into(),
            path: dir.path().join(source),
        });
        for root in &roots {
            fs::create_dir_all(root.path.join("same-name")).unwrap();
            fs::write(root.path.join("same-name/index.html"), "<h1>Hello</h1>").unwrap();
        }
        fs::write(
            roots[1].path.join("same-name/webui.json"),
            r#"{"name":"测试界面","apiVersion":1,"nativeFrame":false}"#,
        )
        .unwrap();
        fs::create_dir(roots[1].path.join("broken")).unwrap();
        fs::create_dir(roots[1].path.join("bad-manifest")).unwrap();
        fs::write(roots[1].path.join("bad-manifest/index.html"), "hello").unwrap();
        fs::write(roots[1].path.join("bad-manifest/webui.json"), "not json").unwrap();
        let entries = scan(&roots);
        assert_eq!(entries.len(), 5);
        assert_eq!(entries[0].id, "builtin");
        assert!(entries
            .iter()
            .any(|e| e.id == "install/same-name" && e.error.is_none()));
        assert!(entries
            .iter()
            .any(|e| e.id == "data/same-name" && e.name == "测试界面" && !e.native_frame));
        assert_eq!(entries.iter().filter(|e| e.error.is_some()).count(), 2);
    }
    #[test]
    fn serves_relative_assets_and_rejects_escape_paths() {
        let dir = tempfile::tempdir().unwrap();
        let root = dir.path().join("ui");
        fs::create_dir_all(root.join("assets")).unwrap();
        fs::write(root.join("index.html"), "index").unwrap();
        fs::write(root.join("assets/图标.svg"), "svg").unwrap();
        fs::write(dir.path().join("secret"), "outside").unwrap();
        assert_eq!(
            fs::read_to_string(safe_asset(&root, "/").unwrap()).unwrap(),
            "index"
        );
        assert!(safe_asset(&root, "/assets/%E5%9B%BE%E6%A0%87.svg").is_ok());
        for path in [
            "/../secret",
            "/%2e%2e/secret",
            "/assets/../../secret",
            "/C:/secret",
            "/..%5csecret",
            "/%00",
            "/missing.js",
        ] {
            assert!(safe_asset(&root, path).is_err(), "{path}");
        }
    }
}
