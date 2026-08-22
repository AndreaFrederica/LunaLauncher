buildconfig_conf = configuration_data()
buildconfig_conf.set('Launcher_Name', launcher_name)
buildconfig_conf.set('Launcher_APP_BINARY_NAME', launcher_app_binary_name)
buildconfig_conf.set('Launcher_DisplayName', launcher_display_name)
buildconfig_conf.set('Launcher_Copyright', launcher_copyright)
buildconfig_conf.set('Launcher_Domain', launcher_domain)
buildconfig_conf.set('Launcher_ConfigFile', launcher_config_file)
buildconfig_conf.set('Launcher_Git', launcher_git_url)
buildconfig_conf.set('Launcher_AppID', launcher_app_id)
buildconfig_conf.set('Launcher_SVGFileName', launcher_svg_file_name)
buildconfig_conf.set('Launcher_ENVName', launcher_env_name)
buildconfig_conf.set('Launcher_UserAgent', launcher_user_agent)
buildconfig_conf.set('Launcher_VERSION_MAJOR', launcher_version_major)
buildconfig_conf.set('Launcher_VERSION_MINOR', launcher_version_minor)
buildconfig_conf.set('Launcher_VERSION_PATCH', launcher_version_patch)
buildconfig_conf.set('Launcher_VERSION_PRERELEASE', launcher_version_prerelease)
buildconfig_conf.set('Launcher_BUILD_PLATFORM', launcher_build_platform)
buildconfig_conf.set('Launcher_BUILD_ARTIFACT', launcher_build_artifact)
buildconfig_conf.set('Launcher_BUILD_TIMESTAMP', build_date)
buildconfig_conf.set('Launcher_UPDATER_GITHUB_REPO', launcher_updater_github_repo)
buildconfig_conf.set('Launcher_COMPILER_NAME', compiler_name)
buildconfig_conf.set('Launcher_COMPILER_VERSION', compiler_version)
buildconfig_conf.set('Launcher_COMPILER_TARGET_SYSTEM', compiler_target_system)
buildconfig_conf.set('Launcher_COMPILER_TARGET_SYSTEM_VERSION', compiler_target_system_version)
buildconfig_conf.set('Launcher_COMPILER_TARGET_PROCESSOR', compiler_target_processor)
buildconfig_conf.set('MACOSX_SPARKLE_UPDATE_PUBLIC_KEY', '')
buildconfig_conf.set('MACOSX_SPARKLE_UPDATE_FEED_URL', '')
buildconfig_conf.set10('Launcher_ENABLE_JAVA_DOWNLOADER', launcher_enable_java_downloader)
buildconfig_conf.set10('Launcher_ENABLE_PCL_DOWNLOAD', launcher_enable_pcl_download)
buildconfig_conf.set('Launcher_GIT_COMMIT', git_commit)
buildconfig_conf.set('Launcher_GIT_TAG', git_tag)
buildconfig_conf.set('Launcher_GIT_REFSPEC', git_refspec)
buildconfig_conf.set('Launcher_NEWS_RSS_URL', launcher_news_rss_url)
buildconfig_conf.set('Launcher_NEWS_OPEN_URL', launcher_news_open_url)
buildconfig_conf.set('Launcher_WIKI_URL', launcher_wiki_url)
buildconfig_conf.set('Launcher_HELP_URL', launcher_help_url)
buildconfig_conf.set('Launcher_LOGIN_CALLBACK_URL', launcher_login_callback_url)
buildconfig_conf.set('Launcher_IMGUR_CLIENT_ID', launcher_imgur_client_id)
buildconfig_conf.set('Launcher_MSA_CLIENT_ID', launcher_msa_client_id)
buildconfig_conf.set('Launcher_CURSEFORGE_API_KEY', launcher_curseforge_api_key)
buildconfig_conf.set('Launcher_META_URL', launcher_meta_url)
buildconfig_conf.set('Launcher_LEGACY_FMLLIBS_BASE_URL', launcher_legacy_fmllibs_base_url)
buildconfig_conf.set('Launcher_GLFW_LIBRARY_NAME', launcher_glfw_library_name)
buildconfig_conf.set('Launcher_OPENAL_LIBRARY_NAME', launcher_openal_library_name)
buildconfig_conf.set('Launcher_BUG_TRACKER_URL', launcher_bug_tracker_url)
buildconfig_conf.set('Launcher_TRANSLATIONS_URL', launcher_translations_url)
buildconfig_conf.set('Launcher_TRANSLATION_FILES_URL', launcher_translation_files_url)
buildconfig_conf.set('Launcher_MATRIX_URL', launcher_matrix_url)
buildconfig_conf.set('Launcher_DISCORD_URL', launcher_discord_url)
buildconfig_conf.set('Launcher_SUBREDDIT_URL', launcher_subreddit_url)

buildconfig_cpp = configure_file(
  input: 'BuildConfig.cpp.in',
  output: 'BuildConfig.cpp',
  configuration: buildconfig_conf,
  format: 'cmake',
)

buildconfig_inc = include_directories('.')

buildconfig_lib = static_library(
  'BuildConfig',
  ['BuildConfig.h', buildconfig_cpp],
  include_directories: buildconfig_inc,
  dependencies: [qt_core_dep],
)

buildconfig_dep = declare_dependency(
  include_directories: buildconfig_inc,
  link_with: buildconfig_lib,
)
