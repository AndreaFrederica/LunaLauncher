project(
  'reusable-library',
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
  'mylib',
  'src/mylib.cpp',
  include_directories: inc,
  install: true,
)

mylib_dep = declare_dependency(
  include_directories: inc,
  link_with: lib,
)

pkg = import('pkgconfig')
pkg.generate(
  lib,
  description: 'Reusable library example',
  filebase: 'mylib',
  subdirs: 'mylib',
)

install_headers('include/mylib/mylib.hpp', subdir: 'mylib')

test_exe = executable(
  'mylib-test',
  'tests/test_mylib.cpp',
  dependencies: mylib_dep,
)
test('mylib', test_exe)
