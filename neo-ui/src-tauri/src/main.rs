#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde::Serialize;
use serde_json::{json, Value};
use std::{
    collections::HashMap,
    io::{BufRead, BufReader, Write},
    path::PathBuf,
    process::{Child, ChildStdin, Command, Stdio},
    sync::{mpsc, Arc, Mutex},
    thread,
};
use tauri::{Emitter, Manager, State};

struct Sidecar {
    child: Child,
    input: Mutex<ChildStdin>,
    pending: Arc<Mutex<HashMap<u64, mpsc::Sender<Value>>>>,
    next_id: Mutex<u64>,
}

struct AppState(Mutex<Option<Sidecar>>);

#[derive(Serialize)]
struct ApiError {
    message: String,
}

fn sidecar_path() -> Result<PathBuf, String> {
    if let Ok(path) = std::env::var("LUNA_LAUNCHER_CLI") {
        return Ok(PathBuf::from(path));
    }
    let current = std::env::current_exe().map_err(|e| e.to_string())?;
    let name = if cfg!(windows) {
        "lunalauncher-cli.exe"
    } else {
        "lunalauncher-cli"
    };
    Ok(current
        .parent()
        .unwrap_or_else(|| std::path::Path::new("."))
        .join(name))
}

fn start_sidecar(app: &tauri::AppHandle) -> Result<Sidecar, String> {
    let executable = sidecar_path()?;
    let data_dir = std::env::var("LUNA_LAUNCHER_DATA")
        .map(PathBuf::from)
        .unwrap_or_else(|_| {
            executable
                .parent()
                .unwrap_or_else(|| std::path::Path::new("."))
                .join("data")
        });
    let mut command = Command::new(executable);
    command
        .args(["--dir", data_dir.to_string_lossy().as_ref(), "--mcp"])
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::null());
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
    let pending: Arc<Mutex<HashMap<u64, mpsc::Sender<Value>>>> =
        Arc::new(Mutex::new(HashMap::new()));
    let reader_pending = Arc::clone(&pending);
    let reader_app = app.clone();
    thread::spawn(move || {
        for line in BufReader::new(output).lines().flatten() {
            let Ok(message) = serde_json::from_str::<Value>(&line) else {
                continue;
            };
            if let Some(id) = message.get("id").and_then(Value::as_u64) {
                if let Some(sender) = reader_pending.lock().ok().and_then(|mut p| p.remove(&id)) {
                    let _ = sender.send(message);
                }
            } else if message.get("method").and_then(Value::as_str) == Some("launcher/event") {
                let _ = reader_app.emit(
                    "launcher-event",
                    message.get("params").cloned().unwrap_or(Value::Null),
                );
            }
        }
        let _ = reader_app.emit("launcher-exit", json!({}));
    });
    Ok(Sidecar {
        child,
        input: Mutex::new(input),
        pending,
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
    if guard.is_none() {
        *guard = Some(start_sidecar(app)?);
    }
    Ok(guard)
}

#[tauri::command]
fn launcher_execute(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    operation: String,
    parameters: Value,
) -> Result<Value, ApiError> {
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
    sidecar
        .pending
        .lock()
        .map_err(|_| ApiError {
            message: "Launcher response map is unavailable".into(),
        })?
        .insert(request_id, sender);
    let request = json!({ "jsonrpc": "2.0", "id": request_id, "method": "launcher/execute", "params": { "operation": operation, "parameters": parameters } });
    let mut input = sidecar.input.lock().map_err(|_| ApiError {
        message: "Launcher stdin is unavailable".into(),
    })?;
    writeln!(input, "{}", request).map_err(|e| ApiError {
        message: format!("Could not write launcher request: {e}"),
    })?;
    input.flush().map_err(|e| ApiError {
        message: format!("Could not flush launcher request: {e}"),
    })?;
    drop(input);
    receiver.recv().map_err(|_| ApiError {
        message: "Launcher sidecar exited before returning a response".into(),
    })
}

fn main() {
    tauri::Builder::default()
        .manage(AppState(Mutex::new(None)))
        .invoke_handler(tauri::generate_handler![launcher_execute])
        .build(tauri::generate_context!())
        .expect("error while building Neo UI")
        .run(|app, event| {
            if let tauri::RunEvent::Exit = event {
                if let Some(state) = app.try_state::<AppState>() {
                    if let Ok(mut guard) = state.0.lock() {
                        if let Some(mut sidecar) = guard.take() {
                            let _ = sidecar.child.kill();
                        }
                    }
                }
            }
        });
}
