curseforge_webview_source_root = meson.current_source_dir()
curseforge_webview_profile = get_option('buildtype') == 'debug' ? 'debug' : 'release'
curseforge_webview_name = is_windows ? 'luna-cf-webview.exe' : 'luna-cf-webview'

curseforge_webview_sources = files(
  'Cargo.toml',
  'Cargo.lock',
  'build.rs',
  'tauri.conf.json',
  'assets/index.html',
  'src/main.rs',
  '../../program_info/lunalauncher.ico',
)

curseforge_webview = custom_target(
  'luna-cf-webview',
  input: curseforge_webview_sources,
  output: curseforge_webview_name,
  command: [
    python_prog,
    meson.project_source_root() / 'scripts/build_cargo_sidecar.py',
    cargo_prog,
    curseforge_webview_source_root / 'Cargo.toml',
    meson.current_build_dir() / 'cargo-target',
    curseforge_webview_profile,
    '@OUTPUT@',
  ],
  build_by_default: true,
  install: true,
  install_dir: binary_dest_dir,
  console: true,
)
