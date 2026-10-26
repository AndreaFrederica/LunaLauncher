# Kvantum is linked privately: downloaded themes contain data, never plugins.
kvantum_inc = include_directories('style')
kvantum_deps = [qt_core_dep, qt_gui_dep, qt_widgets_dep, qt_svg_dep]
kvantum_defines = ['-DNO_KF', '-DNO_X11', '-DDATADIR=""']
kvantum_generated = qt.preprocess(
  moc_headers: files('style/Kvantum.h', 'style/shortcuthandler.h',
    'style/animation/animation.h', 'style/blur/blurhelper.h', 'style/drag/windowmanager.h'),
  qresources: files('style/themeconfig/defaulttheme.qrc'),
  dependencies: kvantum_deps,
  moc_extra_arguments: kvantum_defines,
)
kvantum_lib = static_library('Luna_kvantum',
  files('style/Kvantum.cpp', 'style/eventFiltering.cpp', 'style/polishing.cpp',
    'style/rendering.cpp', 'style/standardIcons.cpp', 'style/viewItems.cpp',
    'style/shortcuthandler.cpp', 'style/animation/animation.cpp',
    'style/blur/blurhelper.cpp', 'style/drag/windowmanager.cpp', 'style/themeconfig/ThemeConfig.cpp'),
  kvantum_generated,
  include_directories: kvantum_inc,
  dependencies: kvantum_deps,
  cpp_args: kvantum_defines,
)
kvantum_dep = declare_dependency(link_with: kvantum_lib,
  include_directories: kvantum_inc, dependencies: kvantum_deps,
  compile_args: ['-DNO_KF', '-DNO_X11'])
install_data('COPYING', 'README.luna.md', install_dir: resources_dest_dir / 'licenses/kvantum')
