project(
  'windows-msvc',
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

lib = shared_library(
  'demo',
  'src/demo.cpp',
  include_directories: inc,
  install: true,
)

demo_dep = declare_dependency(
  include_directories: inc,
  link_with: lib,
)

app = executable(
  'demo-app',
  'src/main.cpp',
  dependencies: demo_dep,
  install: true,
)

test('demo-app', app)
