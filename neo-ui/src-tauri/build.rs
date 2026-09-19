fn main() {
    // The frontend is generated before Cargo runs; re-embed it on every change.
    println!("cargo:rerun-if-changed=../dist/spa");
    tauri_build::build()
}
