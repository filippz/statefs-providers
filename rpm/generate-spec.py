#!/usr/bin/python

import os, sys, re, itertools

template_re = re.compile(r'.*@@([a-z0-9A-Z_-]+)@@.*')

template_qt5 = '''
%package -n {name}
Summary: Statefs provider{summary}
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires: %{{n_common}} = %{{version}}-%{{release}}
Requires: statefs-loader-qt5 >= 0.0.9
{extra}
{obsoletes}
{provides}
{conflicts}
%description -n {name}
%{{summary}}

'''

template_default = '''
%package -n {name}
Summary: Statefs provider{summary}
Group: System Environment/Libraries
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
{extra}
{obsoletes}
{provides}
{conflicts}
%description -n {name}
%{{summary}}

'''

template_inout = '''
%package -n {name}
Summary: Statefs inout provider{summary}
Group: System Environment/Libraries
Requires: statefs >= %{{statefs_ver}}
{extra}
{obsoletes}
{provides}
{conflicts}
BuildArch: noarch
%description -n {name}
%{{summary}}
'''

decl_udev = '''
%if %{undefined suse_version}
BuildRequires: boost-filesystem >= 1.51.0
%endif
BuildRequires: boost-devel >= 1.51.0
BuildRequires: pkgconfig(cor-udev) >= 0.1.14
BuildRequires: pkgconfig(statefs-util) >= %{statefs_ver}
'''

decl_bluez = '''
Requires: bluez-libs >= 4.0
'''

decl_bme = '''
Requires: bme-rm-680-bin >= 0.9.95
'''

decl_upower = '''
Requires: upower >= 0.9.18
'''

decl_connman = '''
Requires: connman >= 1.15
'''

decl_ofono = '''
Requires: ofono >= 1.12
'''

decl_mce = '''
BuildRequires: pkgconfig(mce)
Obsoletes: statefs-provider-inout-mce <= 0.2.43
Provides: statefs-provider-inout-mce = 0.2.44
'''

decl_profile = '''
Requires: profiled >= 0.30
Obsoletes: statefs-provider-inout-profile <= 0.2.44.99
Provides: statefs-provider-inout-profile = 0.2.44.99
'''

decl_keyboard_generic = '''
BuildRequires: pkgconfig(cor-udev) >= 0.1.14
'''

def mk_pkg_name(name):
    return name.replace('_', '-')

class Actions:

    templates = {
        "qt5" : template_qt5
        , "default": template_default
        , "inout" : template_inout }

    extra = {
        "qt5" : {
            "bluez" : decl_bluez
            , "upower" : decl_upower
            , "connman" : decl_connman
            , "ofono" : decl_ofono
            , "mce" : decl_mce
            , "profile" : decl_profile
        }, "default" : {
            "udev" : decl_udev
            , "keyboard_generic" : decl_keyboard_generic
            , "bme" : decl_bme
        }
    }

    def pk_type_name_data(self, src, pk_type, name, defval):
        res = src.get(pk_type, None)
        if res:
            res = res.get(name, None)
            
        return res or defval

    def pk_type_name_list(self, src, pk_type, name):
        res = self.pk_type_name_data(src, pk_type, name, [])
        if type(res) == str:
            res = [res]
        return res

    def get_extra(self, pk_type, name):
        res = self.pk_type_name_data(Actions.extra, pk_type, name, "")
        return res.strip().split('\n')

    provides = {
        "qt5" : {
            "bluez" : "bluetooth"
            , "upower" : "power"
            , "connman" : ["internet", "network"]
            , "ofono" : "cellular"
            , "mce" : "system"
            , "profile" : "profile-info"
        }, "default" : {
            "udev" : "power"
            , "keyboard_generic" : "keyboard"
            , "bme" : "power"
        }, "inout" : {
            "bluetooth" : "bluetooth"
            , "power" : "power"
            , "network" : ["internet", "network"]
            , "cellular" : "cellular"
            , "mode_control" : "system"
            , "keyboard" : "keyboard"
            , "profile" : "profile-info"
            , "location" : "location"
        }
    }

    conflicts = {
        "qt5" : {
            "bluez" : "inout-bluetooth"
            , "upower" : [ "udev", "inout-power" ]
            , "connman" : "inout-network"
            , "ofono" : "inout-cellular"
            , "mce" : "inout-mode-control"
            , "profile" : "inout-profile"
        }, "default" : {
            "udev" : [ "upower", "inout-power" ]
            , "keyboard_generic" : "inout-keyboard"
            , "bme" : [ "upower", "inout-power" ]
        }, "inout" : {
            "bluetooth" : "bluez"
            , "power" : [ "upower", "udev" ]
            , "network" : [ "connman" ]
            , "cellular" : "ofono"
            , "mode_control" : "mce"
            , "keyboard" : "keyboard_generic"
            , "profile" : "profile"
            , "location" : "geoclue"
        }
    }

    summaries = {
        "qt5" : {
            "bluez" : ", source - bluez"
            , "upower" : ", source - upower"
            , "connman" : ", source - connman"
            , "ofono" : ", source - ofono"
            , "mce" : ", source - mce"
            , "profile" : ", source - profiled"
        }, "default" : {
            "udev" : ", source - sysfs/udev"
            , "keyboard_generic" : ", source - sysfs/udev"
            , "bme" : ", source - bme"
            , "back_cover" : ", source - back_cover"
        }, "inout" : {
            "bluetooth" : ": bluetooth properties"
            , "power" : ": power properties"
            , "network" : ": network properties"
            , "cellular" : ": cellular properties"
            , "mode_control" : ": system properties"
            , "keyboard" : ": keyboard properties"
            , "profile" : ": profile properties"
            , "location" : ": location properties"
        }
    }

    def get_summary(self, pk_type, name):
        return Actions.summaries[pk_type][name]

    obsoletes = {
        "meego" : {
            "bluetooth" : "bluetooth"
            , "power" : "battery-upower"
            , "internet" : "internet"
            , "cellular" : ["cellular", "phone"]
            , "location" : [ "location-geoclue", "location-skyhook" ]
        }
        , "maemo" : {
            "system" : "mce"
        }, "ckit" : {
            "bluetooth" : ["bluez", "bluetooth"]
            , "power" : ["power", "upower", "power-bme"]
            , "internet" : "connman"
            , "cellular" : ["cellular", "ofono"]
            , "system" : "mce"
            , "keyboard" : "keyboard-generic"
            , "profile-info" : "profile"
            , "location" : ["location-gypsy", "location-skyhook", "location"]
        }
    }


    def generic_name(self, pk_type, name):
        src = Actions.provides[pk_type]
        res = src.get(name, name)
        return res if type(res) == str else res[0]

    def get_provides(self, pk_type, name):
        src = Actions.provides[pk_type]
        res = src.get(name, None)
        if res is None:
            print name, " !providers"
            return res

        if type(res) == str:
            res = [res]
        fmt = "Provides: statefs-provider-{} = %{{version}}-%{{release}}"
        return [fmt.format(mk_pkg_name(v)) for v in res]

    def get_conflicts(self, pk_type, name):
        src = Actions.conflicts[pk_type]
        res = src.get(name, None)
        if res is None:
            return []
        if type(res) == str:
            res = [res]
        fmt = "Conflicts: statefs-provider-{}"
        return [fmt.format(mk_pkg_name(v)) for v in res]

    old_formats = {
        "meego" : "contextkit-meego-{}"
        , "maemo" : "contextkit-maemo-{}"
        , "ckit" : "contextkit-plugin-{}"
    }

    versions = {
        "meego" : [ "%{meego_ver}", "%{meego_ver1}" ]
        , "maemo" : [ "%{maemo_ver}", "%{maemo_ver1}" ]
        , "ckit" : [ "%{ckit_version}", "%{ckit_version1}" ]
    }

    def get_obsoletes(self, pk_type, old_type, name):
        name = self.generic_name(pk_type, name)
        fmt = Actions.old_formats[old_type]
        pkgs = self.pk_type_name_list(Actions.obsoletes, old_type, name)

        ver = Actions.versions[old_type]
        def obsolete(name):
            old_name = mk_pkg_name(fmt.format(name))
            return ("Obsoletes: {} <= {}".format(old_name, ver[0])
                    , "Provides: {} = {}".format(old_name, ver[1]))

        return list(itertools.chain(*[obsolete(x) for x in pkgs]))

    def package_name(self, pk_type, name):
        name = mk_pkg_name(name)
        if pk_type == "inout":
            return "statefs-provider-inout-{}".format(name)
        else:
            return "statefs-provider-{}".format(name)

    def get_package_description(self, pk_type, name):
        old_types = Actions.old_formats.keys()
        obsoletes = itertools.chain(*[self.get_obsoletes(pk_type, t, name)
                                      for t in old_types])
        res = Actions.templates[pk_type].format(
            name = self.package_name(pk_type, name),
            summary = self.get_summary(pk_type, name),
            obsoletes = '\n'.join(list(obsoletes)),
            conflicts = '\n'.join(self.get_conflicts(pk_type, name)),
            extra = '\n'.join(self.get_extra(pk_type, name)),
            provides = '\n'.join(self.get_provides(pk_type, name) or [])
        )
        
        res = [x for x in res.split('\n') if x]
        res.append("\n")
        return '\n'.join(res)

    qt5_system = ["bluez", "upower", "connman", "ofono", "mce"]
    qt5_user = ["profile"]

    default_system = ["udev", "bme", "back_cover", "keyboard_generic"]

    old_names = { "keyboard_generic" : "keyboard-generic" }

    inout_system = ["bluetooth", "power", "network", "cellular", "mode_control"
                    , "keyboard", "location"]
    inout_user = ["profile"]

    packages = {
        "qt5" : qt5_system + qt5_user
        , "default" : default_system
        , "inout" : inout_system + inout_user
    }

    def __init__(self, l):
        self.line = l

    def replace__(self, tpl, src, **kwargs):
        res = ""
        for name in src:
            old_name = Actions.old_names.get(name, name)
            res += tpl.format(name = name, old_name=old_name, **kwargs)
        return res.split("\n")

    def providers(self, name):
        src = getattr(Actions, name)
        with open(name + "-providers.spec.tpl") as f:
            return self.replace__(''.join(f.readlines()), src)

    def install__(self, name, kind):
        src = getattr(Actions, '_'.join((name, kind)), None)
        if src is None:
            return
        with open(name + "-install.spec.tpl") as f:
            return self.replace__(''.join(f.readlines()), src, kind = kind)

    def install(self, name):
        res = self.install__(name, "system") or []
        res.extend(self.install__(name, "user") or [])
        return res

    def declare(self, name):
        res = [self.get_package_description(name, p).split('\n')
               for p in Actions.packages[name]]
        return list(itertools.chain(*res))

def replaced(l):
    m = template_re.match(l)
    if (m is None):
        return (l,)
    actions = Actions(l)
    (part, name) = m.group(1).split("-")
    return getattr(actions, part)(name)

Actions.inout_user = ["inout_" + a for a in Actions.inout_user]
Actions.inout_system = ["inout_" + a for a in Actions.inout_system]


#print Actions("").get_obsoletes("qt5", "meego", "bluez")
#print Actions("").get_provides("qt5", "connman")
#print Actions("").get_package_description("qt5", "connman")
#print Actions("").get_package_description("inout", "power")
with open("statefs-providers.spec.tpl") as f:
    lines = f.readlines()
    res = itertools.chain(*[replaced(l.strip()) for l in lines])
    #res = list(itertools.chain(*[replaced(l) for l in res]))
    with open("statefs-providers.spec", "w") as out:
        out.write('\n'.join(list(res)))
        out.write('\n')
