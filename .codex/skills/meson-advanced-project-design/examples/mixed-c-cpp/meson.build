project(
  'mixed-c-cpp',
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

clib = static_library(
  'clib',
  'src/c_api.c',
  include_directories: inc,
)

app = executable(
  'mixed-app',
  'src/main.cpp',
  include_directories: inc,
  link_with: clib,
  install: true,
)

test('mixed-app', app)
