%{!?cmake_install: %global cmake_install make install DESTDIR=%{buildroot}}

%define ckit_version 0.7.41
%define ckit_version1 0.7.42
%define ckit_statefs_version 0.2.30
%define ckit_statefs_version1 0.2.31
%define maemo_ver 0.7.30
%define maemo_ver1 0.7.31
%define meego_ver 0.1.0
%define meego_ver1 0.1.0.1
%define statefs_ver 0.3.27

Summary: Statefs providers
Name: statefs-providers
Version: x.x.x
Release: 1
License: LGPLv2
Group: System Environment/Libraries
URL: http://github.com/nemomobile/statefs-providers
Source0: %{name}-%{version}.tar.bz2
Source1: generate-spec.py
Source2: inout-install.spec.tpl
Source3: inout_system-providers.spec.tpl
Source4: inout_user-providers.spec.tpl
Source5: qt5-install.spec.tpl
Source6: qt5_system-providers.spec.tpl
Source7: qt5_user-providers.spec.tpl
Source8: statefs-providers.spec.tpl
Source9: default_system-providers.spec.tpl
Source10: default-install.spec.tpl

BuildRequires: cmake >= 2.8
BuildRequires: statefs >= %{statefs_ver}
BuildRequires: pkgconfig(statefs-cpp) >= %{statefs_ver}
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(cor) >= 0.1.17
BuildRequires: pkgconfig(qtaround-dbus) >= 0.2.4

%description
%{summary}

%define p_common -n statefs-provider-qt5
%define n_common statefs-provider-qt5

%package %{p_common}
Summary: Package to replace contextkit plugins
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires: statefs >= %{statefs_ver}
Obsoletes: contextkit-maemo <= %{maemo_ver}
Provides: contextkit-maemo = %{maemo_ver1}
Obsoletes: contextkit-meego <= %{meego_ver}
Provides: contextkit-meego = %{meego_ver1}
Obsoletes: statefs-contextkit-provider <= %{ckit_statefs_version}
Provides: statefs-contextkit-provider = %{ckit_statefs_version1}
BuildRequires: pkgconfig(statefs-qt5) >= 0.2.47
%description %{p_common}
%{summary}

%package qt5-devel
Summary: StateFS Qt5 library for providers, development files
Group: Development/Libraries
Requires: statefs-provider-qt5 = %{version}-%{release}
%description qt5-devel
%{summary}

%define p_bluez -n statefs-provider-bluez
%define p_bme -n statefs-provider-bme
%define p_upower -n statefs-provider-upower
%define p_connman -n statefs-provider-connman
%define p_ofono -n statefs-provider-ofono
%define p_mce -n statefs-provider-mce
%define p_profile -n statefs-provider-profile
%define p_keyboard_generic -n statefs-provider-keyboard-generic

%define p_udev -n statefs-provider-udev
%define p_back_cover -n statefs-provider-back-cover

%define p_inout_bluetooth -n statefs-provider-inout-bluetooth
%define p_inout_power -n statefs-provider-inout-power
%define p_inout_network -n statefs-provider-inout-network
%define p_inout_cellular -n statefs-provider-inout-cellular
%define p_inout_mode_control -n statefs-provider-inout-mode-control
%define p_inout_profile -n statefs-provider-inout-profile
%define p_inout_keyboard -n statefs-provider-inout-keyboard
%define p_inout_location -n statefs-provider-inout-location

@@declare-qt5@@
@@declare-default@@
@@declare-inout@@

%prep
%setup -q


%build
%cmake -DVERSION=%{version} %{?_with_multiarch:-DENABLE_MULTIARCH=ON}
make %{?jobs:-j%jobs}
make doc
pushd inout && %cmake && popd


%install
rm -rf %{buildroot}
%cmake_install
pushd inout && make install DESTDIR=%{buildroot} && popd

@@install-default@@
@@install-qt5@@
@@install-inout@@

%clean
rm -rf %{buildroot}

%files %{p_common}
%defattr(-,root,root,-)
%doc README
%{_libdir}/libstatefs-providers-qt5.so

%post %{p_common} -p /sbin/ldconfig
%postun %{p_common} -p /sbin/ldconfig

%files qt5-devel
%defattr(-,root,root,-)
%{_qt5_headerdir}/statefs/qt/*.hpp
%{_libdir}/pkgconfig/statefs-providers-qt5.pc

@@providers-qt5_system@@
@@providers-qt5_user@@

@@providers-default_system@@

@@providers-inout_system@@
@@providers-inout_user@@
