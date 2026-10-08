Name:           AlsaMixer.app
Version:        1.8.0
Release:        1%{?dist}
Summary:        Dockable ALSA mixer for Window Maker
License:        GPL-2.0-or-later
URL:            https://github.com/h-g-s/dockapps
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  pkgconfig
BuildRequires:  alsa-lib-devel
BuildRequires:  libX11-devel
BuildRequires:  libXext-devel
BuildRequires:  libXpm-devel

%description
AlsaMixer.app is a dockable mixer application for Window Maker. It can
control up to three ALSA volume sources, load and save mixer settings, and
launch an external application on middle click.

%prep
%autosetup

%build
%make_build

%install
%make_install PREFIX=%{_prefix} bindir=%{_bindir}

%files
%{_bindir}/AlsaMixer.app
