project(
  'pkg-config-export',
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
  'pkgdemo',
  'src/pkgdemo.cpp',
  include_directories: inc,
  version: '1.0.0',
  soversion: '1',
  install: true,
)

install_headers('include/pkgdemo/pkgdemo.hpp', subdir: 'pkgdemo')

pkg = import('pkgconfig')
pkg.generate(
  lib,
  description: 'Pkg-config export example',
  filebase: 'pkgdemo',
  subdirs: 'pkgdemo',
)

pkgdemo_dep = declare_dependency(
  include_directories: inc,
  link_with: lib,
)

test_exe = executable(
  'pkgdemo-test',
  'tests/test_pkgdemo.cpp',
  dependencies: pkgdemo_dep,
)
test('pkgdemo', test_exe)
