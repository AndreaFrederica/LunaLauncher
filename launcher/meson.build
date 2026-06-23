subdir('source_lists')

if not is_windows
  _filtered = []
  foreach src : launcher_logic_sources
    if src != 'console/WindowsConsole.cpp' and src != 'console/WindowsConsole.h'
      _filtered += [src]
    endif
  endforeach
  launcher_logic_sources = _filtered
endif

launcher_inc = include_directories('.', 'include')
launcher_minecraft_inc = include_directories('minecraft')

auth_logging_conf = configuration_data()
auth_logging_conf.set('HEADER_NAME', 'Logging.h')
auth_logging_conf.set('IDENTIFIER', 'authCredentials')
auth_logging_conf.set('ARG_CATEGORY_NAME', 'launcher.auth.credentials')
auth_logging_conf.set('ARG_DEFAULT_SEVERITY', 'Warning')
auth_logging_conf.set('GUARD_NAME', 'LUNALAUNCHER_LOGGING_H')
auth_logging_conf.set('OPEN_NAMESPACES', '')
auth_logging_conf.set('CLOSE_NAMESPACES', '')

auth_logging_header = configure_file(
  input: meson.project_source_root() / 'libraries/extra-cmake-modules/modules/ECMQtDeclareLoggingCategory.h.in',
  output: 'Logging.h',
  configuration: auth_logging_conf,
)

auth_logging_source = configure_file(
  input: meson.project_source_root() / 'libraries/extra-cmake-modules/modules/ECMQtDeclareLoggingCategory.cpp.in',
  output: 'Logging.cpp',
  configuration: auth_logging_conf,
)

launcher_ui_generated = qt.preprocess(
  ui_files: files(launcher_ui_sources),
  qresources: files(launcher_qrc_sources) + [launcher_logo_qrc_file],
)

launcher_moc_header_generated = qt.preprocess(
  moc_headers: files(launcher_moc_headers),
  include_directories: [
    launcher_inc,
    launcher_minecraft_inc,
    quickjs_inc,
    qtermwidget_inc,
    simplewebp_inc,
  ],
  dependencies: [
    buildconfig_dep,
    qt_core_dep,
    qt_widgets_dep,
    qt_network_dep,
    qt_networkauth_dep,
    qt_concurrent_dep,
    qt_websockets_dep,
    qt_xml_dep,
    qt_gui_dep,
    qt_opengl_dep,
    qtermwidget_dep,
    libcmark_dep,
    localpeer_dep,
    rainbow_dep,
    zlib_dep,
    libarchive_dep,
    tomlplusplus_dep,
    libqrencode_dep,
  ],
  preserve_paths: true,
)

launcher_moc_source_generated = qt.preprocess(
  moc_sources: files(launcher_moc_sources),
  include_directories: [
    launcher_inc,
    launcher_minecraft_inc,
    quickjs_inc,
    qtermwidget_inc,
    simplewebp_inc,
  ],
  dependencies: [
    buildconfig_dep,
    qt_core_dep,
    qt_widgets_dep,
    qt_network_dep,
    qt_networkauth_dep,
    qt_concurrent_dep,
    qt_websockets_dep,
    qt_xml_dep,
    qt_gui_dep,
    qt_opengl_dep,
    qtermwidget_dep,
    libcmark_dep,
    localpeer_dep,
    rainbow_dep,
    zlib_dep,
    libarchive_dep,
    tomlplusplus_dep,
    libqrencode_dep,
  ],
)

launcher_logic_args = ['-DLAUNCHER_APPLICATION']
if launcher_disable_ownership_check
  launcher_logic_args += ['-DLAUNCHER_DISABLE_OWNERSHIP_CHECK']
endif
if gamemode_dep.found()
  launcher_logic_args += ['-DWITH_GAMEMODE']
endif

launcher_logic = static_library(
  'Launcher_logic',
  files(launcher_logic_sources) + [
    auth_logging_header,
    auth_logging_source,
    launcher_ui_generated,
    launcher_moc_header_generated,
    launcher_moc_source_generated,
  ],
  dependencies: [
    murmur2_dep,
    libnbt_dep,
    qdcss_dep,
    simplewebp_dep,
    quickjs_dep,
    buildconfig_dep,
    qt_core_dep,
    qt_widgets_dep,
    qt_network_dep,
    qt_networkauth_dep,
    qt_concurrent_dep,
    qt_websockets_dep,
    qt_xml_dep,
    qt_gui_dep,
    qt_opengl_dep,
    qtermwidget_dep,
    libcmark_dep,
    localpeer_dep,
    rainbow_dep,
    zlib_dep,
    libarchive_dep,
    tomlplusplus_dep,
    libqrencode_dep,
    gamemode_dep,
  ],
  include_directories: [
    launcher_inc,
    launcher_minecraft_inc,
    quickjs_inc,
    qtermwidget_inc,
    simplewebp_inc,
  ],
  cpp_args: launcher_logic_args,
  cpp_pch: get_option('use_pch') ? ['include/base.pch.hpp', 'include/qtcore.pch.hpp', 'include/qtgui.pch.hpp'] : [],
)

launcher_main_sources = ['main.cpp']
if is_windows
  launcher_main_sources += windows.compile_resources(launcher_windows_rc_file)
endif

if is_windows
  launcher_exe = executable(
    launcher_app_binary_name,
    launcher_main_sources,
    link_with: launcher_logic,
    include_directories: [
      launcher_inc,
      launcher_minecraft_inc,
    ],
    dependencies: [
      buildconfig_dep,
      qt_core_dep,
      qt_gui_dep,
      qt_widgets_dep,
      qt_network_dep,
      qt_entrypoint_dep,
    ],
    install: true,
    install_dir: binary_dest_dir,
    win_subsystem: 'windows',
    name_suffix: 'exe',
  )
else
  launcher_exe = executable(
    launcher_app_binary_name,
    launcher_main_sources,
    link_with: launcher_logic,
    include_directories: [
      launcher_inc,
      launcher_minecraft_inc,
    ],
    dependencies: [
      buildconfig_dep,
      qt_core_dep,
      qt_gui_dep,
      qt_widgets_dep,
      qt_network_dep,
    ],
    install: true,
    install_dir: binary_dest_dir,
  )
endif

if is_windows and get_option('build_filelinker')
  filelink_generated = qt.preprocess(
    moc_headers: files(filelink_moc_headers + ['FileSystem.h']),
    moc_sources: files(filelink_moc_sources),
    include_directories: [launcher_inc],
    dependencies: [
      buildconfig_dep,
      qt_core_dep,
      qt_gui_dep,
      qt_widgets_dep,
      qt_network_dep,
    ],
    preserve_paths: true,
  )

  filelink_logic = static_library(
    'filelink_logic',
    [
      'console/WindowsConsole.cpp',
      'console/WindowsConsole.h',
      'filelink/FileLink.cpp',
      'filelink/FileLink.h',
      'FileSystem.cpp',
      'FileSystem.h',
      'Exception.h',
      'StringUtils.cpp',
      'StringUtils.h',
      'DesktopServices.cpp',
      'DesktopServices.h',
      filelink_generated,
    ],
    include_directories: [launcher_inc],
    dependencies: [
      buildconfig_dep,
      qt_core_dep,
      qt_gui_dep,
      qt_widgets_dep,
      qt_network_dep,
      qt_entrypoint_dep,
    ],
    cpp_pch: get_option('use_pch') ? ['include/base.pch.hpp', 'include/qtcore.pch.hpp', 'include/qtgui.pch.hpp'] : [],
  )

  executable(
    'lunalauncher_filelink',
    ['filelink/filelink_main.cpp'],
    link_with: filelink_logic,
    include_directories: [launcher_inc],
    dependencies: [
      buildconfig_dep,
      qt_core_dep,
      qt_gui_dep,
      qt_widgets_dep,
      qt_network_dep,
      qt_entrypoint_dep,
    ],
    install: true,
    install_dir: binary_dest_dir,
    win_subsystem: 'windows',
    name_suffix: 'exe',
  )
endif

install_data('qtlogging.ini', install_dir: resources_dest_dir)

if launcher_bundle_aria2c and launcher_aria2c_path != ''
  install_data(launcher_aria2c_path, install_dir: 'tools/aria2')
endif

install_data('translations/zh.po', install_dir: join_paths(exresource_dest_dir, 'translations'))
install_subdir('exresources/themes', install_dir: exresource_dest_dir)
install_subdir('exresources/iconthemes', install_dir: exresource_dest_dir)
install_subdir('exresources/translations', install_dir: exresource_dest_dir)
install_subdir('exresources/catpacks', install_dir: exresource_dest_dir)
install_subdir('exresources/icons', install_dir: exresource_dest_dir)
install_subdir('exresources/fonts', install_dir: exresource_dest_dir)

install_data(
  [
    'modplatform/jsapi/modrinth_plugin_api.js',
    'modplatform/jsapi/resource-api.d.ts',
    'modplatform/jsapi/README.md',
    'modplatform/jsapi/DEPLOYMENT.md',
  ],
  install_dir: join_paths(exresource_dest_dir, 'jsapi'),
)
