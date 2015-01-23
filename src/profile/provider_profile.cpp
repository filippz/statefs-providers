/*
 * StateFS Profiled provider
 *
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include "provider_profile.hpp"
#include <math.h>
#include <iostream>
#include <qtaround/dbus.hpp>

namespace statefs { namespace profile {

using statefs::qt::Namespace;
using statefs::qt::PropertiesSource;
using qtaround::dbus::async;
using qtaround::dbus::sync;

static char const *service_name = "com.nokia.profiled";
static char const *root_path = "/com/nokia/profiled";

Bridge::Bridge(ProfileNs *ns, QDBusConnection &bus)
    : PropertiesSource(ns)
    , bus_(bus)
    , watch_(new ServiceWatch(bus, service_name))
{
}

void Bridge::init_conn()
{
    auto set_profile = [this](QString const &v) {
        updateProperty("Name", v);
    };

    auto on_changed = [set_profile]
        (bool, bool active, const QString &profile
         , ProfileDataList) {
        if (active) {
            qDebug() << "Active profile is " << profile;
            set_profile(profile);
        } else {
            qDebug() << "Inactive profile " << profile << " is changed";
        }
    };
    profiled_.reset(new Profile(service_name, root_path, bus_));
    sync(profiled_->get_profile(), set_profile);
    connect(profiled_.get(), &Profile::profile_changed
            , on_changed);
}

void Bridge::init()
{
    auto reset_all = [this]() {
        profiled_.reset();
        static_cast<ProfileNs*>(target_)->reset_properties();
    };
    watch_->init([this]() { init_conn(); }, reset_all);
    init_conn();
}


ProfileNs::ProfileNs(QDBusConnection &bus)
    : Namespace("Profile", std::unique_ptr<PropertiesSource>
                (new Bridge(this, bus)))
    , defaults_({{"Name", ""}})
{
    for (auto v : defaults_)
        addProperty(v.first, v.second);
    src_->init();
}

void ProfileNs::reset_properties()
{
    setProperties(defaults_);
}

class Provider;
static Provider *provider = nullptr;

class Provider : public statefs::AProvider
{
public:
    Provider(statefs_server *server)
        : AProvider("profile", server)
        , bus_(QDBusConnection::sessionBus())
    {
        auto ns = std::make_shared<ProfileNs>(bus_);
        insert(std::static_pointer_cast<statefs::ANode>(ns));
    }
    virtual ~Provider() {}

    virtual void release() {
        if (this == provider) {
            delete provider;
            provider = nullptr;
        }
    }

private:
    QDBusConnection bus_;
};

static inline Provider *init_provider(statefs_server *server)
{
    registerDataTypes();
    if (provider)
        throw std::logic_error("provider ptr is already set");
    provider = new Provider(server);
    return provider;
}

}}

EXTERN_C struct statefs_provider * statefs_provider_get
(struct statefs_server *server)
{
    return statefs::profile::init_provider(server);
}
