%global debug_package %{nil}
%define _hardened_build 1

Name:           kew
Version:        3.5.0
Release:        1%{?dist}
Summary:        Terminal music player

License:        GPLv3
URL:            https://codeberg.org/ravachol/kew
Source0:        kew-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  pkg-config
BuildRequires:  taglib-devel
BuildRequires:  fftw-devel
BuildRequires:  opus-devel
BuildRequires:  opusfile-devel
BuildRequires:  libvorbis-devel
BuildRequires:  libogg-devel
BuildRequires:  chafa-devel
BuildRequires:  glib2-devel
BuildRequires:  faad2-devel

%description
kew is a terminal music player with a customizable interface, playlist management, and support for various audio formats.

%prep
%autosetup -n %{name}-%{version}

%build
%make_build

%install
make install DESTDIR=%{buildroot} PREFIX=%{_prefix}

%files
%{_bindir}/kew
%{_datadir}/kew/
%{_datadir}/locale/zh_CN/LC_MESSAGES/
%{_datadir}/locale/ja/LC_MESSAGES/
%{_mandir}/man1/kew.1.gz

%changelog
* Sun Oct 05 2025 Jules - 3.5.0-1
- Initial RPM release.
