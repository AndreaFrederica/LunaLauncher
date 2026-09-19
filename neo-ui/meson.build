neo_ui_sources = files(
  'package.json',
  'pnpm-lock.yaml',
  'pnpm-workspace.yaml',
  'quasar.config.ts',
  'index.html',
  'src/App.vue',
  'src/main.ts',
  'src/types.d.ts',
  'src/css/app.scss',
  'src/boot/launcher-api.ts',
  'src/router/index.ts',
  'src/pages/IndexPage.vue',
  'src-tauri/Cargo.toml',
  'src-tauri/Cargo.lock',
  'src-tauri/build.rs',
  'src-tauri/tauri.conf.json',
  'src-tauri/src/main.rs',
)

neo_ui_name = is_windows ? 'luna-neo-ui.exe' : 'luna-neo-ui'
neo_ui_target = custom_target(
  'luna-neo-ui',
  input: neo_ui_sources,
  output: neo_ui_name,
  command: [
    python_prog,
    meson.project_source_root() / 'scripts/build_neo_ui.py',
    pnpm_prog,
    cargo_prog,
    meson.current_source_dir(),
    meson.current_build_dir(),
    '@OUTPUT@',
  ],
  build_by_default: true,
  install: true,
  install_dir: binary_dest_dir,
  console: true,
)
