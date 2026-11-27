use std::path::PathBuf;

use anyhow::Result;
use clap::Parser;
use luna_installer_shell::{archive::create_zip_with_version, version::probe_launcher};

#[derive(Debug, Parser)]
#[command(name = "create-zip", about = "Create a UTF-8 marked ZIP payload")]
struct Args {
    #[arg(long)]
    source: PathBuf,
    #[arg(long)]
    output: PathBuf,
}

fn main() -> Result<()> {
    let args = Args::parse();
    let launcher = args.source.join("lunalauncher.exe");
    let version = launcher.is_file().then(|| probe_launcher(&launcher)).transpose()?;
    let count = create_zip_with_version(&args.source, &args.output, version)?;
    println!("Wrote {count} entries to {}", args.output.display());
    Ok(())
}
