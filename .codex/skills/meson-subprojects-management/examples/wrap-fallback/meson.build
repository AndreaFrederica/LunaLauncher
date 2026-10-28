project(
  'wrap-fallback',
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

wrap_lib = library(
  'wrapdemo',
  'src/wrapdemo.cpp',
  include_directories: inc,
)

tiny_dep = dependency('tinydep', fallback: ['tinydep', 'tinydep_dep'])

app = executable(
  'wrap-app',
  'src/main.cpp',
  include_directories: inc,
  link_with: wrap_lib,
  dependencies: tiny_dep,
  install: true,
)

# Integration test: run the app (exercises both wrapdemo and the subproject dependency)
test('wrap-app', app)
