%global forgeurl https://github.com/W-874/LunaLauncher
%global datadir_name LunaLauncher
%global build_platform %(if [ -n "%{?fedora}" ]; then printf "fedora%s" "%{fedora}"; else printf "fedora"; fi)
%bcond_with tests
%if %{with tests}
%global cmake_build_testing ON
%else
%global cmake_build_testing OFF
%endif

Name:           lunalauncher
Version:        11.0.2
Release:        1%{?dist}
Summary:        Custom Minecraft launcher based on Prism Launcher

License:        GPL-3.0-only AND CC-BY-SA-4.0
URL:            %{forgeurl}
Source0:        %{name}-%{version}.tar.gz
Patch0:         quickjs-ng-rpm.patch

# Fedora/COPR baseline.
# Keep Fedora-version-specific dependency adjustments grouped here so targets
# like Fedora 42-rawhide can be carried in one spec.
BuildRequires:  cmake >= 3.28
BuildRequires:  desktop-file-utils
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  java-devel
BuildRequires:  ninja-build
BuildRequires:  pkgconf-pkg-config
BuildRequires:  qt6-qtbase-devel
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

%description
Luna Launcher is a custom Minecraft launcher based on Prism Launcher with
additional multiplayer, authentication, mirror, theme, server, and
customization features.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake \
    -DLauncher_BUILD_PLATFORM:STRING=%{build_platform} \
    -DLauncher_BUILD_ARTIFACT:STRING= \
    -DLauncher_UPDATER_GITHUB_REPO:STRING= \
    -DLauncher_ENABLE_JAVA_DOWNLOADER:BOOL=ON \
    -DBUILD_TESTING:BOOL=%{cmake_build_testing}
%cmake_build

%install
%cmake_install

%check
%if %{with tests}
%ctest --output-on-failure
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
%{_libdir}/libqjs.so.0*
%{_datadir}/applications/org.lunalauncher.LunaLauncher.desktop
%{_datadir}/icons/hicolor/256x256/apps/org.lunalauncher.LunaLauncher.png
%{_datadir}/icons/hicolor/scalable/apps/org.lunalauncher.LunaLauncher.svg
%{_datadir}/metainfo/org.lunalauncher.LunaLauncher.metainfo.xml
%{_datadir}/mime/packages/org.lunalauncher.LunaLauncher.xml
%{_datadir}/qlogging-categories6/*.categories
%{_datadir}/%{datadir_name}/
%{_mandir}/man6/lunalauncher.6*

%changelog
* Sun May 24 2026 W-874 <1317825684@qq.com> - 11.0.2-1
- Generalize Fedora packaging for COPR targets Fedora 42 through Rawhide

* Mon May 18 2026 W-874 <1317825684@qq.com> - 11.0.2-1
- Add Fedora 44 COPR-friendly RPM packaging baseline
