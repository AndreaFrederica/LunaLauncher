project(
  'executable-with-tests',
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
core = static_library(
  'core',
  'src/core.cpp',
  include_directories: inc,
)

app = executable(
  'example-app',
  'src/main.cpp',
  include_directories: inc,
  link_with: core,
  install: true,
)

test_exe = executable(
  'core-test',
  'tests/test_core.cpp',
  include_directories: inc,
  link_with: core,
)
test('core', test_exe)
test('app-smoke', app, args: ['--version'], should_fail: false,
  suite: 'install')

# Public API headers (visible in --dry-run)
install_headers('include/example/core.hpp', subdir: 'example')
install_data('LICENSE.md', install_dir: 'share/licenses/executable-with-tests')
