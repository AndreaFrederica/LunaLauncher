use std::path::PathBuf;

use anyhow::Result;
use clap::Parser;
use luna_installer_shell::sfx::append_payload;

#[derive(Debug, Parser)]
#[command(
    name = "assemble-sfx",
    about = "Append a ZIP payload to the installer shell"
)]
struct Args {
    #[arg(long)]
    stub: PathBuf,
    #[arg(long)]
    payload: PathBuf,
    #[arg(long)]
    output: PathBuf,
}

fn main() -> Result<()> {
    let args = Args::parse();
    let footer = append_payload(&args.stub, &args.payload, &args.output)?;
    println!(
        "Wrote {} bytes of payload to {}",
        footer.payload_size,
        args.output.display()
    );
    Ok(())
}
