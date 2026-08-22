# PCL.Download - .NET download module
# Requires: .NET 10.0+ SDK (dotnet CLI)
#
# Compiles the C# project via NativeAOT into a native shared library:
#   Windows: PCL.Download.dll
#   Linux:   libPCL.Download.so
#   macOS:   libPCL.Download.dylib
# The launcher loads it at runtime through PclDownloadLibrary (QLibrary).

dotnet_prog = find_program('dotnet', required: false)

if not dotnet_prog.found()
  warning('dotnet SDK not found. PCL.Download will not be built.')
  pcl_download_lib = disabler()
else
  # Determine the .NET Runtime Identifier (RID) for the host machine
  if is_windows
    if host_machine.cpu_family() == 'x86_64'
      dotnet_rid = 'win-x64'
    elif host_machine.cpu_family() == 'aarch64'
      dotnet_rid = 'win-arm64'
    else
      dotnet_rid = 'win-x86'
    endif
    pcl_download_lib_name = 'PCL.Download.dll'
  elif is_linux
    if host_machine.cpu_family() == 'x86_64'
      dotnet_rid = 'linux-x64'
    elif host_machine.cpu_family() == 'aarch64'
      dotnet_rid = 'linux-arm64'
    elif host_machine.cpu_family() == 'arm'
      dotnet_rid = 'linux-arm'
    else
      error('Unsupported Linux architecture for PCL.Download: ' + host_machine.cpu_family())
    endif
    pcl_download_lib_name = 'libPCL.Download.so'
  else
    # macOS
    if host_machine.cpu_family() == 'x86_64'
      dotnet_rid = 'osx-x64'
    else
      dotnet_rid = 'osx-arm64'
    endif
    pcl_download_lib_name = 'libPCL.Download.dylib'
  endif

  # Map meson buildtype to .NET configuration
  meson_buildtype = get_option('buildtype')
  if meson_buildtype == 'debug'
    dotnet_config = 'Debug'
  else
    dotnet_config = 'Release'
  endif

  pcl_download_source_root = meson.current_source_dir()

  # Collect all source files for dependency tracking
  pcl_download_sources = run_command(
    python_prog,
    '-c',
    'import os; root = r"@0@"; [print(os.path.relpath(os.path.join(dp, f), root)) for dp, dn, fn in os.walk(root) for f in fn if f.endswith((".cs", ".csproj")) and "bin" not in dp and "obj" not in dp]'.format(pcl_download_source_root),
    check: true,
  ).stdout().strip().split('\n')

  # NativeAOT publish: dotnet writes the shared library directly into the
  # current build dir so that it matches the custom_target output.
  pcl_download_lib = custom_target(
    'PCL.Download',
    input: pcl_download_sources,
    output: pcl_download_lib_name,
    command: [
      dotnet_prog,
      'publish',
      pcl_download_source_root / 'PCL.Download.csproj',
      '-c', dotnet_config,
      '-r', dotnet_rid,
      '--self-contained', 'true',
      '-o', meson.current_build_dir(),
      '--nologo',
      '-v', 'quiet',
    ],
    install: true,
    install_dir: library_dest_dir,
    console: true,
  )

  # Convenience target for manual rebuilds
  run_target(
    'pcl-download-rebuild',
    command: [
      dotnet_prog,
      'publish',
      pcl_download_source_root / 'PCL.Download.csproj',
      '-c', dotnet_config,
      '-r', dotnet_rid,
      '--self-contained', 'true',
      '--nologo',
    ],
  )
endif
