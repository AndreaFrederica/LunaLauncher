//! Read-only migration preview; only apply a plan after installer review.
use anyhow::{Context, Result, bail};
use serde_json::{Value, json};
use std::{
    fs,
    path::{Path, PathBuf},
};

#[derive(Clone, Debug)]
pub struct Profile {
    pub path: PathBuf,
    pub name: String,
    pub mode: String,
    pub evidence: String,
    original: Option<Vec<u8>>,
}
#[derive(Clone, Debug)]
pub struct Plan {
    pub root: PathBuf,
    pub profiles: Vec<Profile>,
    pub warnings: Vec<String>,
    sources: Vec<(PathBuf, Option<Vec<u8>>)>,
}
fn object(path: &Path) -> Value {
    fs::read(path)
        .ok()
        .and_then(|b| serde_json::from_slice(&b).ok())
        .unwrap_or(Value::Null)
}
pub fn candidates(folder: &Path) -> Vec<PathBuf> {
    let mut paths = vec![folder.to_path_buf()];
    if folder.join(".minecraft").is_dir() {
        paths.push(folder.join(".minecraft"));
    }
    for base in [Some(folder), folder.parent()].into_iter().flatten() {
        if let Some(profiles) = object(&base.join("hmcl.json"))["configurations"].as_object() {
            for profile in profiles.values() {
                if let Some(path) = profile["gameDir"].as_str().filter(|s| !s.is_empty()) {
                    let path = base.join(path);
                    if path.is_dir() {
                        paths.push(path);
                    }
                }
            }
        }
    }
    paths.sort();
    paths.dedup();
    paths
}
pub fn scan(folder: &Path) -> Plan {
    scan_from(folder, folder)
}
pub fn scan_from(folder: &Path, launcher_folder: &Path) -> Plan {
    let root = if folder.join("versions").is_dir()
        || folder.file_name().is_some_and(|n| n == ".minecraft")
    {
        folder.to_path_buf()
    } else {
        folder.join(".minecraft")
    };
    let mut plan = Plan {
        root: root.clone(),
        profiles: vec![],
        warnings: vec![],
        sources: vec![],
    };
    let mut global = Value::Null;
    for base in [Some(root.as_path()), root.parent(), Some(launcher_folder)]
        .into_iter()
        .flatten()
    {
        plan.sources.push((
            base.join("hmcl.json"),
            fs::read(base.join("hmcl.json")).ok(),
        ));
        if let Some(profiles) = object(&base.join("hmcl.json"))["configurations"].as_object() {
            for profile in profiles.values() {
                if let Some(dir) = profile["gameDir"].as_str() {
                    if base.join(dir).canonicalize().ok() == root.canonicalize().ok() {
                        global = profile["global"].clone();
                    }
                }
            }
        }
    }
    if let Ok(entries) = fs::read_dir(root.join("versions")) {
        for entry in entries.flatten() {
            let path = entry.path();
            let name = entry.file_name().to_string_lossy().into_owned();
            if !path.join(format!("{name}.json")).is_file() {
                continue;
            }
            for source in [
                path.join(format!("{name}.json")),
                path.join("hmclversion.cfg"),
                path.join("PCL/Setup.ini"),
            ] {
                plan.sources.push((source.clone(), fs::read(&source).ok()));
            }
            let original = fs::read(path.join("luna.settings.json")).ok();
            if original.as_ref().is_some_and(|b| {
                serde_json::from_slice::<Value>(b)
                    .ok()
                    .and_then(|v| v.get("settings").cloned())
                    .is_none_or(|v| !v.is_object())
            }) {
                plan.warnings
                    .push(format!("{name}: invalid luna.settings.json; not changed"));
                continue;
            }
            let luna = object(&path.join("luna.settings.json"));
            let local = object(&path.join("hmclversion.cfg"));
            let hmcl = if local.is_null() || local["usesGlobal"].as_bool() == Some(true) {
                &global
            } else {
                &local
            };
            let mut mode = "isolated".to_owned();
            let mut evidence = "未识别到明确设置，默认隔离 / default isolated".to_owned();
            if let Some(saved) = luna["settings"]["VanillaGameDirectoryMode"]
                .as_str()
                .filter(|s| ["isolated", "shared"].contains(s))
            {
                mode = saved.to_owned();
                evidence = "Luna settings".into();
            } else if let Some(kind) = hmcl["gameDirType"].as_i64() {
                match kind {
                    0 => {
                        mode = "shared".into();
                        evidence = "HMCL: root directory".into();
                    }
                    1 => {
                        evidence = "HMCL: version directory".into();
                    }
                    _ => {
                        mode = "auto".into();
                        evidence = format!(
                            "HMCL custom/unknown directory: {} (review required)",
                            hmcl["gameDir"]
                        );
                    }
                }
            } else {
                let pcl = fs::read_to_string(path.join("PCL/Setup.ini")).unwrap_or_default();
                let values: std::collections::HashMap<_, _> = pcl
                    .lines()
                    .filter_map(|l| l.split_once(':'))
                    .map(|(k, v)| (k.trim(), v.trim()))
                    .collect();
                if let Some(value) = values
                    .get("VersionArgumentIndieV2")
                    .filter(|v| v.eq_ignore_ascii_case("true") || v.eq_ignore_ascii_case("false"))
                {
                    mode = if value.eq_ignore_ascii_case("true") {
                        "isolated"
                    } else {
                        "shared"
                    }
                    .into();
                    evidence = "PCL: VersionArgumentIndieV2".into();
                } else if let Some(value) = values
                    .get("VersionArgumentIndie")
                    .filter(|v| **v == "1" || **v == "2")
                {
                    mode = if *value == "1" { "isolated" } else { "shared" }.into();
                    evidence = "PCL: VersionArgumentIndie".into();
                } else if ["mods", "saves", "config", "game"]
                    .iter()
                    .any(|n| path.join(n).is_dir())
                {
                    evidence = "版本目录存在游戏数据 / version content".into();
                } else if ["mods", "saves", "config", "options.txt"]
                    .iter()
                    .any(|n| root.join(n).exists())
                {
                    mode = "shared".into();
                    evidence = "根目录存在游戏数据（推断，请确认） / inferred shared data".into();
                }
            }
            if evidence != "Luna settings" {
                let pcl = fs::read_to_string(path.join("PCL/Setup.ini")).unwrap_or_default();
                let values: std::collections::HashMap<_, _> = pcl
                    .lines()
                    .filter_map(|l| l.split_once(':'))
                    .map(|(k, v)| (k.trim(), v.trim()))
                    .collect();
                let pcl_mode = values
                    .get("VersionArgumentIndieV2")
                    .and_then(|v| {
                        if v.eq_ignore_ascii_case("true") {
                            Some("isolated")
                        } else if v.eq_ignore_ascii_case("false") {
                            Some("shared")
                        } else {
                            None
                        }
                    })
                    .or_else(|| {
                        values.get("VersionArgumentIndie").and_then(|v| match *v {
                            "1" => Some("isolated"),
                            "2" => Some("shared"),
                            _ => None,
                        })
                    });
                if evidence.starts_with("HMCL:") && pcl_mode.is_some_and(|v| v != mode) {
                    mode = "auto".into();
                    evidence =
                        "HMCL/PCL 设置冲突，请选择 / Conflicting launcher settings; choose a mode"
                            .into();
                }
                if path.join("game").is_dir() && evidence.contains("version content") {
                    mode = "auto".into();
                    evidence = format!(
                        "自定义子目录 {}，请选择要使用的模式 / Custom subdirectory; choose a mode",
                        path.join("game").display()
                    );
                }
            }
            plan.profiles.push(Profile {
                path,
                name,
                mode,
                evidence,
                original,
            });
        }
    }
    plan.profiles.sort_by(|a, b| a.name.cmp(&b.name));
    plan
}
impl Plan {
    pub fn validate(&self) -> Result<()> {
        for (path, content) in &self.sources {
            if fs::read(path).ok() != *content {
                bail!("{} changed after preview; scan again", path.display());
            }
        }
        for p in &self.profiles {
            if fs::read(p.path.join("luna.settings.json")).ok() != p.original {
                bail!("{} settings changed after preview; scan again", p.name);
            }
            if !["isolated", "shared"].contains(&p.mode.as_str()) {
                bail!("invalid directory mode");
            }
        }
        Ok(())
    }
    pub fn apply(&self) -> Result<()> {
        self.validate()?;
        for p in &self.profiles {
            if p.mode == "auto" {
                continue;
            }
            let mut data = p
                .original
                .as_ref()
                .map(|b| serde_json::from_slice::<Value>(b))
                .transpose()?
                .unwrap_or(json!({"formatVersion":1,"settings":{}}));
            data["settings"]["VanillaGameDirectoryMode"] = json!(p.mode);
            let mut file = tempfile::NamedTempFile::new_in(&p.path)?;
            serde_json::to_writer_pretty(&mut file, &data)?;
            file.persist(p.path.join("luna.settings.json"))
                .context("save confirmed directory mode")?;
        }
        Ok(())
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn preserves_shared_and_isolated_profiles_and_requires_fresh_preview() {
        let dir = tempfile::tempdir().unwrap();
        let root = dir.path().join(".minecraft");
        for name in ["shared", "isolated", "new"] {
            let p = root.join("versions").join(name);
            fs::create_dir_all(&p).unwrap();
            fs::write(p.join(format!("{name}.json")), "{}").unwrap();
        }
        fs::write(
            root.join("versions/shared/hmclversion.cfg"),
            r#"{"gameDirType":0}"#,
        )
        .unwrap();
        fs::create_dir_all(root.join("versions/isolated/PCL")).unwrap();
        fs::write(
            root.join("versions/isolated/PCL/Setup.ini"),
            "VersionArgumentIndieV2:True",
        )
        .unwrap();
        let plan = scan(dir.path());
        assert_eq!(
            plan.profiles
                .iter()
                .map(|p| p.mode.as_str())
                .collect::<Vec<_>>(),
            ["isolated", "isolated", "shared"]
        );
        assert!(!root.join("versions/shared/luna.settings.json").exists());
        plan.apply().unwrap();
        assert_eq!(
            object(&root.join("versions/shared/luna.settings.json"))["settings"]["VanillaGameDirectoryMode"],
            "shared"
        );
        assert!(plan.validate().is_err());
    }
    #[test]
    fn follows_external_hmcl_directory_and_preserves_sidecar_fields() {
        let launcher = tempfile::tempdir().unwrap();
        let games = tempfile::tempdir().unwrap();
        let p = games.path().join("versions/test");
        fs::create_dir_all(&p).unwrap();
        fs::write(p.join("test.json"), "{}").unwrap();
        fs::write(
            p.join("luna.settings.json"),
            r#"{"settings":{"name":"Keep","JavaPath":"keep-java"}}"#,
        )
        .unwrap();
        fs::write(launcher.path().join("hmcl.json"), serde_json::to_vec(&json!({"configurations":{"external":{"gameDir":games.path(),"global":{"usesGlobal":true,"gameDirType":0}}}})).unwrap()).unwrap();
        let plan = scan_from(games.path(), launcher.path());
        assert_eq!(plan.profiles[0].mode, "shared");
        plan.apply().unwrap();
        assert_eq!(
            object(&p.join("luna.settings.json"))["settings"]["JavaPath"],
            "keep-java"
        );
        assert!(candidates(launcher.path()).contains(&games.path().to_path_buf()));
    }
    #[test]
    fn refuses_unknown_mode_and_stale_foreign_settings() {
        let dir = tempfile::tempdir().unwrap();
        let p = dir.path().join("versions/test");
        fs::create_dir_all(&p).unwrap();
        fs::write(p.join("test.json"), "{}").unwrap();
        fs::write(
            p.join("hmclversion.cfg"),
            r#"{"gameDirType":2,"gameDir":"custom"}"#,
        )
        .unwrap();
        let plan = scan(dir.path());
        assert_eq!(plan.profiles[0].mode, "auto");
        assert!(plan.validate().is_err());
        fs::write(p.join("hmclversion.cfg"), r#"{"gameDirType":1}"#).unwrap();
        let plan = scan(dir.path());
        assert!(plan.validate().is_ok());
        fs::write(p.join("hmclversion.cfg"), r#"{"gameDirType":0}"#).unwrap();
        assert!(plan.validate().is_err());
    }
}
