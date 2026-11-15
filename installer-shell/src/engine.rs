//! Generic installation engine shared by self-extracting applications.
//!
//! This module deliberately knows nothing about LunaLauncher, Windows
//! registry keys, shortcuts, or a particular launcher layout. Applications
//! provide an [`InstallBackend`] for those responsibilities and receive a
//! verified, progress-reporting payload extraction pipeline in return.

use std::path::Path;

use anyhow::Result;

use crate::{archive, sfx::PayloadSource};

/// Application-specific work around a generic payload installation.
pub trait InstallBackend {
    /// Stop running processes and prepare the destination before replacement.
    fn before_install(&self, destination: &Path) -> Result<()>;

    /// Register application files, shortcuts, configuration, and integrations
    /// after the payload has been extracted successfully.
    fn after_install(&self, destination: &Path, installed_entries: &[String]) -> Result<()>;
}

/// Input to the generic payload installer.
#[derive(Debug, Clone, Copy)]
pub struct InstallRequest<'a> {
    pub source: &'a PayloadSource,
    pub destination: &'a Path,
    /// A running SFX shell may be installing into its own directory. In that
    /// case the directory cannot be renamed and must be updated in place.
    pub in_place: bool,
    /// Preserve files that are not part of the package during replacement.
    pub preserve_user_data: bool,
}

/// Extract a payload and invoke the application adapter around it.
pub fn install<B: InstallBackend>(
    request: InstallRequest<'_>,
    backend: &B,
    progress: &mut dyn FnMut(f32, &str),
) -> Result<usize> {
    progress(0.02, "Validating package...");
    backend.before_install(request.destination)?;
    let installed_entries = archive::list(request.source)?;

    progress(0.05, "Extracting application files...");
    let mut extraction_progress = |done: usize, total: usize| {
        let fraction = 0.05 + 0.73 * (done as f32 / total.max(1) as f32);
        progress(
            fraction,
            &format!("Extracting application files ({done}/{total})..."),
        );
    };
    let count = if request.in_place {
        archive::extract_with_progress(
            request.source,
            request.destination,
            &mut extraction_progress,
        )?
    } else {
        archive::extract_atomic_with_progress(
            request.source,
            request.destination,
            request.preserve_user_data,
            &mut extraction_progress,
        )?
    };

    progress(0.80, "Finalizing application installation...");
    backend.after_install(request.destination, &installed_entries)?;
    progress(1.0, "Installation complete");
    Ok(count)
}

#[cfg(test)]
mod tests {
    use std::cell::Cell;

    use tempfile::tempdir;

    use super::*;

    struct RecordingBackend {
        before_called: Cell<bool>,
        after_called: Cell<bool>,
    }

    impl InstallBackend for RecordingBackend {
        fn before_install(&self, _destination: &Path) -> Result<()> {
            self.before_called.set(true);
            Ok(())
        }

        fn after_install(&self, _destination: &Path, entries: &[String]) -> Result<()> {
            assert_eq!(entries, &["launcher.exe"]);
            self.after_called.set(true);
            Ok(())
        }
    }

    #[test]
    fn generic_engine_extracts_payload_and_runs_backend_hooks() {
        let directory = tempdir().unwrap();
        let source_dir = directory.path().join("source");
        let payload = directory.path().join("payload.zip");
        let destination = directory.path().join("installed");
        std::fs::create_dir_all(&source_dir).unwrap();
        std::fs::write(source_dir.join("launcher.exe"), b"launcher").unwrap();
        crate::archive::create_zip(&source_dir, &payload).unwrap();
        let source = PayloadSource::from_path(&payload).unwrap();
        let backend = RecordingBackend {
            before_called: Cell::new(false),
            after_called: Cell::new(false),
        };
        let mut progress = Vec::new();
        let count = install(
            InstallRequest {
                source: &source,
                destination: &destination,
                in_place: false,
                preserve_user_data: true,
            },
            &backend,
            &mut |fraction, message| progress.push((fraction, message.to_owned())),
        )
        .unwrap();

        assert_eq!(count, 1);
        assert!(destination.join("launcher.exe").is_file());
        assert!(backend.before_called.get());
        assert!(backend.after_called.get());
        assert_eq!(progress.first().map(|item| item.0), Some(0.02));
        assert_eq!(progress.last().map(|item| item.0), Some(1.0));
    }
}
