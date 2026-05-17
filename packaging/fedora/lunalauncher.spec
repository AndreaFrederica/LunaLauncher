%global forgeurl https://github.com/W-874/LunaLauncher
%global datadir_name LunaLauncher
%bcond_with tests

Name:           lunalauncher
Version:        11.0.2
Release:        1%{?dist}
Summary:        Custom Minecraft launcher based on Prism Launcher

License:        GPL-3.0-only AND CC-BY-SA-4.0
URL:            %{forgeurl}
Source0:        %{name}-%{version}.tar.gz

# Fedora 44 baseline.
# Keep Fedora-version-specific dependency adjustments grouped here so future
# COPR targets (for example Fedora 41-43 or Rawhide) can be added cleanly.
BuildRequires:  cmake >= 3.28
BuildRequires:  desktop-file-utils
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  java-17-openjdk-devel
BuildRequires:  ninja-build
BuildRequires:  qt6-qt5compat-devel
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtmultimedia-devel
BuildRequires:  qt6-qtnetworkauth-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  qt6-qtwebsockets-devel
BuildRequires:  scdoc
BuildRequires:  shared-mime-info
BuildRequires:  cmark-devel
BuildRequires:  gamemode-devel
BuildRequires:  libarchive-devel
BuildRequires:  qrencode-devel
BuildRequires:  tomlplusplus-devel >= 3.2.0
BuildRequires:  zlib-devel

Recommends:     java-17-openjdk

%description
Luna Launcher is a custom Minecraft launcher based on Prism Launcher with
additional multiplayer, authentication, mirror, theme, server, and
customization features.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake \
    -DLauncher_BUILD_PLATFORM:STRING=fedora44 \
    -DLauncher_BUILD_ARTIFACT:STRING= \
    -DLauncher_UPDATER_GITHUB_REPO:STRING= \
    -DLauncher_ENABLE_JAVA_DOWNLOADER:BOOL=ON
%cmake_build

%install
%cmake_install

%check
%if %{with tests}
%ctest --output-on-failure
%endif

%post
%desktop_database_post
%icon_theme_cache_post
%mime_database_post

%postun
%desktop_database_postun
%icon_theme_cache_postun
%mime_database_postun

%files
%license LICENSE
%doc README.md
%{_bindir}/lunalauncher
%{_datadir}/applications/org.lunalauncher.LunaLauncher.desktop
%{_datadir}/icons/hicolor/256x256/apps/org.lunalauncher.LunaLauncher.png
%{_datadir}/icons/hicolor/scalable/apps/org.lunalauncher.LunaLauncher.svg
%{_datadir}/metainfo/org.lunalauncher.LunaLauncher.metainfo.xml
%{_datadir}/mime/packages/org.lunalauncher.LunaLauncher.xml
%{_datadir}/qlogging-categories6/*.categories
%{_datadir}/%{datadir_name}/
%{_mandir}/man6/lunalauncher.6*

%changelog
* Mon May 18 2026 W-874 <1317825684@qq.com> - 11.0.2-1
- Add Fedora 44 COPR-friendly RPM packaging baseline
