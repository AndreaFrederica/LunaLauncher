project(
  'tinydep',
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


inc = include_directories('.')

tinydep_lib = library(
  'tinydep',
  'tinydep.cpp',
  include_directories: inc,
  install: false,
)

tinydep_dep = declare_dependency(
  include_directories: inc,
  link_with: tinydep_lib,
)
