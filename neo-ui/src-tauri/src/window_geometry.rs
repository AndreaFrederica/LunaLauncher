// SPDX-License-Identifier: MPL-2.0
//! Persistent native window geometry for the Neo UI shell.
//!
//! Geometry belongs to the launcher data directory, alongside the other Web
//! UI settings. It is intentionally kept outside WebView storage so the
//! native window opens at the same size for every theme and every WebView.

use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use tauri::{LogicalSize, Manager, Window, WebviewWindow};

const DEFAULT_MAIN_WIDTH: f64 = 1200.0;
const DEFAULT_MAIN_HEIGHT: f64 = 800.0;
const MIN_MAIN_WIDTH: f64 = 720.0;
const MIN_MAIN_HEIGHT: f64 = 480.0;
const MAX_MAIN_WIDTH: f64 = 10000.0;
const MAX_MAIN_HEIGHT: f64 = 10000.0;

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
struct Size {
    width: f64,
    height: f64,
}

#[derive(Debug, Default, Deserialize, Serialize)]
struct Geometry {
    main: Option<Size>,
}

fn path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let startup = app.state::<crate::startup::Startup>();
    let context = startup.launcher.as_ref().map_err(Clone::clone)?;
    Ok(context.data_root.join("webui").join("window-geometry.json"))
}

fn clamp_size(size: Size) -> Size {
    Size {
        width: size.width.clamp(MIN_MAIN_WIDTH, MAX_MAIN_WIDTH),
        height: size.height.clamp(MIN_MAIN_HEIGHT, MAX_MAIN_HEIGHT),
    }
}

fn read(app: &tauri::AppHandle) -> Result<Geometry, String> {
    match std::fs::read(path(app)?) {
        Ok(bytes) => serde_json::from_slice(&bytes).map_err(|error| error.to_string()),
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(Geometry::default()),
        Err(error) => Err(error.to_string()),
    }
}

pub fn restore_main(window: &WebviewWindow) -> Result<(), String> {
    let geometry = read(window.app_handle())?;
    let size = geometry
        .main
        .map(clamp_size)
        .unwrap_or(Size { width: DEFAULT_MAIN_WIDTH, height: DEFAULT_MAIN_HEIGHT });
    window
        .set_size(LogicalSize::new(size.width, size.height))
        .map_err(|error| error.to_string())
}

pub fn save_main(window: &Window) -> Result<(), String> {
    let physical = window.inner_size().map_err(|error| error.to_string())?;
    let scale = window.scale_factor().unwrap_or(1.0).max(0.01);
    let size = clamp_size(Size {
        width: f64::from(physical.width) / scale,
        height: f64::from(physical.height) / scale,
    });
    let file_path = path(window.app_handle())?;
    let parent = file_path
        .parent()
        .ok_or_else(|| "窗口尺寸路径无效".to_string())?;
    std::fs::create_dir_all(parent).map_err(|error| error.to_string())?;
    let mut geometry = read(window.app_handle()).unwrap_or_default();
    geometry.main = Some(size);
    let mut file = tempfile::NamedTempFile::new_in(parent).map_err(|error| error.to_string())?;
    serde_json::to_writer(&mut file, &geometry).map_err(|error| error.to_string())?;
    use std::io::Write;
    file.flush().map_err(|error| error.to_string())?;
    file.persist(file_path).map_err(|error| error.to_string())?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sizes_are_clamped_to_native_window_limits() {
        let size = clamp_size(Size { width: 1.0, height: 20_000.0 });
        assert_eq!(size.width, MIN_MAIN_WIDTH);
        assert_eq!(size.height, MAX_MAIN_HEIGHT);
        assert_eq!(DEFAULT_MAIN_WIDTH, 1200.0);
        assert_eq!(DEFAULT_MAIN_HEIGHT, 800.0);
    }
}
