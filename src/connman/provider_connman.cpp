/*
 * StateFS Connman provider
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

#include "provider_connman.hpp"
#include <math.h>
#include <iostream>
#include <qtaround/dbus.hpp>
#include "dbus_types.hpp"

namespace statefs { namespace connman {

using statefs::qt::Namespace;
using statefs::qt::PropertiesSource;
using qtaround::dbus::sync;
using qtaround::dbus::async;

static char const *service_name = "net.connman";

Bridge::Bridge(InternetNs *ns, QDBusConnection &bus)
    : PropertiesSource(ns)
    , bus_(bus)
    , watch_(new ServiceWatch(bus, service_name))
    , net_type_map_{
    {"wifi", "WLAN"}
    , {"gprs", "GPRS"}
    , {"cellular", "GPRS"}
    , {"edge", "GPRS"}
    , {"umts", "GPRS"}
    , {"ethernet", "ethernet"}}
    , state_map_{
        {"offline", Status::Offline}
        , {"idle", Status::Offline}
        , {"online", Status::Online}
        , {"ready", Status::Online}}
    , states_{"disconnected", "connected"}
{
}

void Bridge::process_manager_props(QVariantMap const &props)
{

    auto update = [this](QString const &n, QVariant const &v) {
        if (n == "State") {
            auto state = v.toString();
            qDebug() << "Network manager is " << state;
            if (state_map_[state] == Status::Online) {
                process_services();
                process_technologies();
            } else {
                reset_properties();
            }
        }
    };
    connect(manager_.get(), &Manager::PropertyChanged
            , [update] (QString const &n, QDBusVariant const &v) {
                qDebug() << "Manager property " << n;
                update(n, v.variant());
            });
    connect(manager_.get(), &Manager::TechnologyAdded
            , [this] (const QDBusObjectPath &path, const QVariantMap &props) {
                qDebug() << "Technology added " << path.path();
                process_technology(path.path(), props);
            });
    connect(manager_.get(), &Manager::TechnologyRemoved
            , [this] (const QDBusObjectPath &path) {
                qDebug() << "Technology removed " << path.path();
                process_technologies();
            });
    connect(manager_.get(), &Manager::ServicesAdded
            , [this] (PathPropertiesArray const &) {
                qDebug() << "Services added";
                process_services();
            });
    connect(manager_.get(), &Manager::ServicesRemoved
            , [this] (const QList<QDBusObjectPath> &data) {
                qDebug() << "Services removed";
                auto services = QSet<QDBusObjectPath>::fromList(data);
                if (services.contains(QDBusObjectPath(current_service_))) {
                    process_services();
                }
            });

    for (auto it = props.begin(); it != props.end(); ++it)
        update(it.key(), it.value());
}

void Bridge::init()
{
    auto init_manager = [this]() {
        qDebug() << "Establish connection with connman";
        manager_.reset(new Manager(service_name, "/", bus_));
        sync(manager_->GetProperties()
             , [this](QVariantMap const &v) {
                 process_manager_props(v);
             });
    };
    watch_->init(init_manager, [this]() { reset_manager(); });
    init_manager();
}

void Bridge::reset_properties()
{
    qDebug() << "Internet: reset properties";
    static_cast<InternetNs*>(target_)->reset_properties();
}

void Bridge::reset_manager()
{
    qDebug() << "Connman is unregistered, cleaning up";
    manager_.reset();
    reset_properties();
}

void Bridge::process_technologies()
{
    technologies_.clear();
    auto process_props = [this](PathPropertiesArray const &techs) {
        for (auto pp = techs.begin(); pp != techs.end(); ++pp) {
            auto const &path = std::get<0>(*pp).path();
            auto const &props = std::get<1>(*pp);
            process_technology(path, props);
        }
    };
    sync(manager_->GetTechnologies(), process_props);
}

Status Bridge::process_service
(QString const &path, QVariantMap const &props)
{
    auto get_status = [this](QVariant const &state) {
        return state_map_[state.toString()];
    };

    auto status = get_status(props["State"]);

    qDebug() << "Service " << props["Name"].toString() << " is "
             << (status == Status::Online ? "online" : "offline");

    if (status == Status::Offline)
        return status;

    current_service_ = path;

    auto update_status = [this, path, get_status](QVariant const &v) {
        qDebug() << path << " status ->" << v.toString();
        auto status = get_status(v);
        auto name = states_[static_cast<size_t>(status)];
        updateProperty("NetworkState", name);
    };

    auto update = [this, update_status](QString const &n, QVariant const &v) {
        // qDebug() << "Changed: " << n << " for " << path;
        if (n == "Name") {
            updateProperty("NetworkName", v);
        } else if (n == "Strength") {
            updateProperty("SignalStrength", v.toUInt());
        } else if (n == "State") {
            update_status(v);
            process_services();
        } else if (n == "Type") {
            updateProperty("NetworkType", net_type_map_[v.toString()]);
        }
    };

    for (QString n : {"Name", "Strength", "Type"})
        update(n, props[n]);

    update_status(props["State"]);

    service_.reset(new Service(service_name, path, bus_));
    connect(service_.get(), &Service::PropertyChanged
            , [update](QString const &n, QDBusVariant const&v) {
                update(n, v.variant());
            });
    return status;
}

void Bridge::process_services()
{
    current_service_ = "";
    service_.reset();

    auto process = [this](PathPropertiesArray const &services) {

        if (services.begin() == services.end()) {
            qDebug() << "No services";
            reset_properties();
        } else {
            for (auto pp = services.begin(); pp != services.end(); ++pp) {
                auto const &path = std::get<0>(*pp).path();
                auto const &props = std::get<1>(*pp);
                // first connection provided by connman according to
                // connman docs is default, so monitor only it if it
                // is online
                if (process_service(path, props) == Status::Online)
                    break;
            }
        }
    };
    sync(manager_->GetServices(), process);
}

void Bridge::process_technology(QString const &path
                                , QVariantMap const &props)
{
    auto net_type = props["Type"].toString();

    auto update_tethering = [this, net_type](QVariant const &v) {
        // TODO handle potential cases if there is more than one tech
        // of one type
        if (v.toBool())
            tethering_.insert(net_type);
        else
            tethering_.remove(net_type);

        auto len = tethering_.size();
        QString teth_str;
        if (!len) {
            teth_str = "";
        } else if (len == 1) {
            teth_str = *tethering_.begin();
        } else {
            QStringList values(QStringList::fromSet(tethering_));
            teth_str = values.join("\n");
        }
        updateProperty("Tethering", teth_str);
        qDebug() << "Technology (type=" << net_type << ") tethering is " << teth_str;
    };

    auto update = [update_tethering](QString const &n, QVariant const &v) {
        if (n == "Tethering")
            update_tethering(v);
    };

    update_tethering(props["Tethering"]);
    auto tech = cor::make_unique<Technology>(service_name, path, bus_);
    connect(tech.get(), &Technology::PropertyChanged
            , [update](QString const &n, QDBusVariant const&v) {
                update(n, v.variant());
            });

    technologies_.insert(std::make_pair(path, std::move(tech)));
}

InternetNs::InternetNs(QDBusConnection &bus)
    : Namespace("Internet", std::unique_ptr<PropertiesSource>
                (new Bridge(this, bus)))
    , defaults_({{"NetworkType", ""}
            , {"NetworkState", "disconnected"}
            , {"NetworkName", ""}
            , {"SignalStrength", "0"}
            , {"Tethering", ""}})
{
    addProperty("NetworkType", "");
    addProperty("NetworkState", "disconnected");
    addProperty("NetworkName", "");
    addProperty("TrafficIn", "0");
    addProperty("TrafficOut", "0");
    addProperty("SignalStrength", "0");
    addProperty("Tethering", "");
    src_->init();
}

void InternetNs::reset_properties()
{
    setProperties(defaults_);
}

class Provider;
static Provider *provider = nullptr;

class Provider : public statefs::AProvider
{
public:
    Provider(statefs_server *server)
        : AProvider("connman", server)
        , bus_(QDBusConnection::systemBus())
    {
        auto ns = std::make_shared<InternetNs>(bus_);
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
    return statefs::connman::init_provider(server);
}
