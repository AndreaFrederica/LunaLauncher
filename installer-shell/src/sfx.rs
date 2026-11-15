use std::fs::File;
use std::io::{self, Read, Seek, SeekFrom, Take, Write};
use std::path::Path;

use anyhow::{Context, Result, bail};
use sha2::{Digest, Sha256};

pub const FOOTER_SIZE: usize = 64;
const MAGIC: &[u8; 8] = b"LUNASFX\0";
const VERSION: u32 = 1;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Footer {
    pub payload_offset: u64,
    pub payload_size: u64,
    pub payload_sha256: [u8; 32],
}

impl Footer {
    pub fn encode(self) -> [u8; FOOTER_SIZE] {
        let mut bytes = [0u8; FOOTER_SIZE];
        bytes[0..8].copy_from_slice(MAGIC);
        bytes[8..12].copy_from_slice(&VERSION.to_le_bytes());
        bytes[12..20].copy_from_slice(&self.payload_offset.to_le_bytes());
        bytes[20..28].copy_from_slice(&self.payload_size.to_le_bytes());
        bytes[28..60].copy_from_slice(&self.payload_sha256);
        bytes[60..64].copy_from_slice(&(FOOTER_SIZE as u32).to_le_bytes());
        bytes
    }

    pub fn decode(bytes: &[u8]) -> Result<Self> {
        if bytes.len() != FOOTER_SIZE {
            bail!("invalid SFX footer size: {}", bytes.len());
        }
        if &bytes[0..8] != MAGIC {
            bail!("SFX footer magic not found");
        }
        let version = u32::from_le_bytes(bytes[8..12].try_into().unwrap());
        if version != VERSION {
            bail!("unsupported SFX footer version: {version}");
        }
        let footer_size = u32::from_le_bytes(bytes[60..64].try_into().unwrap());
        if footer_size as usize != FOOTER_SIZE {
            bail!("invalid SFX footer length: {footer_size}");
        }
        Ok(Self {
            payload_offset: u64::from_le_bytes(bytes[12..20].try_into().unwrap()),
            payload_size: u64::from_le_bytes(bytes[20..28].try_into().unwrap()),
            payload_sha256: bytes[28..60].try_into().unwrap(),
        })
    }
}

#[derive(Debug, Clone)]
pub struct PayloadSource {
    pub path: std::path::PathBuf,
    pub footer: Option<Footer>,
}

impl PayloadSource {
    pub fn from_path(path: impl AsRef<Path>) -> Result<Self> {
        let path = path.as_ref().to_path_buf();
        let mut file =
            File::open(&path).with_context(|| format!("open payload: {}", path.display()))?;
        let length = file.metadata()?.len();
        let footer = find_footer(&mut file, length)?;
        if let Some(footer) = footer {
            validate_range(length, footer)?;
            Ok(Self {
                path,
                footer: Some(footer),
            })
        } else {
            Ok(Self { path, footer: None })
        }
    }

    pub fn payload_range(&self) -> Result<(u64, u64)> {
        let length = std::fs::metadata(&self.path)?.len();
        match self.footer {
            Some(footer) => {
                validate_range(length, footer)?;
                Ok((footer.payload_offset, footer.payload_size))
            }
            None => Ok((0, length)),
        }
    }

    pub fn open_reader(&self) -> Result<Take<File>> {
        let (offset, size) = self.payload_range()?;
        let mut file = File::open(&self.path)?;
        file.seek(SeekFrom::Start(offset))?;
        Ok(file.take(size))
    }

    pub fn verify_hash(&self) -> Result<()> {
        let Some(footer) = self.footer else {
            return Ok(());
        };
        let mut reader = self.open_reader()?;
        let mut hasher = Sha256::new();
        io::copy(&mut reader, &mut hasher)?;
        let actual: [u8; 32] = hasher.finalize().into();
        if actual != footer.payload_sha256 {
            bail!("SFX payload SHA-256 mismatch");
        }
        Ok(())
    }
}

pub fn append_payload(stub: &Path, payload: &Path, output: &Path) -> Result<Footer> {
    let output = output.to_path_buf();
    if paths_equal(&output, stub) || paths_equal(&output, payload) {
        bail!("SFX output must be different from stub and payload");
    }
    let payload_size = std::fs::metadata(payload)?.len();
    let stub_size = std::fs::metadata(stub)?.len();
    let payload_offset = stub_size + FOOTER_SIZE as u64;
    let payload_sha256 = hash_file(payload)?;
    let footer = Footer {
        payload_offset,
        payload_size,
        payload_sha256,
    };
    let mut output_file = File::create(&output)
        .with_context(|| format!("create SFX output: {}", output.display()))?;
    let mut stub_file = File::open(stub)?;
    io::copy(&mut stub_file, &mut output_file)?;
    output_file.write_all(&footer.encode())?;
    let mut payload_file = File::open(payload)?;
    let mut buffer = vec![0u8; 1024 * 1024];
    loop {
        let read = payload_file.read(&mut buffer)?;
        if read == 0 {
            break;
        }
        output_file.write_all(&buffer[..read])?;
    }
    output_file.flush()?;
    Ok(footer)
}

fn paths_equal(left: &Path, right: &Path) -> bool {
    let left = left.canonicalize().unwrap_or_else(|_| left.to_path_buf());
    let right = right.canonicalize().unwrap_or_else(|_| right.to_path_buf());
    left == right
}

fn hash_file(path: &Path) -> Result<[u8; 32]> {
    let mut file = File::open(path)?;
    let mut hasher = Sha256::new();
    let mut buffer = vec![0u8; 1024 * 1024];
    loop {
        let read = file.read(&mut buffer)?;
        if read == 0 {
            break;
        }
        hasher.update(&buffer[..read]);
    }
    Ok(hasher.finalize().into())
}

fn find_footer(file: &mut File, file_size: u64) -> Result<Option<Footer>> {
    if file_size < FOOTER_SIZE as u64 {
        return Ok(None);
    }
    // The footer is placed immediately before the ZIP payload. Limit the
    // probe so opening a normal large ZIP does not scan the entire archive.
    const MAX_PROBE: u64 = 16 * 1024 * 1024;
    let probe_size = file_size.min(MAX_PROBE) as usize;
    file.seek(SeekFrom::Start(0))?;
    let mut bytes = vec![0u8; probe_size];
    file.read_exact(&mut bytes)?;
    for index in 0..=bytes.len().saturating_sub(FOOTER_SIZE) {
        if &bytes[index..index + 8] != MAGIC {
            continue;
        }
        let Ok(footer) = Footer::decode(&bytes[index..index + FOOTER_SIZE]) else {
            continue;
        };
        if footer.payload_offset != index as u64 + FOOTER_SIZE as u64 {
            continue;
        }
        if validate_range(file_size, footer).is_ok() {
            return Ok(Some(footer));
        }
    }
    Ok(None)
}

fn validate_range(file_size: u64, footer: Footer) -> Result<()> {
    if footer.payload_offset > file_size
        || footer.payload_size > file_size.saturating_sub(footer.payload_offset)
        || footer.payload_offset.saturating_add(footer.payload_size) > file_size
    {
        bail!("SFX payload range is outside the file");
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::tempdir;

    #[test]
    fn footer_round_trips() {
        let footer = Footer {
            payload_offset: 123,
            payload_size: 456,
            payload_sha256: [0xabu8; 32],
        };
        assert_eq!(Footer::decode(&footer.encode()).unwrap(), footer);
    }

    #[test]
    fn appends_and_reads_payload() {
        let directory = tempdir().unwrap();
        let stub = directory.path().join("stub.bin");
        let payload = directory.path().join("payload.zip");
        let output = directory.path().join("setup.bin");
        std::fs::write(&stub, b"stub").unwrap();
        std::fs::write(&payload, b"payload bytes").unwrap();

        let footer = append_payload(&stub, &payload, &output).unwrap();
        let source = PayloadSource::from_path(&output).unwrap();
        assert_eq!(source.footer, Some(footer));
        source.verify_hash().unwrap();
        let mut bytes = Vec::new();
        source
            .open_reader()
            .unwrap()
            .read_to_end(&mut bytes)
            .unwrap();
        assert_eq!(bytes, b"payload bytes");
    }
}
