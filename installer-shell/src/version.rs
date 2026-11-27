use std::{
    fmt,
    io::Read,
    path::Path,
    process::{Command, Stdio},
    thread,
    time::{Duration, Instant},
};

use crate::sfx::PayloadSource;
use anyhow::{Context, Result, bail};

pub const PACKAGE_ENTRY: &str = ".luna-package-version";

/// Read the small version entry without unpacking or running the payload.
/// The update transaction verifies the full SFX hash before extraction.
pub fn package_version(source: &PayloadSource) -> Result<Option<LauncherVersion>> {
    let mut archive = zip::ZipArchive::new(source.open_reader()?)?;
    let mut entry = match archive.by_name(PACKAGE_ENTRY) {
        Ok(entry) => entry,
        Err(zip::result::ZipError::FileNotFound) => return Ok(None),
        Err(error) => return Err(error.into()),
    };
    if entry.is_dir() || entry.size() > 128 {
        bail!("invalid launcher package version entry");
    }
    let mut text = String::new();
    entry.read_to_string(&mut text)?;
    LauncherVersion::parse(text.trim())
        .map(Some)
        .context("invalid launcher package version")
}

/// Luna releases use MAJOR.MINOR.PATCH[-YYYYMMDDrN]. The optional channel
/// appended to the display version is irrelevant to Windows upgrade ordering.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct LauncherVersion {
    number: [u32; 3],
    date: u32,
    revision: u32,
}

impl LauncherVersion {
    pub fn parse(value: &str) -> Option<Self> {
        let (number, suffix) = value.split_once('-').unwrap_or((value, ""));
        let mut parts = number.split('.');
        let number = [
            decimal(parts.next()?)?,
            decimal(parts.next()?)?,
            decimal(parts.next()?)?,
        ];
        if parts.next().is_some() {
            return None;
        }
        let (date, revision) = if suffix.is_empty() {
            (0, 0)
        } else {
            let build = suffix.split('-').next()?;
            let (date, revision) = build.split_once('r').unwrap_or((build, "0"));
            if date.len() != 8 {
                return None;
            }
            (decimal(date)?, decimal(revision)?)
        };
        Some(Self {
            number,
            date,
            revision,
        })
    }
}

fn decimal(value: &str) -> Option<u32> {
    if !value.bytes().all(|byte| byte.is_ascii_digit()) {
        return None;
    }
    value.parse().ok()
}

impl fmt::Display for LauncherVersion {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        let [major, minor, patch] = self.number;
        write!(formatter, "{major}.{minor}.{patch}")?;
        if self.date != 0 {
            write!(formatter, "-{:08}", self.date)?;
        }
        if self.revision != 0 {
            write!(formatter, "r{}", self.revision)?;
        }
        Ok(())
    }
}

pub fn probe_launcher(executable: &Path) -> Result<LauncherVersion> {
    let executable = executable
        .canonicalize()
        .context("locate launcher to check its version")?;
    let mut command = Command::new(&executable);
    command
        .arg("--version")
        .current_dir(
            executable
                .parent()
                .context("launcher has no installation directory")?,
        )
        .env("QT_QPA_PLATFORM", "offscreen")
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::null());
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        command.creation_flags(windows_sys::Win32::System::Threading::CREATE_NO_WINDOW);
    }
    let mut child = command.spawn().context("run lunalauncher.exe --version")?;
    let deadline = Instant::now() + Duration::from_secs(15);
    loop {
        if let Some(status) = child.try_wait()? {
            if !status.success() {
                bail!("launcher version check failed ({status})");
            }
            break;
        }
        if Instant::now() >= deadline {
            let _ = child.kill();
            let _ = child.wait();
            bail!("launcher version check timed out");
        }
        thread::sleep(Duration::from_millis(50));
    }
    let mut output = String::new();
    child
        .stdout
        .take()
        .unwrap()
        .take(4096)
        .read_to_string(&mut output)?;
    output
        .lines()
        .find_map(|line| LauncherVersion::parse(line.strip_prefix("LunaLauncher ")?.trim()))
        .context("launcher did not report a recognized LunaLauncher version")
}

#[cfg(test)]
mod tests {
    use super::LauncherVersion;

    #[test]
    fn compares_release_dates_and_numeric_revisions_without_display_channels() {
        let version = |value| LauncherVersion::parse(value).unwrap();
        assert!(version("11.0.2-20261119r10") > version("11.0.2-20261119r2"));
        assert!(version("11.0.2-20261120") > version("11.0.2-20261119r10"));
        assert!(version("11.0.3") > version("11.0.2-20261120"));
        assert_eq!(
            version("11.0.2-20261120-meson").to_string(),
            "11.0.2-20261120"
        );
        assert!(LauncherVersion::parse("11.0.2.0").is_none());
    }
}
