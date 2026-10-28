use serde::{Deserialize, Serialize};
use std::{
    collections::VecDeque,
    env,
    ffi::OsStr,
    fs,
    io::{self, BufRead, Write},
    path::{Path, PathBuf},
    sync::{Arc, Mutex},
    thread,
    time::{Duration, Instant},
};
use tauri::{
    webview::DownloadEvent,
    window::{ProgressBarState, ProgressBarStatus},
    AppHandle, Manager, PhysicalPosition, PhysicalSize, RunEvent, Webview, WebviewBuilder,
    WebviewUrl, Window, WindowBuilder, WindowEvent, Wry,
};
use url::Url;

const TOOLBAR_HEIGHT: f64 = 76.0;
const RETRY_EXIT_CODE: i32 = 3;
const STALLED_AFTER: Duration = Duration::from_secs(15);

#[derive(Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
struct DownloadItem {
    url: String,
    file_name: String,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct DownloadRequest {
    download_directory: PathBuf,
    items: Vec<DownloadItem>,
    #[serde(default = "default_start_index")]
    start_index: usize,
    total_items: Option<usize>,
    #[serde(default)]
    proxy: ProxySettings,
}

fn default_start_index() -> usize {
    1
}

#[derive(Clone, Deserialize)]
struct ProxySettings {
    #[serde(rename = "type")]
    proxy_type: String,
    #[serde(default)]
    host: String,
    #[serde(default)]
    port: u16,
}

impl Default for ProxySettings {
    fn default() -> Self {
        Self {
            proxy_type: "Default".into(),
            host: String::new(),
            port: 0,
        }
    }
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct HelperEvent<'a> {
    event: &'a str,
    file_name: Option<&'a str>,
    message: Option<&'a str>,
    file_index: Option<usize>,
    file_count: Option<usize>,
    bytes_received: Option<u64>,
    bytes_per_second: Option<u64>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct ToolbarState<'a> {
    status: &'a str,
    file_name: &'a str,
    file_index: usize,
    file_count: usize,
    bytes_received: u64,
    bytes_per_second: u64,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct ControlCommand {
    command: String,
    file_name: String,
}

struct QueueState {
    current: DownloadItem,
    remaining: VecDeque<DownloadItem>,
    download_in_progress: bool,
    retry_requested: bool,
    ignored_finished_events: usize,
    current_index: usize,
    total_items: usize,
}

fn emit_event(event: HelperEvent<'_>) {
    if let Ok(line) = serde_json::to_string(&event) {
        println!("{line}");
        let _ = io::stdout().flush();
    }
}

fn format_bytes(bytes: u64) -> String {
    const UNITS: [&str; 5] = ["B", "KiB", "MiB", "GiB", "TiB"];
    let mut value = bytes as f64;
    let mut unit = 0;
    while value >= 1024.0 && unit < UNITS.len() - 1 {
        value /= 1024.0;
        unit += 1;
    }
    if unit == 0 {
        format!("{bytes} {}", UNITS[unit])
    } else {
        format!("{value:.1} {}", UNITS[unit])
    }
}

fn update_toolbar(toolbar: &Webview, state: ToolbarState<'_>) {
    if let Ok(payload) = serde_json::to_string(&state) {
        let _ = toolbar.eval(format!(
            "window.setDownloadState && window.setDownloadState({payload});"
        ));
    }
}

fn monitor_download(
    window: Window,
    toolbar: Webview,
    queue: Arc<Mutex<QueueState>>,
    path: PathBuf,
    file_name: String,
    file_index: usize,
    file_count: usize,
) {
    thread::spawn(move || {
        let mut samples = VecDeque::from([(Instant::now(), 0_u64)]);
        let mut last_growth = Instant::now();
        let mut previous_bytes = 0_u64;
        let mut last_reported = (u64::MAX, u64::MAX, false);
        loop {
            thread::sleep(Duration::from_millis(250));
            let still_downloading = {
                let state = queue.lock().expect("download queue mutex poisoned");
                state.download_in_progress
                    && !state.retry_requested
                    && state.current_index == file_index
                    && state.current.file_name == file_name
            };
            if !still_downloading {
                break;
            }

            let bytes_received = fs::metadata(&path)
                .map(|metadata| metadata.len())
                .unwrap_or(0);
            let now = Instant::now();
            if bytes_received > previous_bytes {
                last_growth = now;
                previous_bytes = bytes_received;
            }
            samples.push_back((now, bytes_received));
            while samples.len() > 2
                && now.duration_since(samples.front().expect("speed sample missing").0)
                    > Duration::from_secs(2)
            {
                samples.pop_front();
            }
            let (sample_time, sample_bytes) = samples.front().expect("speed sample missing");
            let elapsed = now.duration_since(*sample_time).as_secs_f64();
            let bytes_per_second = if elapsed > 0.0 {
                (bytes_received.saturating_sub(*sample_bytes) as f64 / elapsed) as u64
            } else {
                0
            };
            let stalled = now.duration_since(last_growth) >= STALLED_AFTER;
            if last_reported == (bytes_received, bytes_per_second, stalled) {
                continue;
            }
            last_reported = (bytes_received, bytes_per_second, stalled);

            let title = format!(
                "Downloading {file_index}/{file_count}: {file_name} - {} - {}/s",
                format_bytes(bytes_received),
                format_bytes(bytes_per_second)
            );
            let _ = window.set_title(&title);
            update_toolbar(
                &toolbar,
                ToolbarState {
                    status: if stalled { "stalled" } else { "downloading" },
                    file_name: &file_name,
                    file_index,
                    file_count,
                    bytes_received,
                    bytes_per_second,
                },
            );
            emit_event(HelperEvent {
                event: "downloadProgress",
                file_name: Some(&file_name),
                message: if stalled {
                    Some("No download activity has been detected for 15 seconds")
                } else {
                    None
                },
                file_index: Some(file_index),
                file_count: Some(file_count),
                bytes_received: Some(bytes_received),
                bytes_per_second: Some(bytes_per_second),
            });
        }
    });
}

fn is_curseforge_page(url: &Url) -> bool {
    url.scheme() == "https"
        && matches!(
            url.host_str(),
            Some("curseforge.com" | "www.curseforge.com")
        )
        && url.path().contains("/download/")
}

fn validate_file_name(file_name: &str) -> bool {
    !file_name.is_empty()
        && Path::new(file_name)
            .file_name()
            .is_some_and(|leaf| leaf == file_name)
}

fn proxy_url(proxy: &ProxySettings) -> Result<Option<Url>, String> {
    let scheme = match proxy.proxy_type.as_str() {
        "Default" | "None" => return Ok(None),
        "HTTP" => "http",
        "SOCKS5" => "socks5",
        other => return Err(format!("unsupported proxy type: {other}")),
    };
    if proxy.host.trim().is_empty() || proxy.port == 0 {
        return Err("the proxy host and port must be set".into());
    }

    let mut url = Url::parse(&format!("{scheme}://localhost")).expect("static proxy URL is valid");
    url.set_host(Some(proxy.host.trim()))
        .map_err(|_| "the proxy host is invalid")?;
    url.set_port(Some(proxy.port))
        .map_err(|_| "the proxy port is invalid")?;
    Ok(Some(url))
}

fn configure_webview_builder(
    builder: WebviewBuilder<Wry>,
    proxy: &ProxySettings,
) -> WebviewBuilder<Wry> {
    if proxy.proxy_type == "None" {
        #[cfg(windows)]
        return builder.additional_browser_args(
            "--disable-features=msWebOOUI,msPdfOOUI,msSmartScreenProtection --no-proxy-server",
        );
    }
    match proxy_url(proxy).expect("proxy settings were validated") {
        Some(url) => builder.proxy_url(url),
        None => builder,
    }
}

fn parse_request() -> Result<DownloadRequest, String> {
    let mut args = env::args_os().skip(1);
    if args.next().as_deref() != Some(OsStr::new("--request")) {
        return Err("expected --request <path>".into());
    }
    let request_path = args.next().ok_or("missing request path")?;
    if args.next().is_some() {
        return Err("unexpected command-line arguments".into());
    }

    let data = fs::read(request_path).map_err(|error| error.to_string())?;
    let request: DownloadRequest =
        serde_json::from_slice(&data).map_err(|error| error.to_string())?;
    if request.items.is_empty() {
        return Err("the download queue is empty".into());
    }
    if !request.download_directory.is_absolute() {
        return Err("the download directory must be absolute".into());
    }
    fs::create_dir_all(&request.download_directory).map_err(|error| error.to_string())?;

    let total_items = request.total_items.unwrap_or(request.items.len());
    if request.start_index == 0 || request.start_index + request.items.len() - 1 != total_items {
        return Err("the download queue range is invalid".into());
    }
    proxy_url(&request.proxy)?;

    for item in &request.items {
        let url = Url::parse(&item.url).map_err(|error| error.to_string())?;
        if !is_curseforge_page(&url) {
            return Err(format!("unsupported CurseForge URL: {}", item.url));
        }
        if !validate_file_name(&item.file_name) {
            return Err(format!("invalid output file name: {}", item.file_name));
        }
    }

    Ok(request)
}

fn fail(app: &AppHandle, message: &str) {
    emit_event(HelperEvent {
        event: "error",
        file_name: None,
        message: Some(message),
        file_index: None,
        file_count: None,
        bytes_received: None,
        bytes_per_second: None,
    });
    app.exit(2);
}

fn request_retry(app: &AppHandle, queue: &Arc<Mutex<QueueState>>) {
    let mut state = queue.lock().expect("download queue mutex poisoned");
    if state.retry_requested {
        return;
    }
    state.retry_requested = true;
    emit_event(HelperEvent {
        event: "retryRequested",
        file_name: Some(&state.current.file_name),
        message: None,
        file_index: Some(state.current_index),
        file_count: Some(state.total_items),
        bytes_received: None,
        bytes_per_second: None,
    });
    drop(state);
    app.exit(RETRY_EXIT_CODE);
}

fn finish_current(webview: &Webview, toolbar: &Webview, queue: &Arc<Mutex<QueueState>>) {
    let (next, completed_name, completed_index, total_items) = {
        let mut state = queue.lock().expect("download queue mutex poisoned");
        if state.retry_requested {
            return;
        }
        state.download_in_progress = false;
        let completed_name = state.current.file_name.clone();
        let completed_index = state.current_index;
        let total_items = state.total_items;
        let next = state.remaining.pop_front();
        if next.is_some() {
            state.current_index += 1;
        }
        (next, completed_name, completed_index, total_items)
    };

    emit_event(HelperEvent {
        event: "downloadFinished",
        file_name: Some(&completed_name),
        message: None,
        file_index: Some(completed_index),
        file_count: Some(total_items),
        bytes_received: None,
        bytes_per_second: None,
    });
    let completed_percent = (completed_index as u64 * 100) / total_items as u64;
    let _ = webview.window().set_progress_bar(ProgressBarState {
        status: Some(ProgressBarStatus::Normal),
        progress: Some(completed_percent),
    });

    if let Some(item) = next {
        let next_url = match Url::parse(&item.url) {
            Ok(url) => url,
            Err(error) => {
                fail(webview.app_handle(), &error.to_string());
                return;
            }
        };
        let mut state = queue.lock().expect("download queue mutex poisoned");
        state.current = item;
        let file_name = state.current.file_name.clone();
        let file_index = state.current_index;
        drop(state);
        update_toolbar(
            toolbar,
            ToolbarState {
                status: "waiting",
                file_name: &file_name,
                file_index,
                file_count: total_items,
                bytes_received: 0,
                bytes_per_second: 0,
            },
        );
        if let Err(error) = webview.navigate(next_url) {
            fail(webview.app_handle(), &error.to_string());
        }
    } else {
        update_toolbar(
            toolbar,
            ToolbarState {
                status: "complete",
                file_name: "Downloads complete",
                file_index: total_items,
                file_count: total_items,
                bytes_received: 0,
                bytes_per_second: 0,
            },
        );
        let _ = webview.window().set_title("CurseForge downloads complete");
        let _ = webview.window().set_progress_bar(ProgressBarState {
            status: Some(ProgressBarStatus::None),
            progress: None,
        });
        webview.app_handle().exit(0);
    }
}

fn listen_for_commands(app: AppHandle, queue: Arc<Mutex<QueueState>>) {
    thread::spawn(move || {
        for line in io::stdin().lock().lines().map_while(Result::ok) {
            let Ok(command) = serde_json::from_str::<ControlCommand>(&line) else {
                continue;
            };
            if command.command != "acceptCurrent" {
                continue;
            }

            let should_advance = {
                let mut state = queue.lock().expect("download queue mutex poisoned");
                if state.retry_requested || state.current.file_name != command.file_name {
                    false
                } else {
                    if state.download_in_progress {
                        state.ignored_finished_events += 1;
                    }
                    true
                }
            };
            if !should_advance {
                continue;
            }

            let Some(content) = app.get_webview("curseforge-content") else {
                continue;
            };
            let Some(toolbar) = app.get_webview("download-toolbar") else {
                continue;
            };
            finish_current(&content, &toolbar, &queue);
        }
    });
}

fn handle_download(
    webview: Webview,
    toolbar: &Webview,
    event: DownloadEvent<'_>,
    queue: &Arc<Mutex<QueueState>>,
    download_directory: &Path,
) -> bool {
    match event {
        DownloadEvent::Requested {
            url: _,
            destination,
        } => {
            let mut state = queue.lock().expect("download queue mutex poisoned");
            if state.download_in_progress || state.retry_requested {
                return false;
            }

            let download_path = download_directory.join(&state.current.file_name);
            *destination = download_path.clone();
            state.download_in_progress = true;
            let file_name = state.current.file_name.clone();
            let file_index = state.current_index;
            let file_count = state.total_items;
            emit_event(HelperEvent {
                event: "downloadStarted",
                file_name: Some(&file_name),
                message: None,
                file_index: Some(file_index),
                file_count: Some(file_count),
                bytes_received: Some(0),
                bytes_per_second: Some(0),
            });
            drop(state);

            let window = webview.window().clone();
            let _ = window.set_title(&format!(
                "Downloading {file_index}/{file_count}: {file_name}"
            ));
            let _ = window.set_progress_bar(ProgressBarState {
                status: Some(ProgressBarStatus::Indeterminate),
                progress: None,
            });
            update_toolbar(
                toolbar,
                ToolbarState {
                    status: "downloading",
                    file_name: &file_name,
                    file_index,
                    file_count,
                    bytes_received: 0,
                    bytes_per_second: 0,
                },
            );
            monitor_download(
                window,
                toolbar.clone(),
                Arc::clone(queue),
                download_path,
                file_name,
                file_index,
                file_count,
            );
            true
        }
        DownloadEvent::Finished { path, success, .. } => {
            let mut state = queue.lock().expect("download queue mutex poisoned");
            if state.retry_requested {
                return true;
            }
            if state.ignored_finished_events > 0 {
                state.ignored_finished_events -= 1;
                return true;
            }
            let completed_name = state.current.file_name.clone();
            state.download_in_progress = false;

            if !success {
                if let Some(path) = path {
                    let _ = fs::remove_file(path);
                } else {
                    let _ = fs::remove_file(download_directory.join(&completed_name));
                }
                let file_index = state.current_index;
                let file_count = state.total_items;
                drop(state);
                update_toolbar(
                    toolbar,
                    ToolbarState {
                        status: "stalled",
                        file_name: &completed_name,
                        file_index,
                        file_count,
                        bytes_received: 0,
                        bytes_per_second: 0,
                    },
                );
                emit_event(HelperEvent {
                    event: "downloadFailed",
                    file_name: Some(&completed_name),
                    message: Some("The download failed; use CurseForge Try Again or Luna restart"),
                    file_index: Some(file_index),
                    file_count: Some(file_count),
                    bytes_received: None,
                    bytes_per_second: None,
                });
                let _ = webview.window().set_progress_bar(ProgressBarState {
                    status: Some(ProgressBarStatus::Error),
                    progress: None,
                });
                return true;
            }
            let file_index = state.current_index;
            let file_count = state.total_items;
            drop(state);
            update_toolbar(
                toolbar,
                ToolbarState {
                    status: "waiting",
                    file_name: &completed_name,
                    file_index,
                    file_count,
                    bytes_received: 0,
                    bytes_per_second: 0,
                },
            );
            let _ = webview.window().set_title(&format!(
                "Verifying {file_index}/{file_count}: {completed_name}"
            ));
            emit_event(HelperEvent {
                event: "fileComplete",
                file_name: Some(&completed_name),
                message: None,
                file_index: Some(file_index),
                file_count: Some(file_count),
                bytes_received: None,
                bytes_per_second: None,
            });
            true
        }
        _ => true,
    }
}

fn layout_webviews(window: &Window, toolbar: &Webview, content: &Webview, size: PhysicalSize<u32>) {
    let scale_factor = window.scale_factor().unwrap_or(1.0);
    let toolbar_height = ((TOOLBAR_HEIGHT * scale_factor).round() as u32).min(size.height);
    let content_height = size.height.saturating_sub(toolbar_height);
    let _ = toolbar.set_position(PhysicalPosition::new(0, 0));
    let _ = toolbar.set_size(PhysicalSize::new(size.width, toolbar_height));
    let _ = content.set_position(PhysicalPosition::new(0, toolbar_height as i32));
    let _ = content.set_size(PhysicalSize::new(size.width, content_height));
}

fn run(request: DownloadRequest) -> tauri::Result<i32> {
    let mut items = VecDeque::from(request.items);
    let total_items = request.total_items.unwrap_or(items.len());
    let first = items
        .pop_front()
        .expect("request was validated as non-empty");
    let first_url = Url::parse(&first.url).expect("request URL was validated");
    let queue = Arc::new(Mutex::new(QueueState {
        current: first,
        remaining: items,
        download_in_progress: false,
        retry_requested: false,
        ignored_finished_events: 0,
        current_index: request.start_index,
        total_items,
    }));

    let queue_for_setup = Arc::clone(&queue);
    let download_directory = request.download_directory;
    let proxy = request.proxy;
    let app = tauri::Builder::default()
        .setup(move |app| {
            let window = WindowBuilder::new(app, "curseforge-download")
                .title("CurseForge Download")
                .inner_size(1100.0, 760.0)
                .min_inner_size(800.0, 600.0)
                .visible(true)
                .build()?;

            let app_for_retry = app.handle().clone();
            let queue_for_retry = Arc::clone(&queue_for_setup);
            let queue_for_toolbar = Arc::clone(&queue_for_setup);
            let toolbar_builder = configure_webview_builder(
                WebviewBuilder::new("download-toolbar", WebviewUrl::App("index.html".into()))
                    .on_navigation(move |url| {
                        if url.scheme() == "luna" && url.host_str() == Some("retry") {
                            request_retry(&app_for_retry, &queue_for_retry);
                            return false;
                        }
                        true
                    })
                    .on_page_load(move |toolbar, _| {
                        let state = queue_for_toolbar
                            .lock()
                            .expect("download queue mutex poisoned");
                        update_toolbar(
                            &toolbar,
                            ToolbarState {
                                status: "waiting",
                                file_name: &state.current.file_name,
                                file_index: state.current_index,
                                file_count: state.total_items,
                                bytes_received: 0,
                                bytes_per_second: 0,
                            },
                        );
                    }),
                &proxy,
            );

            let initial_size = window.inner_size()?;
            let toolbar_height =
                ((TOOLBAR_HEIGHT * window.scale_factor()?).round() as u32).min(initial_size.height);
            let toolbar = window.add_child(
                toolbar_builder,
                PhysicalPosition::new(0, 0),
                PhysicalSize::new(initial_size.width, toolbar_height),
            )?;

            let queue_for_download = Arc::clone(&queue_for_setup);
            let directory_for_download = download_directory.clone();
            let toolbar_for_download = toolbar.clone();
            let content_builder = configure_webview_builder(
                WebviewBuilder::new("curseforge-content", WebviewUrl::External(first_url))
                    .on_download(move |webview, event| {
                        handle_download(
                            webview,
                            &toolbar_for_download,
                            event,
                            &queue_for_download,
                            &directory_for_download,
                        )
                    }),
                &proxy,
            );
            let content = window.add_child(
                content_builder,
                PhysicalPosition::new(0, toolbar_height as i32),
                PhysicalSize::new(
                    initial_size.width,
                    initial_size.height.saturating_sub(toolbar_height),
                ),
            )?;

            listen_for_commands(app.handle().clone(), Arc::clone(&queue_for_setup));

            let window_for_resize = window.clone();
            let toolbar_for_resize = toolbar.clone();
            let content_for_resize = content.clone();
            window.on_window_event(move |event| {
                if let WindowEvent::Resized(size) = event {
                    layout_webviews(
                        &window_for_resize,
                        &toolbar_for_resize,
                        &content_for_resize,
                        *size,
                    );
                }
            });
            Ok(())
        })
        .build(tauri::generate_context!())?;
    let requested_exit_code = Arc::new(Mutex::new(None));
    let exit_code_for_event = Arc::clone(&requested_exit_code);
    let runtime_exit_code = app.run_return(move |_, event| {
        if let RunEvent::ExitRequested {
            code: Some(exit_code),
            ..
        } = event
        {
            *exit_code_for_event
                .lock()
                .expect("exit code mutex poisoned") = Some(exit_code);
        }
    });
    let requested_exit_code = *requested_exit_code
        .lock()
        .expect("exit code mutex poisoned");
    Ok(requested_exit_code.unwrap_or(runtime_exit_code))
}

fn main() {
    let request = match parse_request() {
        Ok(request) => request,
        Err(message) => {
            emit_event(HelperEvent {
                event: "error",
                file_name: None,
                message: Some(&message),
                file_index: None,
                file_count: None,
                bytes_received: None,
                bytes_per_second: None,
            });
            std::process::exit(2);
        }
    };

    match run(request) {
        Ok(exit_code) => std::process::exit(exit_code),
        Err(error) => {
            emit_event(HelperEvent {
                event: "error",
                file_name: None,
                message: Some(&error.to_string()),
                file_index: None,
                file_count: None,
                bytes_received: None,
                bytes_per_second: None,
            });
            std::process::exit(2);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn accepts_only_curseforge_download_pages() {
        assert!(is_curseforge_page(
            &Url::parse("https://www.curseforge.com/minecraft/mc-mods/example/download/123")
                .unwrap()
        ));
        assert!(!is_curseforge_page(
            &Url::parse("https://www.curseforge.com/minecraft/mc-mods/example/files/123").unwrap()
        ));
        assert!(!is_curseforge_page(
            &Url::parse("https://example.com/download/123").unwrap()
        ));
    }

    #[test]
    fn accepts_only_leaf_file_names() {
        assert!(validate_file_name("example.jar"));
        assert!(!validate_file_name("../example.jar"));
        assert!(!validate_file_name("folder/example.jar"));
        assert!(!validate_file_name(""));
    }

    #[test]
    fn creates_supported_proxy_urls() {
        let proxy = ProxySettings {
            proxy_type: "SOCKS5".into(),
            host: "127.0.0.1".into(),
            port: 1080,
        };
        assert_eq!(
            proxy_url(&proxy).unwrap().unwrap().as_str(),
            "socks5://127.0.0.1:1080"
        );
        assert!(proxy_url(&ProxySettings::default()).unwrap().is_none());
    }
}
