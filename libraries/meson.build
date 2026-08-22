rainbow_inc = include_directories('rainbow/include')
localpeer_inc = include_directories('LocalPeer/include')
murmur2_inc = include_directories('murmur2', 'murmur2/src')
qdcss_inc = include_directories('qdcss/include')
simplewebp_inc = include_directories('simplewebp')
libnbt_inc = include_directories('libnbtplusplus/include', '.')
quickjs_inc = include_directories('quickjs-ng')
qtermwidget_inc = include_directories(
  'qtermwidget/lib',
  'qtermwidget/lib/util',
  'qtermwidget/lib/utf8proc',
  'qtermwidget/lib/ptyqt',
)

rainbow_lib = static_library(
  'Launcher_rainbow',
  ['rainbow/src/rainbow.cpp'],
  include_directories: rainbow_inc,
  dependencies: [qt_core_dep, qt_gui_dep],
)

rainbow_dep = declare_dependency(include_directories: rainbow_inc, link_with: rainbow_lib)

localpeer_sources = [
  'LocalPeer/src/LocalPeer.cpp',
  'LocalPeer/src/LockedFile.cpp',
  'LocalPeer/src/LockedFile.h',
  'LocalPeer/include/LocalPeer.h',
] + (is_windows ? ['LocalPeer/src/LockedFile_win.cpp'] : ['LocalPeer/src/LockedFile_unix.cpp'])

localpeer_moc_generated = qt.preprocess(
  moc_headers: files('LocalPeer/include/LocalPeer.h'),
  include_directories: localpeer_inc,
  dependencies: [qt_core_dep, qt_network_dep],
)

localpeer_lib = static_library(
  'LocalPeer',
  localpeer_sources + [localpeer_moc_generated],
  include_directories: localpeer_inc,
  dependencies: [qt_core_dep, qt_network_dep],
)

localpeer_dep = declare_dependency(include_directories: localpeer_inc, link_with: localpeer_lib)

murmur2_lib = static_library(
  'Launcher_murmur2',
  ['murmur2/src/MurmurHash2.cpp', 'murmur2/src/MurmurHash2.h'],
  include_directories: murmur2_inc,
)

murmur2_dep = declare_dependency(include_directories: murmur2_inc, link_with: murmur2_lib)

qdcss_lib = static_library(
  'qdcss',
  ['qdcss/include/qdcss.h', 'qdcss/src/qdcss.cpp'],
  include_directories: qdcss_inc,
  dependencies: [qt_core_dep],
)

qdcss_dep = declare_dependency(include_directories: qdcss_inc, link_with: qdcss_lib)

simplewebp_lib = static_library(
  'simplewebp',
  ['simplewebp/simplewebp.c', 'simplewebp/simplewebp.h'],
  include_directories: simplewebp_inc,
)

simplewebp_dep = declare_dependency(include_directories: simplewebp_inc, link_with: simplewebp_lib)

libnbt_sources = [
  'libnbtplusplus/src/endian_str.cpp',
  'libnbtplusplus/src/tag.cpp',
  'libnbtplusplus/src/tag_compound.cpp',
  'libnbtplusplus/src/tag_list.cpp',
  'libnbtplusplus/src/tag_string.cpp',
  'libnbtplusplus/src/value.cpp',
  'libnbtplusplus/src/value_initializer.cpp',
  'libnbtplusplus/src/io/stream_reader.cpp',
  'libnbtplusplus/src/io/stream_writer.cpp',
  'libnbtplusplus/src/io/izlibstream.cpp',
  'libnbtplusplus/src/io/ozlibstream.cpp',
  'libnbtplusplus/src/text/json_formatter.cpp',
]

libnbt_lib = static_library(
  'nbt++',
  libnbt_sources,
  include_directories: libnbt_inc,
  dependencies: [zlib_dep],
  cpp_args: ['-DNBT_HAVE_ZLIB'],
)

libnbt_dep = declare_dependency(include_directories: libnbt_inc, link_with: libnbt_lib, dependencies: [zlib_dep])

quickjs_lib = static_library(
  'qjs',
  [
    'quickjs-ng/cutils.c',
    'quickjs-ng/dtoa.c',
    'quickjs-ng/libregexp.c',
    'quickjs-ng/libunicode.c',
    'quickjs-ng/quickjs.c',
  ],
  include_directories: quickjs_inc,
  c_args: [
    '-D_GNU_SOURCE',
    '-DWIN32_LEAN_AND_MEAN',
    '-D_WIN32_WINNT=0x0601',
  ],
  dependencies: [math_dep, threads_dep, dl_dep],
)

quickjs_dep = declare_dependency(
  include_directories: quickjs_inc,
  link_with: quickjs_lib,
  dependencies: [math_dep, threads_dep, dl_dep],
)

subdir('source_lists')

qtermwidget_ui_generated = qt.preprocess(
  ui_files: files('qtermwidget/lib/util/SearchBar.ui'),
  qresources: files('qtermwidget/lib/res.qrc'),
)

qtermwidget_moc_generated = qt.preprocess(
  moc_headers: files(qtermwidget_moc_headers),
  moc_sources: files(qtermwidget_moc_sources),
  include_directories: [qtermwidget_inc],
  dependencies: [
    qt_core_dep,
    qt_gui_dep,
    qt_widgets_dep,
    qt_network_dep,
    qt_multimedia_dep,
  ],
  preserve_paths: true,
)

qtermwidget_sources = [
  'qtermwidget/lib/utf8proc/utf8proc.c',
  'qtermwidget/lib/util/CharWidth.cpp',
  'qtermwidget/lib/util/ColorScheme.cpp',
  'qtermwidget/lib/util/Filter.cpp',
  'qtermwidget/lib/util/History.cpp',
  'qtermwidget/lib/util/HistorySearch.cpp',
  'qtermwidget/lib/util/KeyboardTranslator.cpp',
  'qtermwidget/lib/util/SearchBar.cpp',
  'qtermwidget/lib/util/TerminalCharacterDecoder.cpp',
  'qtermwidget/lib/Emulation.cpp',
  'qtermwidget/lib/Vt102Emulation.cpp',
  'qtermwidget/lib/Screen.cpp',
  'qtermwidget/lib/ScreenWindow.cpp',
  'qtermwidget/lib/TerminalDisplay.cpp',
  'qtermwidget/lib/qtermwidget.cpp',
  'qtermwidget/lib/ptyqt/ptyqt.cpp',
]

qtermwidget_platform_deps = []
if is_windows
  qtermwidget_sources += ['qtermwidget/lib/ptyqt/conptyprocess.cpp']
  qtermwidget_platform_deps += [
    cc.find_library('Advapi32', required: true),
    cc.find_library('Crypt32', required: true),
    cc.find_library('Iphlpapi', required: true),
    cc.find_library('Netapi32', required: true),
    cc.find_library('Psapi', required: true),
    cc.find_library('Userenv', required: true),
    cc.find_library('Version', required: true),
    cc.find_library('Winmm', required: true),
    cc.find_library('Ws2_32', required: true),
  ]
elif is_linux or is_macos
  qtermwidget_sources += ['qtermwidget/lib/ptyqt/unixptyprocess.cpp']
  qtermwidget_platform_deps += [threads_dep, dl_dep]
endif

qtermwidget_lib = static_library(
  'qtermwidget',
  qtermwidget_sources + [
    qtermwidget_ui_generated,
    qtermwidget_moc_generated,
  ],
  include_directories: qtermwidget_inc,
  dependencies: [
    qt_core_dep,
    qt_gui_dep,
    qt_widgets_dep,
    qt_network_dep,
    qt_multimedia_dep,
  ] + qtermwidget_platform_deps,
)

qtermwidget_dep = declare_dependency(include_directories: qtermwidget_inc, link_with: qtermwidget_lib)

launcher_java_common_sources = [
  'launcher/org/prismlauncher/EntryPoint.java',
  'launcher/org/prismlauncher/SystemProperties.java',
  'launcher/org/prismlauncher/launcher/Launcher.java',
  'launcher/org/prismlauncher/launcher/impl/AbstractLauncher.java',
  'launcher/org/prismlauncher/launcher/impl/StandardLauncher.java',
  'launcher/org/prismlauncher/exception/ParameterNotFoundException.java',
  'launcher/org/prismlauncher/exception/ParseException.java',
  'launcher/org/prismlauncher/utils/Parameters.java',
  'launcher/org/prismlauncher/utils/ReflectionUtils.java',
  'launcher/org/prismlauncher/utils/logging/Level.java',
  'launcher/org/prismlauncher/utils/logging/Log.java',
]

launcher_java_args = [
  '-nowarn',
  '-Xdoclint:none',
  '-source',
  '8',
  '-target',
  '8',
]

newlaunch_jar = jar(
  'NewLaunch',
  launcher_java_common_sources + [
    'launcher/org/prismlauncher/legacy/LegacyProxy.java',
  ],
  main_class: 'org.prismlauncher.EntryPoint',
  java_args: launcher_java_args,
  install: true,
  install_dir: jars_dest_dir,
)

newlaunchlegacy_jar = jar(
  'NewLaunchLegacy',
  [
    'launcher/legacy/org/prismlauncher/legacy/LegacyFrame.java',
    'launcher/legacy/org/prismlauncher/legacy/LegacyLauncher.java',
    'launcher/legacy/org/prismlauncher/legacy/fix/online/Handler.java',
    'launcher/legacy/org/prismlauncher/legacy/fix/online/OnlineFixes.java',
    'launcher/legacy/org/prismlauncher/legacy/fix/online/OnlineModeFix.java',
    'launcher/legacy/org/prismlauncher/legacy/fix/online/SkinFix.java',
    'launcher/legacy/org/prismlauncher/legacy/utils/Base64.java',
    'launcher/legacy/org/prismlauncher/legacy/utils/api/MojangApi.java',
    'launcher/legacy/org/prismlauncher/legacy/utils/api/Texture.java',
    'launcher/legacy/org/prismlauncher/legacy/utils/json/JsonParseException.java',
    'launcher/legacy/org/prismlauncher/legacy/utils/json/JsonParser.java',
    'launcher/legacy/org/prismlauncher/legacy/utils/url/ByteArrayUrlConnection.java',
    'launcher/legacy/org/prismlauncher/legacy/utils/url/UrlUtils.java',
    'launcher/legacy/net/minecraft/Launcher.java',
    'launcher/legacy/org/prismlauncher/legacy/LegacyProxy.java',
  ],
  link_with: newlaunch_jar,
  main_class: 'org.prismlauncher.EntryPoint',
  java_args: launcher_java_args,
  install: true,
  install_dir: jars_dest_dir,
)

javacheck_jar = jar(
  'JavaCheck',
  ['javacheck/JavaCheck.java'],
  main_class: 'JavaCheck',
  java_args: launcher_java_args,
  install: true,
  install_dir: jars_dest_dir,
)
