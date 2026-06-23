project(
  'basic-build',
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

# Named targets make `meson compile -C build demo` and
# `meson compile -C build demo-app` work without guessing paths.

inc = include_directories('include')

# Target name: 'demo'  →  meson compile -C build demo
demo_lib = static_library(
  'demo',
  'src/demo.cpp',
  include_directories: inc,
)

demo_dep = declare_dependency(
  include_directories: inc,
  link_with: demo_lib,
)

# Target name: 'demo-app'  →  meson compile -C build demo-app
app = executable(
  'demo-app',
  'src/main.cpp',
  dependencies: demo_dep,
  install: true,
)

# Target name: 'demo-test'  →  meson compile -C build demo-test
test_exe = executable(
  'demo-test',
  'tests/test_demo.cpp',
  dependencies: demo_dep,
)

test('demo', test_exe)
test('app-version', app, args: ['--version'])
