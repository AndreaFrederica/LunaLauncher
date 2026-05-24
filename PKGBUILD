pkgname=lunalauncher-git
pkgver=11.0.2.r11009.ga27c5e269
pkgrel=1
pkgdesc='A custom Minecraft launcher based on Prism Launcher'
arch=('x86_64')
url='https://github.com/AndreaFrederica/LunaLauncher'
license=('GPL-3.0-or-later')
options=('!debug')
depends=(
  'cmark'
  'gamemode'
  'hicolor-icon-theme'
  'java-runtime>=8'
  'libarchive'
  'qrencode'
  'qt6-5compat'
  'qt6-base'
  'qt6-imageformats'
  'qt6-multimedia'
  'qt6-networkauth'
  'qt6-svg'
  'qt6-websockets'
  'tomlplusplus'
  'zlib'
)
makedepends=(
  'cmake'
  'extra-cmake-modules'
  'git'
  'java-environment>=8'
  'ninja'
  'scdoc'
)
provides=('lunalauncher')
conflicts=('lunalauncher')
source=()
sha256sums=()

_repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

pkgver() {
  local major minor patch rev_count short_hash

  major="$(
    sed -nE 's/^set\(Launcher_VERSION_MAJOR ([0-9]+)\).*/\1/p' \
      "$_repo_root/CMakeLists.txt" | head -n1
  )"
  minor="$(
    sed -nE 's/^set\(Launcher_VERSION_MINOR ([0-9]+)\).*/\1/p' \
      "$_repo_root/CMakeLists.txt" | head -n1
  )"
  patch="$(
    sed -nE 's/^set\(Launcher_VERSION_PATCH ([0-9]+)\).*/\1/p' \
      "$_repo_root/CMakeLists.txt" | head -n1
  )"
  rev_count="$(git -C "$_repo_root" rev-list --count HEAD)"
  short_hash="$(git -C "$_repo_root" rev-parse --short HEAD)"

  printf '%s.%s.%s.r%s.g%s\n' "$major" "$minor" "$patch" "$rev_count" "$short_hash"
}

prepare() {
  local required_paths=(
    'libraries/libnbtplusplus/CMakeLists.txt'
    'libraries/qtermwidget/CMakeLists.txt'
    'libraries/quickjs-ng/CMakeLists.txt'
  )
  local path

  for path in "${required_paths[@]}"; do
    if [[ ! -e "$_repo_root/$path" ]]; then
      printf 'Missing submodule content: %s\n' "$path" >&2
      printf 'Run: git submodule update --init --recursive\n' >&2
      return 1
    fi
  done
}

build() {
  cmake -S "$_repo_root" -B "$srcdir/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_TESTING=OFF \
    -DENABLE_LTO=ON \
    -DLauncher_BUILD_PLATFORM=archlinux

  cmake --build "$srcdir/build"
}

package() {
  DESTDIR="$pkgdir" cmake --install "$srcdir/build"

  # quickjs is linked into the launcher at build time; these development
  # artifacts and CLI tools are not required to run LunaLauncher itself.
  rm -f \
    "$pkgdir/usr/bin/qjs" \
    "$pkgdir/usr/bin/qjsc" \
    "$pkgdir/usr/include/quickjs.h" \
    "$pkgdir/usr/include/quickjs-libc.h" \
    "$pkgdir/usr/lib/libqjs.a"

  rm -rf \
    "$pkgdir/usr/lib/cmake/quickjs" \
    "$pkgdir/usr/share/doc/quickjs"

  rmdir --ignore-fail-on-non-empty \
    "$pkgdir/usr/include" \
    "$pkgdir/usr/lib/cmake" \
    "$pkgdir/usr/lib" \
    "$pkgdir/usr/share/doc"
}
