use std::path::PathBuf;

use anyhow::Result;
use clap::Parser;
use luna_installer_shell::archive::create_zip;

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
    let count = create_zip(&args.source, &args.output)?;
    println!("Wrote {count} entries to {}", args.output.display());
    Ok(())
}
