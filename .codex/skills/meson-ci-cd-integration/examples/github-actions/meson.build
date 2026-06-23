project(
  'github-actions',
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
core = static_library('core', 'src/core.cpp', include_directories: inc)
app = executable('gha-app', 'src/main.cpp', include_directories: inc, link_with: core, install: true)

test_exe = executable('gha-test', 'tests/test_core.cpp', include_directories: inc, link_with: core)
test('gha', test_exe)
