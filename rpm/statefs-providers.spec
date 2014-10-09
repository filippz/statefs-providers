%define ckit_version 0.7.41
%define ckit_version1 0.7.42
%define ckit_statefs_version 0.2.30
%define ckit_statefs_version1 0.2.31
%define maemo_ver 0.7.30
%define maemo_ver1 0.7.31
%define meego_ver 0.1.0
%define meego_ver1 0.1.0.1
%define statefs_ver 0.3.25

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
BuildRequires: pkgconfig(cor) >= 0.1.16
BuildRequires: pkgconfig(qtaround-dbus) >= 0.2.0

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

%package -n statefs-provider-bluez
Summary: Statefs provider, source - bluez
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires: %{n_common} = %{version}-%{release}
Requires: statefs-loader-qt5 >= 0.0.9
Requires: bluez-libs >= 4.0
Obsoletes: contextkit-meego-bluetooth <= %{meego_ver}
Provides: contextkit-meego-bluetooth = %{meego_ver1}
Obsoletes: contextkit-plugin-bluez <= %{ckit_version}
Provides: contextkit-plugin-bluez = %{ckit_version1}
Obsoletes: contextkit-plugin-bluetooth <= %{ckit_version}
Provides: contextkit-plugin-bluetooth = %{ckit_version1}
Provides: statefs-provider-bluetooth = %{version}-%{release}
Conflicts: statefs-provider-inout-bluetooth
%description -n statefs-provider-bluez
%{summary}


%package -n statefs-provider-upower
Summary: Statefs provider, source - upower
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires: %{n_common} = %{version}-%{release}
Requires: statefs-loader-qt5 >= 0.0.9
Requires: upower >= 0.9.18
Obsoletes: contextkit-meego-battery-upower <= %{meego_ver}
Provides: contextkit-meego-battery-upower = %{meego_ver1}
Obsoletes: contextkit-plugin-power <= %{ckit_version}
Provides: contextkit-plugin-power = %{ckit_version1}
Obsoletes: contextkit-plugin-upower <= %{ckit_version}
Provides: contextkit-plugin-upower = %{ckit_version1}
Obsoletes: contextkit-plugin-power-bme <= %{ckit_version}
Provides: contextkit-plugin-power-bme = %{ckit_version1}
Provides: statefs-provider-power = %{version}-%{release}
Conflicts: statefs-provider-udev
Conflicts: statefs-provider-inout-power
%description -n statefs-provider-upower
%{summary}


%package -n statefs-provider-connman
Summary: Statefs provider, source - connman
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires: %{n_common} = %{version}-%{release}
Requires: statefs-loader-qt5 >= 0.0.9
Requires: connman >= 1.15
Obsoletes: contextkit-meego-internet <= %{meego_ver}
Provides: contextkit-meego-internet = %{meego_ver1}
Obsoletes: contextkit-plugin-connman <= %{ckit_version}
Provides: contextkit-plugin-connman = %{ckit_version1}
Provides: statefs-provider-internet = %{version}-%{release}
Provides: statefs-provider-network = %{version}-%{release}
Conflicts: statefs-provider-inout-network
%description -n statefs-provider-connman
%{summary}


%package -n statefs-provider-ofono
Summary: Statefs provider, source - ofono
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires: %{n_common} = %{version}-%{release}
Requires: statefs-loader-qt5 >= 0.0.9
Requires: ofono >= 1.12
Obsoletes: contextkit-meego-cellular <= %{meego_ver}
Provides: contextkit-meego-cellular = %{meego_ver1}
Obsoletes: contextkit-meego-phone <= %{meego_ver}
Provides: contextkit-meego-phone = %{meego_ver1}
Obsoletes: contextkit-plugin-cellular <= %{ckit_version}
Provides: contextkit-plugin-cellular = %{ckit_version1}
Obsoletes: contextkit-plugin-ofono <= %{ckit_version}
Provides: contextkit-plugin-ofono = %{ckit_version1}
Provides: statefs-provider-cellular = %{version}-%{release}
Conflicts: statefs-provider-inout-cellular
%description -n statefs-provider-ofono
%{summary}


%package -n statefs-provider-mce
Summary: Statefs provider, source - mce
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires: %{n_common} = %{version}-%{release}
Requires: statefs-loader-qt5 >= 0.0.9
BuildRequires: pkgconfig(mce)
Obsoletes: statefs-provider-inout-mce <= 0.2.43
Provides: statefs-provider-inout-mce = 0.2.44
Obsoletes: contextkit-maemo-mce <= %{maemo_ver}
Provides: contextkit-maemo-mce = %{maemo_ver1}
Obsoletes: contextkit-plugin-mce <= %{ckit_version}
Provides: contextkit-plugin-mce = %{ckit_version1}
Provides: statefs-provider-system = %{version}-%{release}
Conflicts: statefs-provider-inout-mode-control
%description -n statefs-provider-mce
%{summary}


%package -n statefs-provider-profile
Summary: Statefs provider, source - profiled
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires: %{n_common} = %{version}-%{release}
Requires: statefs-loader-qt5 >= 0.0.9
Requires: profiled >= 0.30
Obsoletes: statefs-provider-inout-profile <= 0.2.44.99
Provides: statefs-provider-inout-profile = 0.2.44.99
Obsoletes: contextkit-plugin-profile <= %{ckit_version}
Provides: contextkit-plugin-profile = %{ckit_version1}
Provides: statefs-provider-profile-info = %{version}-%{release}
Conflicts: statefs-provider-inout-profile
%description -n statefs-provider-profile
%{summary}


%package -n statefs-provider-udev
Summary: Statefs provider, source - sysfs/udev
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires: boost-filesystem >= 1.51.0
BuildRequires: boost-devel >= 1.51.0
BuildRequires: pkgconfig(cor-udev) >= 0.1.14
BuildRequires: pkgconfig(statefs-util) >= %{statefs_ver}
Obsoletes: contextkit-meego-battery-upower <= %{meego_ver}
Provides: contextkit-meego-battery-upower = %{meego_ver1}
Obsoletes: contextkit-plugin-power <= %{ckit_version}
Provides: contextkit-plugin-power = %{ckit_version1}
Obsoletes: contextkit-plugin-upower <= %{ckit_version}
Provides: contextkit-plugin-upower = %{ckit_version1}
Obsoletes: contextkit-plugin-power-bme <= %{ckit_version}
Provides: contextkit-plugin-power-bme = %{ckit_version1}
Provides: statefs-provider-power = %{version}-%{release}
Conflicts: statefs-provider-upower
Conflicts: statefs-provider-inout-power
%description -n statefs-provider-udev
%{summary}


%package -n statefs-provider-bme
Summary: Statefs provider, source - bme
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires: bme-rm-680-bin >= 0.9.95
Obsoletes: contextkit-meego-battery-upower <= %{meego_ver}
Provides: contextkit-meego-battery-upower = %{meego_ver1}
Obsoletes: contextkit-plugin-power <= %{ckit_version}
Provides: contextkit-plugin-power = %{ckit_version1}
Obsoletes: contextkit-plugin-upower <= %{ckit_version}
Provides: contextkit-plugin-upower = %{ckit_version1}
Obsoletes: contextkit-plugin-power-bme <= %{ckit_version}
Provides: contextkit-plugin-power-bme = %{ckit_version1}
Provides: statefs-provider-power = %{version}-%{release}
Conflicts: statefs-provider-upower
Conflicts: statefs-provider-inout-power
%description -n statefs-provider-bme
%{summary}


%package -n statefs-provider-back-cover
Summary: Statefs provider, source - back_cover
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
%description -n statefs-provider-back-cover
%{summary}


%package -n statefs-provider-keyboard-generic
Summary: Statefs provider, source - sysfs/udev
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires: pkgconfig(cor-udev) >= 0.1.14
Obsoletes: contextkit-plugin-keyboard-generic <= %{ckit_version}
Provides: contextkit-plugin-keyboard-generic = %{ckit_version1}
Provides: statefs-provider-keyboard = %{version}-%{release}
Conflicts: statefs-provider-inout-keyboard
%description -n statefs-provider-keyboard-generic
%{summary}


%package -n statefs-provider-inout-bluetooth
Summary: Statefs inout provider: bluetooth properties
Group: System Environment/Libraries
Requires: statefs >= %{statefs_ver}
Obsoletes: contextkit-meego-bluetooth <= %{meego_ver}
Provides: contextkit-meego-bluetooth = %{meego_ver1}
Obsoletes: contextkit-plugin-bluez <= %{ckit_version}
Provides: contextkit-plugin-bluez = %{ckit_version1}
Obsoletes: contextkit-plugin-bluetooth <= %{ckit_version}
Provides: contextkit-plugin-bluetooth = %{ckit_version1}
Provides: statefs-provider-bluetooth = %{version}-%{release}
Conflicts: statefs-provider-bluez
BuildArch: noarch
%description -n statefs-provider-inout-bluetooth
%{summary}


%package -n statefs-provider-inout-power
Summary: Statefs inout provider: power properties
Group: System Environment/Libraries
Requires: statefs >= %{statefs_ver}
Obsoletes: contextkit-meego-battery-upower <= %{meego_ver}
Provides: contextkit-meego-battery-upower = %{meego_ver1}
Obsoletes: contextkit-plugin-power <= %{ckit_version}
Provides: contextkit-plugin-power = %{ckit_version1}
Obsoletes: contextkit-plugin-upower <= %{ckit_version}
Provides: contextkit-plugin-upower = %{ckit_version1}
Obsoletes: contextkit-plugin-power-bme <= %{ckit_version}
Provides: contextkit-plugin-power-bme = %{ckit_version1}
Provides: statefs-provider-power = %{version}-%{release}
Conflicts: statefs-provider-upower
Conflicts: statefs-provider-udev
BuildArch: noarch
%description -n statefs-provider-inout-power
%{summary}


%package -n statefs-provider-inout-network
Summary: Statefs inout provider: network properties
Group: System Environment/Libraries
Requires: statefs >= %{statefs_ver}
Obsoletes: contextkit-meego-internet <= %{meego_ver}
Provides: contextkit-meego-internet = %{meego_ver1}
Obsoletes: contextkit-plugin-connman <= %{ckit_version}
Provides: contextkit-plugin-connman = %{ckit_version1}
Provides: statefs-provider-internet = %{version}-%{release}
Provides: statefs-provider-network = %{version}-%{release}
Conflicts: statefs-provider-connman
BuildArch: noarch
%description -n statefs-provider-inout-network
%{summary}


%package -n statefs-provider-inout-cellular
Summary: Statefs inout provider: cellular properties
Group: System Environment/Libraries
Requires: statefs >= %{statefs_ver}
Obsoletes: contextkit-meego-cellular <= %{meego_ver}
Provides: contextkit-meego-cellular = %{meego_ver1}
Obsoletes: contextkit-meego-phone <= %{meego_ver}
Provides: contextkit-meego-phone = %{meego_ver1}
Obsoletes: contextkit-plugin-cellular <= %{ckit_version}
Provides: contextkit-plugin-cellular = %{ckit_version1}
Obsoletes: contextkit-plugin-ofono <= %{ckit_version}
Provides: contextkit-plugin-ofono = %{ckit_version1}
Provides: statefs-provider-cellular = %{version}-%{release}
Conflicts: statefs-provider-ofono
BuildArch: noarch
%description -n statefs-provider-inout-cellular
%{summary}


%package -n statefs-provider-inout-mode-control
Summary: Statefs inout provider: system properties
Group: System Environment/Libraries
Requires: statefs >= %{statefs_ver}
Obsoletes: contextkit-maemo-mce <= %{maemo_ver}
Provides: contextkit-maemo-mce = %{maemo_ver1}
Obsoletes: contextkit-plugin-mce <= %{ckit_version}
Provides: contextkit-plugin-mce = %{ckit_version1}
Provides: statefs-provider-system = %{version}-%{release}
Conflicts: statefs-provider-mce
BuildArch: noarch
%description -n statefs-provider-inout-mode-control
%{summary}


%package -n statefs-provider-inout-keyboard
Summary: Statefs inout provider: keyboard properties
Group: System Environment/Libraries
Requires: statefs >= %{statefs_ver}
Obsoletes: contextkit-plugin-keyboard-generic <= %{ckit_version}
Provides: contextkit-plugin-keyboard-generic = %{ckit_version1}
Provides: statefs-provider-keyboard = %{version}-%{release}
Conflicts: statefs-provider-keyboard-generic
BuildArch: noarch
%description -n statefs-provider-inout-keyboard
%{summary}


%package -n statefs-provider-inout-location
Summary: Statefs inout provider: location properties
Group: System Environment/Libraries
Requires: statefs >= %{statefs_ver}
Obsoletes: contextkit-meego-location-geoclue <= %{meego_ver}
Provides: contextkit-meego-location-geoclue = %{meego_ver1}
Obsoletes: contextkit-meego-location-skyhook <= %{meego_ver}
Provides: contextkit-meego-location-skyhook = %{meego_ver1}
Obsoletes: contextkit-plugin-location-gypsy <= %{ckit_version}
Provides: contextkit-plugin-location-gypsy = %{ckit_version1}
Obsoletes: contextkit-plugin-location-skyhook <= %{ckit_version}
Provides: contextkit-plugin-location-skyhook = %{ckit_version1}
Obsoletes: contextkit-plugin-location <= %{ckit_version}
Provides: contextkit-plugin-location = %{ckit_version1}
Provides: statefs-provider-location = %{version}-%{release}
Conflicts: statefs-provider-geoclue
BuildArch: noarch
%description -n statefs-provider-inout-location
%{summary}


%package -n statefs-provider-inout-profile
Summary: Statefs inout provider: profile properties
Group: System Environment/Libraries
Requires: statefs >= %{statefs_ver}
Obsoletes: contextkit-plugin-profile <= %{ckit_version}
Provides: contextkit-plugin-profile = %{ckit_version1}
Provides: statefs-provider-profile-info = %{version}-%{release}
Conflicts: statefs-provider-profile
BuildArch: noarch
%description -n statefs-provider-inout-profile
%{summary}



%prep
%setup -q


%build
%cmake -DVERSION=%{version} %{?_with_multiarch:-DENABLE_MULTIARCH=ON}
make %{?jobs:-j%jobs}
make doc
pushd inout && %cmake && popd


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
pushd inout && make install DESTDIR=%{buildroot} && popd

%statefs_provider_install default udev %{_statefs_libdir}/libprovider-udev.so system

%statefs_provider_install default bme %{_statefs_libdir}/libprovider-bme.so system

%statefs_provider_install default back_cover %{_statefs_libdir}/libprovider-back_cover.so system

%statefs_provider_install default keyboard_generic %{_statefs_libdir}/libprovider-keyboard_generic.so system


%statefs_provider_install qt5 bluez %{_statefs_libdir}/libprovider-bluez.so system
%statefs_provider_install qt5 upower %{_statefs_libdir}/libprovider-upower.so system
%statefs_provider_install qt5 connman %{_statefs_libdir}/libprovider-connman.so system
%statefs_provider_install qt5 ofono %{_statefs_libdir}/libprovider-ofono.so system
%statefs_provider_install qt5 mce %{_statefs_libdir}/libprovider-mce.so system

%statefs_provider_install qt5 profile %{_statefs_libdir}/libprovider-profile.so user

%statefs_provider_install inout inout_bluetooth %{_statefs_datadir}/inout_bluetooth.conf system
%statefs_provider_install inout inout_power %{_statefs_datadir}/inout_power.conf system
%statefs_provider_install inout inout_network %{_statefs_datadir}/inout_network.conf system
%statefs_provider_install inout inout_cellular %{_statefs_datadir}/inout_cellular.conf system
%statefs_provider_install inout inout_mode_control %{_statefs_datadir}/inout_mode_control.conf system
%statefs_provider_install inout inout_keyboard %{_statefs_datadir}/inout_keyboard.conf system
%statefs_provider_install inout inout_location %{_statefs_datadir}/inout_location.conf system

%statefs_provider_install inout inout_profile %{_statefs_datadir}/inout_profile.conf user


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


%files %{p_bluez} -f bluez.files
%defattr(-,root,root,-)

%pre %{p_bluez}
%statefs_pre
if [ -f %{_statefs_libdir}/libprovider-bluez.so ]; then
statefs unregister %{_statefs_libdir}/libprovider-bluez.so || :
fi

%post %{p_bluez}
/sbin/ldconfig
%statefs_provider_register qt5 bluez system
%statefs_post

%preun %{p_bluez}
%statefs_preun
%statefs_provider_unregister qt5 bluez system

%postun %{p_bluez}
/sbin/ldconfig
%statefs_postun

%files %{p_upower} -f upower.files
%defattr(-,root,root,-)

%pre %{p_upower}
%statefs_pre
if [ -f %{_statefs_libdir}/libprovider-upower.so ]; then
statefs unregister %{_statefs_libdir}/libprovider-upower.so || :
fi

%post %{p_upower}
/sbin/ldconfig
%statefs_provider_register qt5 upower system
%statefs_post

%preun %{p_upower}
%statefs_preun
%statefs_provider_unregister qt5 upower system

%postun %{p_upower}
/sbin/ldconfig
%statefs_postun

%files %{p_connman} -f connman.files
%defattr(-,root,root,-)

%pre %{p_connman}
%statefs_pre
if [ -f %{_statefs_libdir}/libprovider-connman.so ]; then
statefs unregister %{_statefs_libdir}/libprovider-connman.so || :
fi

%post %{p_connman}
/sbin/ldconfig
%statefs_provider_register qt5 connman system
%statefs_post

%preun %{p_connman}
%statefs_preun
%statefs_provider_unregister qt5 connman system

%postun %{p_connman}
/sbin/ldconfig
%statefs_postun

%files %{p_ofono} -f ofono.files
%defattr(-,root,root,-)

%pre %{p_ofono}
%statefs_pre
if [ -f %{_statefs_libdir}/libprovider-ofono.so ]; then
statefs unregister %{_statefs_libdir}/libprovider-ofono.so || :
fi

%post %{p_ofono}
/sbin/ldconfig
%statefs_provider_register qt5 ofono system
%statefs_post

%preun %{p_ofono}
%statefs_preun
%statefs_provider_unregister qt5 ofono system

%postun %{p_ofono}
/sbin/ldconfig
%statefs_postun

%files %{p_mce} -f mce.files
%defattr(-,root,root,-)

%pre %{p_mce}
%statefs_pre
if [ -f %{_statefs_libdir}/libprovider-mce.so ]; then
statefs unregister %{_statefs_libdir}/libprovider-mce.so || :
fi

%post %{p_mce}
/sbin/ldconfig
%statefs_provider_register qt5 mce system
%statefs_post

%preun %{p_mce}
%statefs_preun
%statefs_provider_unregister qt5 mce system

%postun %{p_mce}
/sbin/ldconfig
%statefs_postun


%files %{p_profile} -f profile.files
%defattr(-,root,root,-)

%pre %{p_profile}
%statefs_pre

%post %{p_profile}
/sbin/ldconfig
%statefs_provider_register qt5 profile
%statefs_post

%preun %{p_profile}
%statefs_preun
%statefs_provider_unregister qt5 profile

%postun %{p_profile}
/sbin/ldconfig
%statefs_postun



%files %{p_udev} -f udev.files
%defattr(-,root,root,-)

%pre %{p_udev}
%statefs_pre

%post %{p_udev}
/sbin/ldconfig
%statefs_provider_register default udev system
%statefs_post

%preun %{p_udev}
%statefs_preun
%statefs_provider_unregister default udev system

%postun %{p_udev}
/sbin/ldconfig
%statefs_postun

%files %{p_bme} -f bme.files
%defattr(-,root,root,-)

%pre %{p_bme}
%statefs_pre

%post %{p_bme}
/sbin/ldconfig
%statefs_provider_register default bme system
%statefs_post

%preun %{p_bme}
%statefs_preun
%statefs_provider_unregister default bme system

%postun %{p_bme}
/sbin/ldconfig
%statefs_postun

%files %{p_back_cover} -f back_cover.files
%defattr(-,root,root,-)

%pre %{p_back_cover}
%statefs_pre

%post %{p_back_cover}
/sbin/ldconfig
%statefs_provider_register default back_cover system
%statefs_post

%preun %{p_back_cover}
%statefs_preun
%statefs_provider_unregister default back_cover system

%postun %{p_back_cover}
/sbin/ldconfig
%statefs_postun

%files %{p_keyboard_generic} -f keyboard_generic.files
%defattr(-,root,root,-)

%pre %{p_keyboard_generic}
%statefs_pre

%post %{p_keyboard_generic}
/sbin/ldconfig
%statefs_provider_register default keyboard_generic system
%statefs_post

%preun %{p_keyboard_generic}
%statefs_preun
%statefs_provider_unregister default keyboard_generic system

%postun %{p_keyboard_generic}
/sbin/ldconfig
%statefs_postun



%files %{p_inout_bluetooth} -f inout_bluetooth.files
%defattr(-,root,root,-)

%pre %{p_inout_bluetooth}
%statefs_pre

%post %{p_inout_bluetooth}
%{_statefs_libdir}/provider-do unregister inout inout_bluetooth
%statefs_provider_register inout inout_bluetooth system
%statefs_post

%preun %{p_inout_bluetooth}
%statefs_preun
%statefs_provider_unregister inout inout_bluetooth

%postun %{p_inout_bluetooth}
%statefs_postun

%files %{p_inout_power} -f inout_power.files
%defattr(-,root,root,-)

%pre %{p_inout_power}
%statefs_pre

%post %{p_inout_power}
%{_statefs_libdir}/provider-do unregister inout inout_power
%statefs_provider_register inout inout_power system
%statefs_post

%preun %{p_inout_power}
%statefs_preun
%statefs_provider_unregister inout inout_power

%postun %{p_inout_power}
%statefs_postun

%files %{p_inout_network} -f inout_network.files
%defattr(-,root,root,-)

%pre %{p_inout_network}
%statefs_pre

%post %{p_inout_network}
%{_statefs_libdir}/provider-do unregister inout inout_network
%statefs_provider_register inout inout_network system
%statefs_post

%preun %{p_inout_network}
%statefs_preun
%statefs_provider_unregister inout inout_network

%postun %{p_inout_network}
%statefs_postun

%files %{p_inout_cellular} -f inout_cellular.files
%defattr(-,root,root,-)

%pre %{p_inout_cellular}
%statefs_pre

%post %{p_inout_cellular}
%{_statefs_libdir}/provider-do unregister inout inout_cellular
%statefs_provider_register inout inout_cellular system
%statefs_post

%preun %{p_inout_cellular}
%statefs_preun
%statefs_provider_unregister inout inout_cellular

%postun %{p_inout_cellular}
%statefs_postun

%files %{p_inout_mode_control} -f inout_mode_control.files
%defattr(-,root,root,-)

%pre %{p_inout_mode_control}
%statefs_pre

%post %{p_inout_mode_control}
%{_statefs_libdir}/provider-do unregister inout inout_mode_control
%statefs_provider_register inout inout_mode_control system
%statefs_post

%preun %{p_inout_mode_control}
%statefs_preun
%statefs_provider_unregister inout inout_mode_control

%postun %{p_inout_mode_control}
%statefs_postun

%files %{p_inout_keyboard} -f inout_keyboard.files
%defattr(-,root,root,-)

%pre %{p_inout_keyboard}
%statefs_pre

%post %{p_inout_keyboard}
%{_statefs_libdir}/provider-do unregister inout inout_keyboard
%statefs_provider_register inout inout_keyboard system
%statefs_post

%preun %{p_inout_keyboard}
%statefs_preun
%statefs_provider_unregister inout inout_keyboard

%postun %{p_inout_keyboard}
%statefs_postun

%files %{p_inout_location} -f inout_location.files
%defattr(-,root,root,-)

%pre %{p_inout_location}
%statefs_pre

%post %{p_inout_location}
%{_statefs_libdir}/provider-do unregister inout inout_location
%statefs_provider_register inout inout_location system
%statefs_post

%preun %{p_inout_location}
%statefs_preun
%statefs_provider_unregister inout inout_location

%postun %{p_inout_location}
%statefs_postun


%files %{p_inout_profile} -f inout_profile.files
%defattr(-,root,root,-)

%pre %{p_inout_profile}
%statefs_pre

%post %{p_inout_profile}
%statefs_provider_register inout inout_profile system
%statefs_post

%preun %{p_inout_profile}
%statefs_preun
%statefs_provider_unregister inout inout_profile user

%postun %{p_inout_profile}
%statefs_postun

