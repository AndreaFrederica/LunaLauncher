yggdrasil_cargo = find_program('cargo', required: get_option('yggdrasil'))
if yggdrasil_cargo.found()
  if meson.is_cross_build()
    error('The Yggdrasil sidecar currently requires a native build; use -Dyggdrasil=disabled when cross-compiling.')
  endif
  yggdrasil_sources = files(
    'Cargo.toml', 'Cargo.lock', 'LICENSE', 'build_sidecar.py',
    'crates/yggdrasil-core/Cargo.toml', 'crates/yggdrasil-core/src/lib.rs',
    'crates/yggdrasil-storage/Cargo.toml', 'crates/yggdrasil-storage/src/lib.rs',
    'crates/yggdrasil-service/Cargo.toml', 'crates/yggdrasil-service/src/lib.rs',
    'crates/yggdrasil-upstream/Cargo.toml', 'crates/yggdrasil-upstream/src/lib.rs',
    'crates/yggdrasil-federation/Cargo.toml', 'crates/yggdrasil-federation/src/lib.rs',
    'crates/yggdrasil-cache/Cargo.toml', 'crates/yggdrasil-cache/src/lib.rs',
    'crates/yggdrasil-protocol-http/Cargo.toml', 'crates/yggdrasil-protocol-http/src/lib.rs',
    'crates/yggdrasil-management-api/Cargo.toml', 'crates/yggdrasil-management-api/src/lib.rs',
    'crates/yggdrasil-management-api/src/schemas.rs',
    'crates/yggdrasil-management-api/src/launcher.rs',
    'crates/yggdrasil-server/Cargo.toml', 'crates/yggdrasil-server/src/lib.rs', 'crates/yggdrasil-server/src/main.rs',
  )
  yggdrasil_server = custom_target('yggdrasil-server',
    input: yggdrasil_sources,
    output: is_windows ? 'yggdrasil-server.exe' : 'yggdrasil-server',
    command: [python_prog, meson.current_source_dir() / 'build_sidecar.py', yggdrasil_cargo,
      meson.current_source_dir() / 'Cargo.toml', meson.current_build_dir() / 'cargo-target',
      get_option('buildtype') == 'debug' ? 'debug' : 'release', '@OUTPUT@'],
    build_by_default: true, install: true, install_dir: binary_dest_dir, console: true,
  )
  install_data('LICENSE', install_dir: get_option('datadir') / 'licenses' / 'yggdrasil')
endif
