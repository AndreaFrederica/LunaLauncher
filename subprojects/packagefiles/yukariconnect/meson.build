project(
  'YukariConnect',
  version: '0.0.2',
  meson_version: '>=1.4.0',
)

dotnet = find_program('dotnet', required: true)
python = find_program('python3', 'python', required: true)

system = host_machine.system()
cpu_family = host_machine.cpu_family()

if system == 'windows'
  if cpu_family == 'x86_64'
    runtime_id = 'win-x64'
  elif cpu_family == 'aarch64'
    runtime_id = 'win-arm64'
  elif cpu_family == 'x86'
    runtime_id = 'win-x86'
  else
    error('Unsupported Windows architecture for YukariConnect: ' + cpu_family)
  endif
  executable_name = 'YukariConnect.exe'
elif system == 'linux'
  if cpu_family == 'x86_64'
    runtime_id = 'linux-x64'
  elif cpu_family == 'aarch64'
    runtime_id = 'linux-arm64'
  else
    error('Unsupported Linux architecture for YukariConnect: ' + cpu_family)
  endif
  executable_name = 'YukariConnect'
elif system == 'darwin'
  if cpu_family == 'x86_64'
    runtime_id = 'osx-x64'
  elif cpu_family == 'aarch64'
    runtime_id = 'osx-arm64'
  else
    error('Unsupported macOS architecture for YukariConnect: ' + cpu_family)
  endif
  executable_name = 'YukariConnect'
else
  error('Unsupported operating system for YukariConnect: ' + system)
endif

dotnet_configuration = get_option('buildtype') == 'debug' ? 'Debug' : 'Release'
source_root = meson.current_source_dir()
project_file = source_root / 'YukariConnect/YukariConnect.csproj'

source_list_script = (
  'import os, sys\n'
  + 'root = sys.argv[1]\n'
  + 'project = os.path.join(root, "YukariConnect")\n'
  + 'extensions = {".cs", ".csproj", ".json", ".razor", ".cshtml", ".resx", ".props", ".targets"}\n'
  + 'for path, dirs, names in os.walk(project):\n'
  + '    dirs[:] = [name for name in dirs if name not in {"bin", "obj"}]\n'
  + '    relative_dir = os.path.relpath(path, project)\n'
  + '    for name in names:\n'
  + '        if os.path.splitext(name)[1].lower() in extensions or relative_dir == "wwwroot" or relative_dir.startswith("wwwroot" + os.sep):\n'
  + '            print(os.path.relpath(os.path.join(path, name), root))\n'
)

source_paths = run_command(
  python,
  '-c',
  source_list_script,
  source_root,
  check: true,
).stdout().strip().split('\n')

yukari_connect_exe = custom_target(
  'YukariConnect',
  input: files(source_paths),
  output: executable_name,
  command: [
    dotnet,
    'publish',
    project_file,
    '-c', dotnet_configuration,
    '-r', runtime_id,
    '--self-contained', 'true',
    '-o', meson.current_build_dir(),
    '--nologo',
    '-v', 'minimal',
    '-p:PublishAot=true',
    '-p:PublishSingleFile=true',
    '-p:DebugSymbols=false',
    '-p:DebugType=None',
  ],
  build_by_default: true,
  install: true,
  install_dir: get_option('install_dir'),
  install_tag: 'yukariconnect',
  console: true,
)

install_data(
  'LICENSE',
  install_dir: get_option('install_dir'),
  rename: 'LICENSE.MPL-2.0.txt',
  install_tag: 'yukariconnect',
)
