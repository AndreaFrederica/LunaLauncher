project(
  'plugin-system',
  'c',
  'cpp',
  version: '0.1.0',
  meson_version: '>=1.7.0',
  license: '0BSD',
  license_files: ['LICENSE.md'],
  default_options: [
    'c_std=c17',
    'cpp_std=c++20',
    'warning_level=3',
  ],
)


inc = include_directories('include')
cc = meson.get_compiler('cpp')
dl_dep = cc.find_library('dl', required: false)

plugin = shared_module(
  'sample_plugin',
  'src/plugin.cpp',
  include_directories: inc,
  install: true,
  install_dir: get_option('libdir') / 'plugdemo' / 'plugins',
)

host_deps = []
if dl_dep.found()
  host_deps += dl_dep
endif

host = executable(
  'plughost',
  'src/main.cpp',
  include_directories: inc,
  dependencies: host_deps,
  install: true,
)

test('host-smoke', host, args: ['--help'])
