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

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum InstanceLayoutMode {
    #[default]
    Mmc,
    Vanilla,
    Custom,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum InstanceStoreFormat {
    #[default]
    Prism,
    VanillaCompatible,
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
    #[serde(default)]
    pub instance_layout: InstanceLayoutMode,
    #[serde(default)]
    pub portable_mode: bool,
    #[serde(default)]
    pub data_dir: Option<PathBuf>,
    #[serde(default)]
    pub register_instance_store: bool,
    #[serde(default)]
    pub instance_store_path: Option<PathBuf>,
    #[serde(default)]
    pub instance_store_format: InstanceStoreFormat,
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
    save_install(config)?;
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

/// Installation identity belongs to this checkout, not the last wizard run.
pub const INSTALL_RECORD: &str = ".luna-install.json";

pub fn load_install(directory: &Path) -> Result<Option<InstallConfig>> {
    let path = directory.join(INSTALL_RECORD);
    if !path.exists() { return Ok(None); }
    let mut config: InstallConfig = serde_json::from_slice(&fs::read(&path)?)
        .with_context(|| format!("read installation record: {}", path.display()))?;
    // Portable installations can be moved after installation.
    config.install_dir = directory.to_path_buf();
    Ok(Some(config))
}

pub fn save_install(config: &InstallConfig) -> Result<()> {
    fs::write(config.install_dir.join(INSTALL_RECORD), serde_json::to_vec_pretty(config)?)?;
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn older_installer_config_defaults_to_system_data_and_custom_path_roundtrips() {
        let mut config: InstallConfig = serde_json::from_str(r#"{"install_dir":"launcher"}"#).unwrap();
        assert!(config.data_dir.is_none());
        config.data_dir = Some(PathBuf::from("数据 with spaces"));
        let decoded: InstallConfig = serde_json::from_str(&serde_json::to_string(&config).unwrap()).unwrap();
        assert_eq!(decoded.data_dir, config.data_dir);
    }
}
