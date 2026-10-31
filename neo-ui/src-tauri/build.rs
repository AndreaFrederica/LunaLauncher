use std::{fs, path::Path};

fn main() {
    // The frontend is generated before Cargo runs; re-embed it on every change.
    println!("cargo:rerun-if-changed=../dist/spa");
    let bridge = Path::new("generated/webui-bridge.js");
    println!("cargo:rerun-if-changed={}", bridge.display());
    // Keep standalone `cargo check`/`cargo test` usable without the frontend
    // build. Production builds regenerate this file from src/client/bridge.ts.
    if !bridge.exists() {
        let _ = fs::create_dir_all("generated");
        let _ = fs::write(
            bridge,
            "/* Generated bridge missing. Run `pnpm build:bridge`. */\nconsole.error('[luna] webui bridge was not generated');\n",
        );
    }
    tauri_build::build()
}
