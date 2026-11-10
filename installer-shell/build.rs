fn main() {
    println!("cargo:rerun-if-changed=resources/luna-installer-shell.rc");
    println!("cargo:rerun-if-changed=resources/lunalauncher.ico");

    embed_resource::compile("resources/luna-installer-shell.rc", embed_resource::NONE)
        .manifest_optional()
        .unwrap();
}
