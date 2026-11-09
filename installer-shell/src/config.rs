use std::fs;
use std::path::{Path, PathBuf};

use anyhow::{Context, Result};
use directories::ProjectDirs;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum EntryPoint {
    #[default]
    Classic,
    Neo,
    Both,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InstallConfig {
    pub install_dir: PathBuf,
    #[serde(default)]
    pub installed_entries: Vec<String>,
    #[serde(default)]
    pub entry_point: EntryPoint,
    #[serde(default)]
    pub desktop_shortcuts: bool,
    #[serde(default)]
    pub start_menu_shortcuts: bool,
    #[serde(default = "default_true")]
    pub keep_user_data: bool,
}

fn default_true() -> bool {
    true
}

pub fn config_path() -> Option<PathBuf> {
    ProjectDirs::from("org", "LunaLauncher", "LunaLauncher")
        .map(|dirs| dirs.config_dir().join("installer.json"))
}

pub fn load() -> Result<Option<InstallConfig>> {
    let Some(path) = config_path() else {
        return Ok(None);
    };
    if !path.exists() {
        return Ok(None);
    }
    let contents = fs::read_to_string(&path)
        .with_context(|| format!("read installer config: {}", path.display()))?;
    Ok(Some(serde_json::from_str(&contents)?))
}

pub fn save(config: &InstallConfig) -> Result<()> {
    let Some(path) = config_path() else {
        return Ok(());
    };
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let contents = serde_json::to_string_pretty(config)?;
    fs::write(&path, contents)
        .with_context(|| format!("write installer config: {}", path.display()))?;
    Ok(())
}

pub fn reset() -> Result<()> {
    if let Some(path) = config_path() {
        if path.exists() {
            fs::remove_file(path)?;
        }
    }
    Ok(())
}

pub fn is_within_install_dir(path: &Path, install_dir: &Path) -> bool {
    path == install_dir || path.starts_with(install_dir)
}
