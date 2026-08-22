project(
  'LunaLauncher',
  ['c', 'cpp', 'java'],
  version: '11.0.2',
  meson_version: '>=1.4.0',
  default_options: [
    'buildtype=release',
    'b_ndebug=if-release',
    'b_vscrt=md',
    'c_std=c11',
    'cpp_std=c++20',
    'warning_level=0',
  ],
)

host_system = host_machine.system()
is_windows = host_system == 'windows'
is_linux = host_system == 'linux'
is_macos = host_system == 'darwin'

if not is_windows and not is_linux and not is_macos
  error('This Meson build supports Windows, Linux, and macOS targets.')
endif

if meson.project_source_root() == meson.project_build_root()
  error('In-source Meson builds are not supported.')
endif

qt = import('qt6')
if is_windows
  windows = import('windows')
endif

git_prog = find_program('git', required: false)
python_prog = find_program('python', required: true)

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

if cpp.get_argument_syntax() == 'msvc'
  add_project_arguments(
    [
      '-DUNICODE',
      '-D_UNICODE',
      '/permissive-',
      '/Zc:__cplusplus',
      '/Zc:preprocessor',
      '/utf-8',
      '/W4',
      '/wd4100',
    ],
    language: 'cpp',
  )
  add_project_link_arguments(['/STACK:8388608'], language: ['c', 'cpp'])
else
  add_project_arguments(
    [
      '-fstack-protector-strong',
      '-DQT_NO_DEBUG',
    ],
    language: ['c', 'cpp'],
  )
endif

add_project_arguments(
  [
    '-DQT_WARN_DEPRECATED_UP_TO=0x060400',
    '-DQT_DISABLE_DEPRECATED_UP_TO=0x060400',
  ],
  language: 'cpp',
)

qt_core_dep = dependency('Qt6Core', method: 'cmake', required: true)
qt_widgets_dep = dependency('Qt6Widgets', method: 'cmake', required: true)
qt_network_dep = dependency('Qt6Network', method: 'cmake', required: true)
qt_networkauth_dep = dependency('Qt6NetworkAuth', method: 'cmake', required: true)
qt_concurrent_dep = dependency('Qt6Concurrent', method: 'cmake', required: true)
qt_websockets_dep = dependency('Qt6WebSockets', method: 'cmake', required: true)
qt_xml_dep = dependency('Qt6Xml', method: 'cmake', required: true)
qt_gui_dep = dependency('Qt6Gui', method: 'cmake', required: true)
qt_test_dep = dependency('Qt6Test', method: 'cmake', required: true)
qt_opengl_dep = dependency('Qt6OpenGL', method: 'cmake', required: true)
qt_multimedia_dep = dependency('Qt6Multimedia', method: 'cmake', required: true)
qt_dbus_dep = dependency('Qt6DBus', method: 'cmake', required: false)
if is_windows
  qt_entrypoint_dep = cpp.find_library(
    'Qt6EntryPoint',
    dirs: [meson.project_source_root() / 'third_party/qt/6.10.1/msvc2022_64/lib'],
    required: true,
  )
else
  qt_entrypoint_dep = dependency('', required: false)
endif

zlib_dep = dependency('zlib', fallback: ['zlib', 'zlib_dep'], required: true)
libarchive_dep = dependency('libarchive', fallback: ['libarchive', 'libarchive_dep'], required: true)
libqrencode_dep = dependency('libqrencode', fallback: ['qrencode', 'qrencode_dep'], required: true)
tomlplusplus_dep = dependency('tomlplusplus', fallback: ['tomlplusplus', 'tomlplusplus_dep'], required: true)

threads_dep = dependency('threads', required: false)
dl_dep = dependency('dl', required: false)
math_dep = cc.find_library('m', required: false)
gamemode_dep = dependency('gamemode', required: is_linux ? get_option('gamemode') : false)

launcher_name = 'LunaLauncher'
launcher_common_name = 'LunaLauncher'
launcher_display_name = 'Luna Launcher'
launcher_app_binary_name = 'lunalauncher'
launcher_app_id = 'org.lunalauncher.LunaLauncher'
launcher_svg_file_name = 'org.lunalauncher.LunaLauncher.svg'
launcher_env_name = 'LUNALAUNCHER'
launcher_domain = 'lunalauncher.org'
launcher_config_file = 'lunalauncher.cfg'
launcher_git_url = 'https://github.com/AndreaFrederica/LunaLauncher'
launcher_copyright = '© 2025 Luna Launcher Contributors\\n© 2022-2025 Prism Launcher Contributors\\n© 2021-2022 PolyMC Contributors\\n© 2012-2021 MultiMC Contributors'
launcher_version_major = 11
launcher_version_minor = 0
launcher_version_patch = 2
launcher_version_prerelease = ''
launcher_version_name = '@0@.@1@.@2@'.format(launcher_version_major, launcher_version_minor, launcher_version_patch)
launcher_version_name4 = '@0@.@1@.@2@.0'.format(launcher_version_major, launcher_version_minor, launcher_version_patch)
launcher_version_name4_comma = '@0@,@1@,@2@,0'.format(launcher_version_major, launcher_version_minor, launcher_version_patch)
launcher_build_platform = '@0@-@1@'.format(host_machine.system(), host_machine.cpu_family())
launcher_updater_github_repo = 'https://github.com/AndreaFrederica/LunaLauncher'
launcher_build_artifact = ''
launcher_news_rss_url = 'https://prismlauncher.org/feed/feed.xml'
launcher_news_open_url = 'https://prismlauncher.org/news'
launcher_wiki_url = 'https://prismlauncher.org/wiki/'
launcher_help_url = 'https://prismlauncher.org/wiki/help-pages/%1'
launcher_login_callback_url = 'https://lunalauncher.sirrus.cc/oauth-callback.html'
launcher_legacy_fmllibs_base_url = 'https://files.prismlauncher.org/fmllibs/'
launcher_meta_url = 'https://meta.prismlauncher.org/v1/'
launcher_imgur_client_id = '5b97b0713fba4a3'
launcher_bug_tracker_url = 'https://github.com/AndreaFrederica/LunaLauncher/issues'
launcher_translations_url = 'https://hosted.weblate.org/projects/prismlauncher/launcher/'
launcher_translation_files_url = 'https://i18n.prismlauncher.org/'
launcher_matrix_url = 'https://prismlauncher.org/matrix'
launcher_discord_url = 'https://prismlauncher.org/discord'
launcher_subreddit_url = 'https://prismlauncher.org/reddit'
launcher_user_agent = '@0@/@1@'.format(launcher_common_name, launcher_version_name)
launcher_glfw_library_name = 'glfw.dll'
launcher_openal_library_name = 'OpenAL.dll'
launcher_msa_client_id = 'a60a8e54-8c3c-4b25-9fe4-8ec4e616c410'
launcher_curseforge_api_key = '$2a$10$wuAJuNZuted3NORVmpgUC.m8sI.pv1tOPKZyBgLFGjxFp/br0lZCC'
launcher_enable_java_downloader = get_option('enable_java_downloader')
launcher_enable_pcl_download = get_option('build_pcl_download')
launcher_enable_yukari_connect = get_option('build_yukari_connect')
launcher_disable_ownership_check = get_option('disable_ownership_check')
launcher_bundle_aria2c = get_option('bundle_aria2c')
launcher_aria2c_path = get_option('aria2c_path')

git_commit = ''
git_tag = 'GIT-NOTFOUND'
git_refspec = 'refs/heads/stable'
if git_prog.found()
  git_commit_result = run_command(git_prog, 'rev-parse', 'HEAD', check: false)
  if git_commit_result.returncode() == 0
    git_commit = git_commit_result.stdout().strip()
  endif

  git_tag_result = run_command(git_prog, 'describe', '--tags', '--exact-match', check: false)
  if git_tag_result.returncode() == 0
    git_tag = git_tag_result.stdout().strip()
  endif

  git_refspec_result = run_command(git_prog, 'symbolic-ref', 'HEAD', check: false)
  if git_refspec_result.returncode() == 0
    git_refspec = git_refspec_result.stdout().strip()
  elif git_commit != ''
    git_refspec = git_commit.substring(0, 8)
  endif
endif

build_date = run_command(
  python_prog,
  '-c',
  'import datetime; print(datetime.date.today().isoformat())',
  check: true,
).stdout().strip()

compiler_name = cpp.get_id()
compiler_version = cpp.version()
compiler_target_system = host_machine.system()
compiler_target_system_version = host_machine.kernel()
compiler_target_processor = host_machine.cpu_family()

if is_linux
  binary_dest_dir = 'bin'
  exresource_dest_dir = join_paths('share', launcher_name, 'resources')
  library_dest_dir = '.'
  plugin_dest_dir = '.'
  resources_dest_dir = '.'
  jars_dest_dir = 'jars'
  framework_dest_dir = '.'
elif is_macos
  app_bundle_dir = launcher_name + '.app/Contents'
  binary_dest_dir = join_paths(app_bundle_dir, 'MacOS')
  exresource_dest_dir = join_paths(app_bundle_dir, 'Resources')
  library_dest_dir = join_paths(app_bundle_dir, 'MacOS')
  plugin_dest_dir = join_paths(app_bundle_dir, 'MacOS')
  resources_dest_dir = join_paths(app_bundle_dir, 'Resources')
  jars_dest_dir = join_paths(app_bundle_dir, 'MacOS', 'jars')
  framework_dest_dir = join_paths(app_bundle_dir, 'Frameworks')
else
  binary_dest_dir = '.'
  exresource_dest_dir = 'resource'
  library_dest_dir = '.'
  plugin_dest_dir = '.'
  resources_dest_dir = '.'
  jars_dest_dir = 'jars'
  framework_dest_dir = '.'
endif

subdir('program_info')
subdir('buildconfig')
subdir('libraries/cmark')
subdir('libraries')
subdir('launcher')

if launcher_enable_pcl_download
  subdir('libraries/PCL.Download')

  # Copy the native library next to the launcher executable in the build tree,
  # so the loader finds it when running from the build directory.
  # (If dotnet is missing, pcl_download_lib is a disabler and this target is
  # silently disabled as well.)
  custom_target(
    'pcl-download-deploy',
    input: pcl_download_lib,
    output: 'pcl_download_deploy.stamp',
    command: [
      python_prog,
      '-c',
      'import shutil, sys, pathlib; shutil.copy2(sys.argv[1], sys.argv[2]); pathlib.Path(sys.argv[3]).write_text("ok")',
      '@INPUT@',
      meson.current_build_dir() / 'launcher',
      '@OUTPUT@',
    ],
    build_by_default: true,
  )
endif

if launcher_enable_yukari_connect
  yukari_connect_install_dir = (
    is_macos
    ? join_paths(resources_dest_dir, 'tools', 'yukariconnect')
    : join_paths('tools', 'yukariconnect')
  )
  yukari_connect_project = subproject(
    'yukariconnect',
    default_options: ['install_dir=' + yukari_connect_install_dir],
  )
endif

if get_option('build_testing')
  subdir('tests')
endif
