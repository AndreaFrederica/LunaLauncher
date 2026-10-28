project(
  'package-installation',
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

lib = library(
  'packdemo',
  'src/packdemo.cpp',
  include_directories: inc,
  version: '2.0.0',
  soversion: '2',
  install: true,
)

install_headers('include/packdemo/packdemo.hpp', subdir: 'packdemo')
install_data('data/packdemo.conf', install_dir: get_option('datadir') / 'packdemo')

pkg = import('pkgconfig')
pkg.generate(
  lib,
  description: 'Package installation example',
  filebase: 'packdemo',
  subdirs: 'packdemo',
)

packdemo_dep = declare_dependency(
  include_directories: inc,
  link_with: lib,
)

app = executable(
  'packdemo-tool',
  'src/main.cpp',
  dependencies: packdemo_dep,
  install: true,
)
test('packdemo-tool', app)
