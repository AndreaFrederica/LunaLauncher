use std::collections::HashSet;
use std::fs::{self, File};
use std::io::Write;
use std::path::{Component, Path, PathBuf};

use anyhow::{Context, Result, bail};
use zip::write::{SimpleFileOptions, ZipWriter};
use zip::{CompressionMethod, ZipArchive};

use crate::sfx::PayloadSource;

pub fn list(source: &PayloadSource) -> Result<Vec<String>> {
    source.verify_hash()?;
    let mut archive =
        ZipArchive::new(source.open_reader()?).context("payload is not a readable ZIP archive")?;
    let mut names = Vec::with_capacity(archive.len());
    let mut seen = HashSet::with_capacity(archive.len());
    for index in 0..archive.len() {
        let entry = archive.by_index(index)?;
        validate_utf8_name(entry.name_raw())?;
        let relative = validate_entry_name(entry.name())?;
        if !seen.insert(relative) {
            bail!("payload contains duplicate entry: {}", entry.name());
        }
        names.push(entry.name().to_owned());
    }
    Ok(names)
}

/// Read one UTF-8 named file from the payload without extracting the archive.
///
/// The installer uses this for resources that belong to the shell itself, such
/// as the bundled Chinese font. Keeping the resource in the normal payload
/// keeps the shell executable small while still making the SFX self-contained.
pub fn read_entry(source: &PayloadSource, name: &str) -> Result<Vec<u8>> {
    source.verify_hash()?;
    let mut archive =
        ZipArchive::new(source.open_reader()?).context("payload is not a readable ZIP archive")?;
    let mut entry = archive
        .by_name(name)
        .with_context(|| format!("payload entry is missing: {name}"))?;
    validate_utf8_name(entry.name_raw())?;
    let relative = validate_entry_name(entry.name())?;
    if relative != Path::new(name) || entry.is_dir() {
        bail!("payload entry is not the requested file: {name}");
    }
    let mut contents = Vec::with_capacity(entry.size() as usize);
    std::io::copy(&mut entry, &mut contents)?;
    Ok(contents)
}

pub fn extract(source: &PayloadSource, destination: &Path) -> Result<usize> {
    let mut no_progress = |_: usize, _: usize| {};
    extract_with_progress(source, destination, &mut no_progress)
}

/// Extract a payload and report completed archive entries through `progress`.
///
/// The callback is deliberately entry based: it keeps the archive module
/// independent from the installer UI while still allowing a responsive
/// progress indicator during a large installation.
pub fn extract_with_progress(
    source: &PayloadSource,
    destination: &Path,
    progress: &mut dyn FnMut(usize, usize),
) -> Result<usize> {
    source.verify_hash()?;
    fs::create_dir_all(destination)
        .with_context(|| format!("create extraction directory: {}", destination.display()))?;
    let mut archive =
        ZipArchive::new(source.open_reader()?).context("payload is not a readable ZIP archive")?;
    let total_entries = archive.len();
    let mut extracted = 0;
    let mut seen = HashSet::with_capacity(archive.len());
    for index in 0..archive.len() {
        let mut entry = archive.by_index(index)?;
        validate_utf8_name(entry.name_raw())?;
        let relative = validate_entry_name(entry.name())?;
        if !seen.insert(relative.clone()) {
            bail!("payload contains duplicate entry: {}", entry.name());
        }
        let output = destination.join(relative);
        if entry.is_dir() {
            ensure_safe_parent(destination, output.parent().unwrap_or(destination))?;
            if fs::symlink_metadata(&output).is_ok_and(|metadata| metadata.file_type().is_symlink())
            {
                bail!("refusing to overwrite symbolic link: {}", output.display());
            }
            fs::create_dir_all(&output)?;
            progress(index + 1, total_entries);
            continue;
        }
        if entry
            .unix_mode()
            .is_some_and(|mode| mode & 0o170000 == 0o120000)
        {
            bail!(
                "symbolic links are not allowed in payload: {}",
                entry.name()
            );
        }
        if let Some(parent) = output.parent() {
            ensure_safe_parent(destination, parent)?;
            fs::create_dir_all(parent)?;
        }
        if fs::symlink_metadata(&output).is_ok_and(|metadata| metadata.file_type().is_symlink()) {
            bail!("refusing to overwrite symbolic link: {}", output.display());
        }
        let mut file = File::create(&output)
            .with_context(|| format!("create extracted file: {}", output.display()))?;
        std::io::copy(&mut entry, &mut file)?;
        file.flush()?;
        extracted += 1;
        progress(index + 1, total_entries);
    }
    Ok(extracted)
}

/// Extract through a sibling temporary directory and replace the destination
/// only after the complete ZIP has been verified. Existing `UserData` is
/// moved across the replacement when requested.
pub fn extract_atomic(
    source: &PayloadSource,
    destination: &Path,
    preserve_user_data: bool,
) -> Result<usize> {
    let mut no_progress = |_: usize, _: usize| {};
    extract_atomic_with_progress(source, destination, preserve_user_data, &mut no_progress)
}

pub fn extract_atomic_with_progress(
    source: &PayloadSource,
    destination: &Path,
    preserve_user_data: bool,
    progress: &mut dyn FnMut(usize, usize),
) -> Result<usize> {
    let parent = destination.parent().unwrap_or_else(|| Path::new("."));
    fs::create_dir_all(parent)?;
    let staging = tempfile::Builder::new()
        .prefix(".luna-install-")
        .tempdir_in(parent)
        .context("create installation staging directory")?;
    let count = extract_with_progress(source, staging.path(), progress)?;

    let backup = if destination.exists() {
        let backup = tempfile::Builder::new()
            .prefix(".luna-install-backup-")
            .tempdir_in(parent)?
            .keep();
        fs::remove_dir(&backup)?;
        fs::rename(destination, &backup)
            .with_context(|| format!("move existing installation to {}", backup.display()))?;
        Some(backup)
    } else {
        None
    };

    if let Some(backup) = &backup {
        if preserve_user_data {
            let old_data = backup.join("UserData");
            let new_data = staging.path().join("UserData");
            if old_data.exists() {
                if new_data.exists() {
                    fs::remove_dir_all(&new_data)?;
                }
                fs::rename(old_data, new_data)?;
            }
        }
    }

    if let Err(error) = fs::rename(staging.path(), destination) {
        if let Some(backup) = &backup {
            let _ = fs::rename(backup, destination);
        }
        return Err(error)
            .with_context(|| format!("activate installation at {}", destination.display()));
    }
    let _ = staging.keep();
    if let Some(backup) = backup {
        let _ = fs::remove_dir_all(backup);
    }
    Ok(count)
}

/// Create a deterministic ZIP payload from a directory.
///
/// The zip crate sets the UTF-8/EFS flag for names containing non-ASCII
/// characters. ASCII names are encoding-invariant, so they need no legacy
/// code-page interpretation. The resulting archive remains readable by
/// standard ZIP tools without a private metadata format.
pub fn create_zip(source: &Path, output: &Path) -> Result<usize> {
    if !source.is_dir() {
        bail!("ZIP source is not a directory: {}", source.display());
    }
    if output.exists() && output.is_dir() {
        bail!("ZIP output is a directory: {}", output.display());
    }
    if let Some(parent) = output.parent() {
        fs::create_dir_all(parent)?;
    }
    let output_file =
        File::create(output).with_context(|| format!("create ZIP output: {}", output.display()))?;
    let mut writer = ZipWriter::new(output_file);
    let source = source.canonicalize()?;
    let mut paths = Vec::new();
    collect_entries(&source, &source, &mut paths)?;
    // Portable/system selection is installation state, not release content.
    // Never let a stale local marker make a freshly packaged release start in
    // the wrong data location.
    paths.retain(|(relative, _)| !is_runtime_state_marker(relative)
        && !relative.components().next().is_some_and(|part| part.as_os_str().eq_ignore_ascii_case("UserData")));
    paths.sort_by(|left, right| left.0.cmp(&right.0));

    let mut count = 0;
    for (relative, is_directory) in paths {
        let name = relative.to_string_lossy().replace('\\', "/");
        let name = if is_directory && !name.ends_with('/') {
            format!("{name}/")
        } else {
            name
        };
        let source_path = source.join(&relative);
        let options = SimpleFileOptions::default()
            .compression_method(CompressionMethod::Deflated)
            .large_file(!is_directory && fs::metadata(&source_path)?.len() > u32::MAX as u64);
        if is_directory {
            writer.add_directory(name, options)?;
        } else {
            writer.start_file(name, options)?;
            let mut file = File::open(source_path)?;
            std::io::copy(&mut file, &mut writer)?;
        }
        count += 1;
    }
    writer.finish()?;
    Ok(count)
}

fn is_runtime_state_marker(path: &Path) -> bool {
    if path.components().count() != 1 {
        return false;
    }
    matches!(
        path.file_name().and_then(|name| name.to_str()),
        Some(".luna-use-system-data") | Some(".luna-data-dir") | Some("portable.txt") | Some("portable.txt.disabled")
        | Some(".luna-install.json") | Some(".luna-update.json") | Some(".luna-update.lock")
    )
}

fn collect_entries(
    root: &Path,
    directory: &Path,
    entries: &mut Vec<(PathBuf, bool)>,
) -> Result<()> {
    let mut children = fs::read_dir(directory)
        .with_context(|| format!("read ZIP source directory: {}", directory.display()))?
        .map(|entry| entry.map(|entry| entry.path()))
        .collect::<std::io::Result<Vec<_>>>()?;
    children.sort();
    for path in children {
        let metadata = fs::symlink_metadata(&path)?;
        let relative = path.strip_prefix(root)?.to_path_buf();
        if metadata.file_type().is_symlink() {
            let resolved = fs::canonicalize(&path)?;
            if !resolved.starts_with(root) || !fs::metadata(&resolved)?.is_file() {
                bail!(
                    "only internal file symbolic links are allowed in ZIP payload: {}",
                    path.display()
                );
            }
            // Materialize internal file links as regular files in the ZIP.
            entries.push((relative, false));
        } else if metadata.is_dir() {
            entries.push((relative.clone(), true));
            collect_entries(root, &path, entries)?;
        } else if metadata.is_file() {
            entries.push((relative, false));
        } else {
            bail!("unsupported ZIP source entry: {}", path.display());
        }
    }
    Ok(())
}

fn validate_entry_name(raw_name: &str) -> Result<PathBuf> {
    if raw_name.is_empty() || raw_name.contains('\0') {
        bail!("payload contains an empty entry name");
    }
    let normalized = raw_name.replace('\\', "/");
    let name = Path::new(&normalized);
    let mut clean = PathBuf::new();
    for component in name.components() {
        match component {
            Component::Normal(part) => clean.push(part),
            Component::CurDir => {}
            Component::ParentDir | Component::RootDir | Component::Prefix(_) => {
                bail!("unsafe payload path: {raw_name}");
            }
        }
    }
    if clean.as_os_str().is_empty() {
        bail!("payload contains an invalid entry name: {raw_name}");
    }
    Ok(clean)
}

fn validate_utf8_name(raw_name: &[u8]) -> Result<()> {
    std::str::from_utf8(raw_name).context("payload contains a non-UTF-8 filename")?;
    Ok(())
}

fn ensure_safe_parent(root: &Path, parent: &Path) -> Result<()> {
    let relative = parent.strip_prefix(root).unwrap_or(parent);
    let mut current = root.to_path_buf();
    for component in relative.components() {
        if let Component::Normal(part) = component {
            current.push(part);
            if fs::symlink_metadata(&current)
                .is_ok_and(|metadata| metadata.file_type().is_symlink())
            {
                bail!("refusing to traverse symbolic link: {}", current.display());
            }
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::tempdir;

    #[test]
    fn rejects_path_escape() {
        assert!(validate_entry_name("../escape.txt").is_err());
        assert!(validate_entry_name("C:/escape.txt").is_err());
        assert!(validate_entry_name("/escape.txt").is_err());
        assert!(validate_entry_name(r"..\escape.txt").is_err());
    }

    #[test]
    fn accepts_unicode_relative_path() {
        let path = validate_entry_name("资源/启动器.exe").unwrap();
        assert_eq!(path, PathBuf::from("资源/启动器.exe"));
    }

    #[test]
    fn rejects_non_utf8_filename_bytes() {
        assert!(validate_utf8_name(&[0xff, 0xfe]).is_err());
        assert!(validate_utf8_name("资源.txt".as_bytes()).is_ok());
    }

    #[test]
    fn creates_utf8_marked_zip_and_round_trips_unicode_names() {
        let directory = tempdir().unwrap();
        let source = directory.path().join("source");
        let output = directory.path().join("payload.zip");
        let destination = directory.path().join("destination");
        fs::create_dir_all(source.join("资源")).unwrap();
        fs::write(source.join("资源/启动器.txt"), "内容").unwrap();
        fs::write(source.join("ascii.txt"), "ascii").unwrap();
        fs::write(source.join(".luna-use-system-data"), "stale state").unwrap();
        fs::write(source.join(".luna-data-dir"), "private local path").unwrap();
        fs::write(source.join(".luna-install.json"), "private installation record").unwrap();
        fs::create_dir_all(source.join("UserData/instances")).unwrap();
        fs::write(source.join("UserData/instances/private.txt"), "private user data").unwrap();

        assert_eq!(create_zip(&source, &output).unwrap(), 3);
        let bytes = fs::read(&output).unwrap();
        assert!(
            bytes
                .windows(4)
                .any(|window| window == [0x50, 0x4b, 0x01, 0x02])
        );
        let payload = PayloadSource::from_path(&output).unwrap();
        assert!(
            list(&payload)
                .unwrap()
                .contains(&"资源/启动器.txt".to_owned())
        );
        let mut updates = Vec::new();
        assert_eq!(
            extract_with_progress(&payload, &destination, &mut |done, total| {
                updates.push((done, total));
            })
            .unwrap(),
            2
        );
        assert_eq!(updates.last().copied(), Some((3, 3)));
        assert_eq!(
            fs::read_to_string(destination.join("资源/启动器.txt")).unwrap(),
            "内容"
        );
        assert_eq!(
            read_entry(&payload, "资源/启动器.txt").unwrap(),
            "内容".as_bytes()
        );
    }

    #[test]
    fn atomic_extract_preserves_user_data() {
        let directory = tempdir().unwrap();
        let source = directory.path().join("source");
        let output = directory.path().join("payload.zip");
        let destination = directory.path().join("destination");
        fs::create_dir_all(destination.join("UserData")).unwrap();
        fs::write(destination.join("UserData/keep.txt"), "keep").unwrap();
        fs::create_dir_all(&source).unwrap();
        fs::write(source.join("new.txt"), "new").unwrap();
        create_zip(&source, &output).unwrap();
        let payload = PayloadSource::from_path(&output).unwrap();

        extract_atomic(&payload, &destination, true).unwrap();
        assert_eq!(
            fs::read_to_string(destination.join("UserData/keep.txt")).unwrap(),
            "keep"
        );
        assert_eq!(
            fs::read_to_string(destination.join("new.txt")).unwrap(),
            "new"
        );
    }

    #[test]
    fn rejects_truncated_zip() {
        let directory = tempdir().unwrap();
        let source = directory.path().join("source");
        let output = directory.path().join("truncated.zip");
        fs::create_dir_all(&source).unwrap();
        fs::write(source.join("file.txt"), "contents").unwrap();
        create_zip(&source, &output).unwrap();
        let mut bytes = fs::read(&output).unwrap();
        bytes.truncate(bytes.len().saturating_sub(8));
        fs::write(&output, bytes).unwrap();

        let payload = PayloadSource::from_path(&output).unwrap();
        assert!(list(&payload).is_err());
    }
}
