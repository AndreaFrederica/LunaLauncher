project(
  'cross-compilation',
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


codegen = executable(
  'codegen',
  'tools/codegen.cpp',
  native: true,
)

generated = custom_target(
  'generated-header',
  input: 'data/input.txt',
  output: 'generated.hpp',
  command: [codegen, '@INPUT@', '@OUTPUT@'],
)

app = executable(
  'cross-app',
  'src/main.cpp',
  sources: [generated],
  install: true,
)
