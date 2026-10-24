// SPDX-License-Identifier: MPL-2.0
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde::Serialize;
use serde_json::{json, Value};
use std::{
    collections::{HashMap, VecDeque},
    io::{BufRead, BufReader, Write},
    process::{Child, ChildStdin, Stdio},
    sync::{mpsc, Arc, Mutex},
    thread,
};
use tauri::{Emitter, Manager, State};
mod startup;
mod webui;

#[derive(Default)]
struct Connection {
    pending: HashMap<u64, mpsc::Sender<Result<Value, String>>>,
    closed: Option<String>,
}

struct Sidecar {
    child: Child,
    input: Mutex<ChildStdin>,
    connection: Arc<Mutex<Connection>>,
    next_id: Mutex<u64>,
}

struct AppState(Mutex<Option<Sidecar>>);

#[derive(Serialize)]
struct ApiError {
    message: String,
}

fn start_sidecar(app: &tauri::AppHandle) -> Result<Sidecar, String> {
    let startup = app.state::<startup::Startup>();
    let context = startup.launcher.as_ref().map_err(Clone::clone)?;
    let mut command = startup::command()?;
    command.arg("--dir").arg(&context.data_root);
    command
        .arg("--mcp")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    let mut child = command
        .spawn()
        .map_err(|e| format!("Could not start launcher sidecar: {e}"))?;
    let input = child
        .stdin
        .take()
        .ok_or_else(|| "Launcher sidecar stdin is unavailable".to_string())?;
    let output = child
        .stdout
        .take()
        .ok_or_else(|| "Launcher sidecar stdout is unavailable".to_string())?;
    let errors = child
        .stderr
        .take()
        .ok_or("Launcher sidecar stderr is unavailable")?;
    let diagnostics = Arc::new(Mutex::new(VecDeque::new()));
    let error_tail = Arc::clone(&diagnostics);
    let (stderr_done, stderr_closed) = mpsc::channel();
    thread::spawn(move || {
        for line in BufReader::new(errors).lines().map_while(Result::ok) {
            if let Ok(mut tail) = error_tail.lock() {
                tail.push_back(line.chars().take(1024).collect::<String>());
                if tail.len() > 8 {
                    tail.pop_front();
                }
            }
        }
        let _ = stderr_done.send(());
    });
    let connection = Arc::new(Mutex::new(Connection::default()));
    let reader_connection = Arc::clone(&connection);
    let reader_app = app.clone();
    let pid = child.id();
    thread::spawn(move || {
        for line in BufReader::new(output).lines().map_while(Result::ok) {
            let Ok(message) = serde_json::from_str::<Value>(&line) else {
                continue;
            };
            if let Some(id) = message.get("id").and_then(Value::as_u64) {
                if let Some(sender) = reader_connection
                    .lock()
                    .ok()
                    .and_then(|mut c| c.pending.remove(&id))
                {
                    let _ = sender.send(Ok(message));
                }
            } else if message.get("method").and_then(Value::as_str) == Some("launcher/event") {
                let _ = reader_app.emit(
                    "launcher-event",
                    message.get("params").cloned().unwrap_or(Value::Null),
                );
            } else if message.get("method").and_then(Value::as_str) == Some("launcher/stream") {
                let _ = reader_app.emit(
                    "launcher-stream",
                    message.get("params").cloned().unwrap_or(Value::Null),
                );
            }
        }
        // Usually both pipes close together. Never leave requests waiting for
        // stderr EOF if a descendant inherited that handle from the backend.
        let _ = stderr_closed.recv_timeout(std::time::Duration::from_millis(200));
        let details = diagnostics
            .lock()
            .map(|tail| tail.iter().cloned().collect::<Vec<_>>().join("\n"))
            .unwrap_or_default();
        let error = if details.contains("data directory is already in use") {
            "启动器数据目录已被另一个进程占用。请使用已打开的 Neo UI；如果运行的是传统 Qt 界面，请先关闭它再打开 Neo UI。".to_string()
        } else if details.is_empty() {
            "启动器后端已退出，未返回响应。".to_string()
        } else {
            format!("启动器后端已退出：\n{details}")
        };
        if let Ok(mut connection) = reader_connection.lock() {
            connection.closed = Some(error.clone());
            for (_, sender) in connection.pending.drain() {
                let _ = sender.send(Err(error.clone()));
            }
        }
        if let Ok(state) = reader_app.state::<AppState>().0.lock() {
            if state
                .as_ref()
                .is_some_and(|sidecar| sidecar.child.id() == pid)
            {
                let _ = reader_app.emit("launcher-exit", json!({ "message": error }));
            }
        }
    });
    Ok(Sidecar {
        child,
        input: Mutex::new(input),
        connection,
        next_id: Mutex::new(1),
    })
}

fn ensure_sidecar<'a>(
    app: &tauri::AppHandle,
    state: &'a State<'_, AppState>,
) -> Result<std::sync::MutexGuard<'a, Option<Sidecar>>, String> {
    let mut guard = state
        .0
        .lock()
        .map_err(|_| "Launcher state lock is poisoned".to_string())?;
    if let Some(sidecar) = guard.as_mut() {
        if sidecar
            .child
            .try_wait()
            .map_err(|e| e.to_string())?
            .is_some()
        {
            *guard = None;
        }
    }
    if guard.is_none() {
        *guard = Some(start_sidecar(app)?);
    }
    Ok(guard)
}

#[tauri::command]
async fn launcher_request(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    method: String,
    parameters: Value,
) -> Result<Value, ApiError> {
    let _lease = webui::RequestLease::acquire(&app).map_err(|message| ApiError { message })?;
    if !["launcher/execute", "launcher/respond", "launcher/catalog"].contains(&method.as_str()) {
        return Err(ApiError {
            message: "Unsupported protocol method".into(),
        });
    }
    let receiver = {
        let guard = ensure_sidecar(&app, &state).map_err(|message| ApiError { message })?;
        let sidecar = guard.as_ref().ok_or_else(|| ApiError {
            message: "Launcher sidecar is unavailable".into(),
        })?;
        let mut id = sidecar.next_id.lock().map_err(|_| ApiError {
            message: "Launcher request counter is unavailable".into(),
        })?;
        let request_id = *id;
        *id += 1;
        let (sender, receiver) = mpsc::channel();
        let mut connection = sidecar.connection.lock().map_err(|_| ApiError {
            message: "Launcher response map is unavailable".into(),
        })?;
        if let Some(message) = &connection.closed {
            return Err(ApiError {
                message: message.clone(),
            });
        }
        connection.pending.insert(request_id, sender);
        drop(connection);
        let request =
            json!({ "jsonrpc": "2.0", "id": request_id, "method": method, "params": parameters });
        let mut input = sidecar.input.lock().map_err(|_| ApiError {
            message: "Launcher stdin is unavailable".into(),
        })?;
        if let Err(error) = writeln!(input, "{}", request).and_then(|_| input.flush()) {
            if let Ok(mut connection) = sidecar.connection.lock() {
                connection.pending.remove(&request_id);
            }
            return Err(ApiError {
                message: format!("Could not write launcher request: {error}"),
            });
        }
        drop(input);
        receiver
    }; // Release state and stdin locks before waiting for a response.
    let message = tauri::async_runtime::spawn_blocking(move || receiver.recv())
        .await
        .map_err(|e| ApiError {
            message: e.to_string(),
        })?
        .map_err(|_| ApiError {
            message: "Launcher sidecar exited before returning a response".into(),
        })?
        .map_err(|message| ApiError { message })?;
    if let Some(error) = message.get("error") {
        return Err(ApiError {
            message: error
                .get("message")
                .and_then(Value::as_str)
                .unwrap_or("Launcher protocol error")
                .to_string(),
        });
    }
    message.get("result").cloned().ok_or_else(|| ApiError {
        message: "Launcher response has no result".into(),
    })
}

fn percent_encode(value: &str) -> String {
    let mut out = String::new();
    for byte in value.as_bytes() {
        let ch = *byte as char;
        if ch.is_ascii_alphanumeric() || matches!(ch, '-' | '_' | '.' | '~') {
            out.push(ch);
        } else {
            out.push_str(&format!("%{byte:02X}"));
        }
    }
    out
}

#[tauri::command]
async fn open_instance_editor(app: tauri::AppHandle, id: String, name: String) -> Result<(), String> {
    use std::hash::{Hash, Hasher};
    let mut hasher = std::collections::hash_map::DefaultHasher::new();
    id.hash(&mut hasher);
    let label = format!("instance-editor-{:x}", hasher.finish());
    if let Some(window) = app.get_webview_window(&label) {
        window.show().map_err(|e| e.to_string())?;
        return window.set_focus().map_err(|e| e.to_string());
    }
    let path = format!("index.html?instanceEditor={}", percent_encode(&id));
    let mut builder = tauri::WebviewWindowBuilder::new(
        &app,
        label,
        tauri::WebviewUrl::External(webui::app_url(&path)),
    )
    .title(format!("实例设置 - {name}"))
    .inner_size(980.0, 760.0)
    .min_inner_size(720.0, 480.0);
    if let Some(path) = &app.state::<startup::Startup>().webview_data_dir {
        builder = builder.data_directory(path.clone());
    }
    builder.build().map_err(|e| e.to_string())?;
    Ok(())
}

fn main() {
    let mut context = tauri::generate_context!();
    let startup = startup::prepare(&mut context);
    tauri::Builder::default()
        .plugin(tauri_plugin_single_instance::init(|app, _, _| {
            startup::activate(app)
        }))
        .manage(startup)
        .manage(AppState(Mutex::new(None)))
        .manage(webui::WebUiState(Mutex::new(webui::Registry::default())))
        .plugin(tauri_plugin_opener::init())
        .plugin(
            tauri::plugin::Builder::<_, ()>::new("webui")
                .js_init_script(include_str!("../../public/webui-bridge.js").to_string())
                .build(),
        )
        .register_uri_scheme_protocol("webui", |context, request| {
            webui::serve(context.app_handle(), request)
        })
        .setup(webui::setup)
        .on_page_load(|webview, payload| {
            if webview.label() == "main"
                && matches!(payload.event(), tauri::webview::PageLoadEvent::Started)
            {
                // New documents call the bridge while scripts are loading,
                // before PageLoadEvent::Finished (including external modules).
                if let Ok(mut state) = webview.state::<webui::WebUiState>().0.lock() {
                    state.switching = false;
                }
            }
        })
        .on_window_event(|window, event| {
            if window.label() == "main" && matches!(event, tauri::WindowEvent::Destroyed) {
                // The recovery window must not keep a closed launcher alive.
                if let Some(manager) = window.get_webview_window("webui-manager") {
                    let _ = manager.close();
                }
            }
        })
        .invoke_handler(tauri::generate_handler![
            launcher_request,
            open_instance_editor,
            webui::webui_list,
            webui::webui_settings_get,
            webui::webui_settings_update,
            webui::webui_select,
            webui::webui_initialize,
            webui::webui_manager,
            webui::webui_open_folder
        ])
        .build(context)
        .expect("error while building Neo UI")
        .run(|app, event| {
            if let tauri::RunEvent::Exit = event {
                if let Some(state) = app.try_state::<AppState>() {
                    if let Ok(mut guard) = state.0.lock() {
                        if let Some(mut sidecar) = guard.take() {
                            let _ = sidecar.child.kill();
                            let _ = sidecar.child.wait();
                        }
                    }
                }
            }
        });
}
