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
    let mut command = Command::new(executable);
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        command.creation_flags(0x08000000); // CREATE_NO_WINDOW
    }
    // Share the launcher's native portable/environment/default directory rules.
    // Only isolate data when the caller explicitly requests it.
    if let Some(data_dir) = std::env::var_os("LUNA_LAUNCHER_DATA").filter(|p| !p.is_empty()) {
        command.arg("--dir").arg(data_dir);
    }
    command
        .arg("--mcp")
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
        for line in BufReader::new(output).lines().map_while(Result::ok) {
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
            } else if message.get("method").and_then(Value::as_str) == Some("launcher/stream") {
                let _ = reader_app.emit(
                    "launcher-stream",
                    message.get("params").cloned().unwrap_or(Value::Null),
                );
            }
        }
        if let Ok(mut pending) = reader_pending.lock() {
            pending.clear(); // Disconnect receivers when stdout closes.
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
        sidecar
            .pending
            .lock()
            .map_err(|_| ApiError {
                message: "Launcher response map is unavailable".into(),
            })?
            .insert(request_id, sender);
        let request =
            json!({ "jsonrpc": "2.0", "id": request_id, "method": method, "params": parameters });
        let mut input = sidecar.input.lock().map_err(|_| ApiError {
            message: "Launcher stdin is unavailable".into(),
        })?;
        if let Err(error) = writeln!(input, "{}", request).and_then(|_| input.flush()) {
            if let Ok(mut pending) = sidecar.pending.lock() {
                pending.remove(&request_id);
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
        })?;
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

fn main() {
    tauri::Builder::default()
        .manage(AppState(Mutex::new(None)))
        .invoke_handler(tauri::generate_handler![launcher_request])
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
