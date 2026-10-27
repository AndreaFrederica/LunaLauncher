%global forgeurl https://github.com/W-874/LunaLauncher
%global datadir_name LunaLauncher
%global build_platform %(if [ -n "%{?fedora}" ]; then printf "fedora%s" "%{fedora}"; else printf "fedora"; fi)
%bcond_with tests
%if %{with tests}
%global meson_build_testing true
%else
%global meson_build_testing false
%endif

Name:           lunalauncher
Version:        11.1.1
Release:        1%{?dist}
Summary:        Custom Minecraft launcher based on Prism Launcher

License:        GPL-3.0-only AND CC-BY-SA-4.0
URL:            %{forgeurl}
Source0:        %{name}-%{version}.tar.gz

# Fedora/COPR baseline.
# Keep Fedora-version-specific dependency adjustments grouped here so targets
# like Fedora 42-rawhide can be carried in one spec.
BuildRequires:  cmake >= 3.28
BuildRequires:  desktop-file-utils
BuildRequires:  gcc-c++
BuildRequires:  java-devel
BuildRequires:  meson >= 1.4.0
BuildRequires:  ninja-build
BuildRequires:  pkgconf-pkg-config
BuildRequires:  python3
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtimageformats
BuildRequires:  qt6-qtmultimedia-devel
BuildRequires:  qt6-qtnetworkauth-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  qt6-qtwebsockets-devel
BuildRequires:  scdoc
BuildRequires:  shared-mime-info
BuildRequires:  cmark-devel
BuildRequires:  pkgconfig(gamemode)
BuildRequires:  pkgconfig(libarchive)
BuildRequires:  pkgconfig(libqrencode)
BuildRequires:  pkgconfig(tomlplusplus) >= 3.2.0
BuildRequires:  zlib-devel

Recommends:     java-headless
Requires:       qt6-qtimageformats

%description
Luna Launcher is a custom Minecraft launcher based on Prism Launcher with
additional multiplayer, authentication, mirror, theme, server, and
customization features.

%prep
%autosetup -n %{name}-%{version}

%build
%meson \
    --wrap-mode=nodownload \
    -Dinstall_layout=system \
    -Dbuild_platform=%{build_platform} \
    -Dupdater_github_repo= \
    -Denable_java_downloader=true \
    -Dbuild_pcl_download=false \
    -Dbuild_yukari_connect=false \
    -Dcurseforge_webview=disabled \
    -Dneo_ui=disabled \
    -Dyggdrasil=disabled \
    -Dbuild_testing=%{meson_build_testing}
%meson_build

%install
%meson_install

%check
%if %{with tests}
export QT_QPA_PLATFORM=offscreen
%meson_test
%endif

%post
if [ -x %{_bindir}/update-desktop-database ]; then
    %{_bindir}/update-desktop-database -q %{_datadir}/applications || :
fi
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
    %{_bindir}/gtk-update-icon-cache -q %{_datadir}/icons/hicolor || :
fi
if [ -x %{_bindir}/update-mime-database ]; then
    %{_bindir}/update-mime-database %{_datadir}/mime || :
fi

%postun
if [ -x %{_bindir}/update-desktop-database ]; then
    %{_bindir}/update-desktop-database -q %{_datadir}/applications || :
fi
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
    %{_bindir}/gtk-update-icon-cache -q %{_datadir}/icons/hicolor || :
fi
if [ -x %{_bindir}/update-mime-database ]; then
    %{_bindir}/update-mime-database %{_datadir}/mime || :
fi

%files
%license LICENSE
%doc README.md
%{_bindir}/lunalauncher
%{_datadir}/applications/org.lunalauncher.LunaLauncher.desktop
%{_datadir}/icons/hicolor/256x256/apps/org.lunalauncher.LunaLauncher.png
%{_datadir}/icons/hicolor/scalable/apps/org.lunalauncher.LunaLauncher.svg
%{_datadir}/metainfo/org.lunalauncher.LunaLauncher.metainfo.xml
%{_datadir}/mime/packages/org.lunalauncher.LunaLauncher.xml
%{_datadir}/%{datadir_name}/
%{_mandir}/man6/lunalauncher.6*

%changelog
* Sun Oct 25 2026 W-874 <1317825684@qq.com> - 11.1.1-1
- Build the native launcher with Meson and the Fedora system install layout
- Include complete pinned submodule sources in COPR source packages

* Sun May 24 2026 W-874 <1317825684@qq.com> - 11.0.2-1
- Generalize Fedora packaging for COPR targets Fedora 42 through Rawhide

* Mon May 18 2026 W-874 <1317825684@qq.com> - 11.0.2-1
- Add Fedora 44 COPR-friendly RPM packaging baseline
