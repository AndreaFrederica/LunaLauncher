use std::fs;

fn main() {
    println!("cargo:rerun-if-changed=resources/luna-installer-shell.rc");
    println!("cargo:rerun-if-changed=resources/lunalauncher.ico");
    println!("cargo:rerun-if-changed=../meson.build");

    let version = fs::read_to_string("../meson.build")
        .ok()
        .and_then(|contents| {
            contents
                .split_once("version: '")
                .and_then(|(_, rest)| rest.split_once('\''))
                .map(|(version, _)| version.to_owned())
        })
        .unwrap_or_else(|| "0.1.0".to_owned());
    println!("cargo:rustc-env=LUNA_LAUNCHER_VERSION={version}");

    embed_resource::compile("resources/luna-installer-shell.rc", embed_resource::NONE)
        .manifest_optional()
        .unwrap();
}
