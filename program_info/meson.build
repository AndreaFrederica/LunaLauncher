program_info_conf = configuration_data()
program_info_conf.set('Launcher_CommonName', launcher_common_name)
program_info_conf.set('Launcher_DisplayName', launcher_display_name)
program_info_conf.set('Launcher_AppID', launcher_app_id)
program_info_conf.set('Launcher_SVGFileName', launcher_svg_file_name)
program_info_conf.set('Launcher_Copyright', launcher_copyright)
program_info_conf.set('Launcher_ENVName', launcher_env_name)
program_info_conf.set('Launcher_Domain', launcher_domain)
program_info_conf.set('Launcher_UserAgent', launcher_user_agent)
program_info_conf.set('Launcher_ConfigFile', launcher_config_file)
program_info_conf.set('Launcher_Git', launcher_git_url)
program_info_conf.set('Launcher_APP_BINARY_NAME', launcher_app_binary_name)
program_info_conf.set('Launcher_VERSION_NAME', launcher_version_name)
program_info_conf.set('Launcher_VERSION_NAME4', launcher_version_name4)
program_info_conf.set('Launcher_VERSION_NAME4_COMMA', launcher_version_name4_comma)
program_info_conf.set('Launcher_VERSION_MAJOR', launcher_version_major)
program_info_conf.set('Launcher_VERSION_MINOR', launcher_version_minor)
program_info_conf.set('Launcher_VERSION_PATCH', launcher_version_patch)
program_info_conf.set('Launcher_BUILD_TIMESTAMP', build_date)
program_info_conf.set('Launcher_Branding_ICO', 'program_info/lunalauncher.ico')
program_info_conf.set('Launcher_MSVC_Redist_NSIS_Section', '')

launcher_desktop_file = configure_file(
  input: 'org.lunalauncher.LunaLauncher.desktop.in',
  output: 'org.lunalauncher.LunaLauncher.desktop',
  configuration: program_info_conf,
  format: 'cmake@',
)

launcher_metainfo_file = configure_file(
  input: 'org.lunalauncher.LunaLauncher.metainfo.xml.in',
  output: 'org.lunalauncher.LunaLauncher.metainfo.xml',
  configuration: program_info_conf,
  format: 'cmake@',
)

launcher_windows_rc_file = configure_file(
  input: 'lunalauncher.rc.in',
  output: 'lunalauncher.rc',
  configuration: program_info_conf,
  format: 'cmake@',
)

launcher_logo_qrc_file = configure_file(
  input: 'lunalauncher.qrc.in',
  output: 'lunalauncher.qrc',
  configuration: program_info_conf,
  format: 'cmake@',
)

launcher_manifest_file = configure_file(
  input: 'lunalauncher.manifest.in',
  output: 'lunalauncher.manifest',
  configuration: program_info_conf,
  format: 'cmake@',
)

launcher_ico_file = configure_file(input: 'lunalauncher.ico', output: 'lunalauncher.ico', copy: true)
launcher_svg_file = configure_file(input: 'org.lunalauncher.LunaLauncher.svg', output: 'org.lunalauncher.LunaLauncher.svg', copy: true)
launcher_mime_file = configure_file(input: 'org.lunalauncher.LunaLauncher.mime.xml', output: 'org.lunalauncher.LunaLauncher.xml', copy: true)
