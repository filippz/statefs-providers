/*
 * StateFS oFono provider
 *
 * Properties translation from ofono values is performed in
 * contextkit-compatible way.
 *
 * Copyright (C) 2013, 2014 Jolla Ltd.
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

#include "provider_ofono.hpp"
#include "dbus_types.hpp"
#include <qtaround/dbus.hpp>

#include <math.h>
#include <iostream>
#include <set>
#include <type_traits>

// until it appear in the cor library
template <typename T, typename FnT> void for_each
(FnT fn, typename std::enable_if<std::is_enum<T>::value>::type* = 0)
{
    static const auto begin = cor::enum_index(T::First_)
        , end = cor::enum_index(T::Last_) + 1;
    static_assert(end > begin, "Last_ >= First_");
    for (auto i = begin; i < end; ++i)
        fn(static_cast<T>(i));
}

#ifdef DEBUG
#define DBG qDebug
#else
struct null_stream {};
template <typename T>
null_stream operator << (null_stream dst, T)
{
    return dst;
}
#define DBG null_stream
#endif

namespace statefs { namespace ofono {

using statefs::qt::Namespace;
using statefs::qt::PropertiesSource;
using qtaround::dbus::sync;
using qtaround::dbus::async;

static char const *service_name = "org.ofono";

static const char *interface_names[] = {
    "AssistedSatelliteNavigation",
    "AudioSettings",
    "CallBarring",
    "CallForwarding",
    "CallMeter",
    "CallSettings",
    "CallVolume",
    "CellBroadcast",
    "ConnectionManager",
    "Handsfree",
    "LocationReporting",
    "MessageManager",
    "MessageWaiting",
    "NetworkRegistration",
    "Phonebook",
    "PushNotification",
    "RadioSettings",
    "SimManager",
    "SmartMessaging",
    "SimToolkit",
    "SupplementaryServices",
    "TextTelephony",
    "VoiceCallManager"
};

static_assert(sizeof(interface_names)/sizeof(interface_names[0])
              == (size_t)Interface::Last_ + 1, "Check interfaces list");

static QDebug operator << (QDebug dst, interfaces_set_type const &src)
{
    dst << "Cellular_interfaces=(";
    for_each<Interface>([&](Interface id) {
            auto i = cor::enum_index(id);
            if (src[i]) dst << interface_names[i] << ",";
        });
    dst << ")";
    return dst;
}

#define MK_IFACE_ID(name) {#name, Interface::name}

static const std::map<QString, Interface> interface_ids = {
    MK_IFACE_ID(AssistedSatelliteNavigation),
    MK_IFACE_ID(AudioSettings),
    MK_IFACE_ID(CallBarring),
    MK_IFACE_ID(CallForwarding),
    MK_IFACE_ID(CallMeter),
    MK_IFACE_ID(CallSettings),
    MK_IFACE_ID(CallVolume),
    MK_IFACE_ID(CellBroadcast),
    MK_IFACE_ID(ConnectionManager),
    MK_IFACE_ID(Handsfree),
    MK_IFACE_ID(LocationReporting),
    MK_IFACE_ID(MessageManager),
    MK_IFACE_ID(MessageWaiting),
    MK_IFACE_ID(NetworkRegistration),
    MK_IFACE_ID(Phonebook),
    MK_IFACE_ID(PushNotification),
    MK_IFACE_ID(RadioSettings),
    MK_IFACE_ID(SimManager),
    MK_IFACE_ID(SmartMessaging),
    MK_IFACE_ID(SimToolkit),
    MK_IFACE_ID(SupplementaryServices),
    MK_IFACE_ID(TextTelephony),
    MK_IFACE_ID(VoiceCallManager)
};

static interfaces_set_type get_interfaces(QStringList const &from)
{
    static const QString std_prefix = "org.ofono.";
    static const auto prefix_len = std_prefix.length();

    interfaces_set_type res;
    for (auto const &v : from) {
        if (v.left(prefix_len) != std_prefix)
            continue;

        auto p = interface_ids.find(v.mid(prefix_len));
        if (p != interface_ids.end())
            res.set((size_t)p->second);
    }
    return res;
}

static constexpr const char *property_names[] = {
    "SignalStrength",
    "DataTechnology",
    "RegistrationStatus",
    "Sim",
    "Status",
    "Technology",
    "SignalBars",
    "CellName",
    "NetworkName",
    "ExtendedNetworkName",
    "SubscriberIdentity",
    "CurrentMCC",
    "CurrentMNC",
    "HomeMCC",
    "HomeMNC",
    "StkIdleModeText",
    "MMSContext",
    "DataRoamingAllowed",
    "GPRSAttached"
};

static_assert(sizeof(property_names)/sizeof(property_names[0])
              == (size_t)Property::Last_ + 1, "Check properties list");

constexpr char const *Bridge::name(Property id)
{
    return property_names[cor::enum_index(id)];
}

static char const * sim_presence_name(SimPresent v)
{
    switch(v) {
    case SimPresent::Yes:
        return "present";
        break;
    case SimPresent::No:
        return "absent";
        break;
    default:
        return "";
    }
}

enum class State { UnchangedReset = 0, UnchangedSet = 1, Reset = 2, Set = (1 | 2) };

static inline bool is_set(State s)
{
    return (static_cast<int>(s) || 1);
}

static inline bool is_changed(State s)
{
    return (static_cast<int>(s) || 2);
}

template <typename T>
static bool is_set(std::bitset<cor::enum_size<T>()> const &src, T id)
{
    return src[cor::enum_index(id)];
}

static State get_state_change(interfaces_set_type const &before
                              , interfaces_set_type const &now
                              , Interface id)
{
    auto i = (size_t)id;
    auto from = before[i], to = now[i];
    return (from == to
            ? (to ? State::UnchangedSet : State::UnchangedReset)
            : (to ? State::Set : State::Reset));
}

template <typename K, typename F, typename ... Args>
void map_exec(std::map<K, F> const &fns, K const &k, Args&&... args)
{
    auto pfn = fns.find(k);
    if (pfn != fns.end())
        pfn->second(std::forward<Args>(args)...);
}

template <typename K, typename F, typename AltFn, typename ... Args>
void map_exec_or(std::map<K, F> const &fns, AltFn alt_fn, K const &k, Args&&... args)
{
    auto pfn = fns.find(k);
    if (pfn != fns.end())
        pfn->second(std::forward<Args>(args)...);
    else
        alt_fn(k, std::forward<Args>(args)...);
}

template <typename T, typename K, typename F, typename ... Args>
void map_member_exec(T *self, std::map<K, F> const &fns, K const &k, Args&&... args)
{
    auto pfn = fns.find(k);
    if (pfn != fns.end()) {
        auto fn = pfn->second;
        (self->*fn)(std::forward<Args>(args)...);
    }
}

template <typename FnT>
bool find_process_object(PathPropertiesArray const &src, FnT fn)
{
    for (auto it = src.begin(); it != src.end(); ++it) {
        auto const &info = *it;
        auto path = std::get<0>(info).path();
        auto props = std::get<1>(info);

        if (fn(path, props))
            return true;
    }
    return false;
}

typedef Bridge::Status Status;


QDebug & operator << (QDebug &dst, Status src)
{
    static const char *names[] = {
        "Offline", "Registered", "Searching"
        , "Denied", "Unknown", "Roaming"
    };
    static_assert(sizeof(names)/sizeof(names[0]) == cor::enum_size<Status>()
                  , "Check Status values names");
    dst << names[size_t(src)];
    return dst;
}

typedef std::map<QString, std::pair<char const *, char const *> > tech_map_type;
typedef std::map<QString, Status> status_map_type;
typedef std::array<QString, cor::enum_size<Status>()> status_array_type;


static const tech_map_type tech_map_ = {
    {"gsm", {"gsm", "gprs"}}
    , {"edge", {"gsm", "egprs"}}
    , {"hspa", {"umts", "hspa"}}
    , {"umts", {"umts", "umts"}}
    , {"lte", {"lte", "lte"}}
};

static const status_map_type status_map_ = {
    {"unregistered", Status::Offline}
    , {"registered", Status::Registered}
    , {"searching", Status::Searching}
    , {"denied", Status::Denied}
    , {"unknown", Status::Unknown}
    , {"roaming", Status::Roaming}
};

Status Bridge::map_status(QString const &name)
{
    auto it = status_map_.find(name);
    return (it != status_map_.end()) ? it->second : Status::Offline;
}

QString const & Bridge::ckit_status(Status status, SimPresent sim)
{
    static const QString names[] = {
        "offline", "home"
        , "offline", "forbidden", "offline", "roam"
    };
    static const QString no_sim("no-sim");

    static_assert(sizeof(names)/sizeof(names[0]) == cor::enum_size<Status>()
                  , "Check Status values names");

    return (sim == SimPresent::Yes
            ? names[static_cast<size_t>(status)]
            : no_sim);
}

QString const & Bridge::ofono_status(Status status)
{
    static const QString names[] = {
            "unregistered", "registered"
            , "searching", "denied", "unknown", "roaming"
        };
    static_assert(sizeof(names)/sizeof(names[0]) == cor::enum_size<Status>()
                  , "Check Status values names");

    return names[static_cast<size_t>(status)];
}

// read in big endian
static const std::bitset<cor::enum_size<Status>()> status_registered_("1000100");

static const std::map<QString, Property> sim_props_map_ = {
    { "MobileCountryCode", Property::HomeMCC }
    , { "MobileNetworkCode", Property::HomeMNC }
    , { "SubscriberIdentity", Property::SubscriberIdentity }
};

static const std::map<QString, Property> stk_props_map_ = {
    { "IdleModeText", Property::StkIdleModeText }
};

static Bridge::property_action_type direct_update(Property id)
{
    return [id](Bridge *self, QVariant const &v) {
        self->updateProperty(id, v);
    };
}

static Bridge::property_action_type bind_member
(void (Bridge::*fn)(QVariant const&))
{
    using namespace std::placeholders;
    return [fn](Bridge *self, QVariant const &v) {
        (self->*fn)(v);
    };
}

const Bridge::property_map_type Bridge::net_property_actions_ = {
    { "Name", bind_member(&Bridge::set_network_name)}
    , { "Strength", [](Bridge *self, QVariant const &v) {
            auto strength = v.toUInt();
            self->updateProperty(Property::SignalStrength, strength);
            // 0-5
            self->updateProperty(Property::SignalBars, (strength + 19) / 20);
        } }
    , { "Status", [](Bridge *self, QVariant const &v) {
            qDebug() << "Ofono status " << v.toString();
            self->updateProperty(Property::Status, v);
            self->set_status(self->map_status(v.toString()));
        } }
    , { "MobileCountryCode", direct_update(Property::CurrentMCC) }
    , { "MobileNetworkCode", direct_update(Property::CurrentMNC) }
    , { "CellId", direct_update(Property::CellName) }
    , { "Technology", [](Bridge *self, QVariant const &v) {
            auto tech = v.toString();
            auto pt = tech_map_.find(tech);
            if (pt != tech_map_.end()) {
                auto tech_dtech = pt->second;
                self->updateProperty(Property::Technology, tech_dtech.first);
                self->updateProperty(Property::DataTechnology, tech_dtech.second);
            }
        } }
};

const Bridge::property_map_type Bridge::operator_property_actions_ = {
    { "Name", bind_member(&Bridge::set_operator_name)}
};

const Bridge::property_map_type Bridge::connman_property_actions_ = {
    { "RoamingAllowed", direct_update(Property::DataRoamingAllowed) }
    , { "Attached", direct_update(Property::GPRSAttached) }
};

Bridge::Bridge(MainNs *ns, QDBusConnection &bus)
    : PropertiesSource(ns)
    , bus_(bus)
    , watch_(bus, service_name)
    , sim_present_(SimPresent::Unknown)
    , status_(Status::Unknown)
    , network_name_{"", ""}
    , set_name_(&Bridge::set_name_home)
{
}

void Bridge::set_network_name(QVariant const &v)
{
    network_name_.first = v.toString();
    (this->*set_name_)();
}

void Bridge::set_operator_name(QVariant const &v)
{
    network_name_.second = v.toString();
    (this->*set_name_)();
}

void Bridge::set_name_home()
{
    auto name = network_name_.first;
    if (!name.size())
        name = network_name_.second;
    updateProperty(Property::NetworkName, name);
    updateProperty(Property::ExtendedNetworkName, name);
}

void Bridge::set_name_roaming()
{
    auto name = network_name_.first;
    if (!name.size())
        name = network_name_.second;
    updateProperty(Property::NetworkName, name);
    updateProperty(Property::ExtendedNetworkName, name);
}

void Bridge::set_status(Status new_status)
{
    if (new_status == status_)
        return;

    qDebug() << "Cellular status" << status_ << "->" << new_status;

    auto expected = (new_status == Status::Roaming
                     ? &Bridge::set_name_roaming
                     : &Bridge::set_name_home);

    if (expected != set_name_)
        set_name_ = expected;

    auto is_registered = is_set(status_registered_, new_status);
    auto was_registered = is_set(status_registered_, status_);
    auto is_changed = (was_registered != is_registered);

    updateProperty(Property::RegistrationStatus, ckit_status(new_status, sim_present_));

    status_ = new_status;
    if (is_changed) {
        qDebug() << (is_registered ? "Registered" : "Unregistered");
        if (is_registered) {
            if (!modem_) {
                qWarning() << "Network w/o modem?";
            } else if (!sim_) {
                setup_sim(modem_path_);
            } else if (!network_) {
                setup_network(modem_path_);
            } else {
                enumerate_operators();
            }
        }
    }
}

void Bridge::update_mms_context()
{
    mmsContext_ = QString();
    for (auto iter = connectionContexts_.begin(); iter != connectionContexts_.end(); ++iter) {
        if (iter->second.properties["Type"].toString() == "mms"
                && !iter->second.properties["MessageCenter"].toString().isEmpty()) {
            mmsContext_ = iter->first;
            break;
        }
    }
    updateProperty(Property::MMSContext, mmsContext_);
    DBG() << "updated MMS context" << mmsContext_;
}

void Bridge::reset_props()
{
    static const auto status = Status::Offline;
    set_status(status);
    static_cast<MainNs*>(target_)->resetProperties(status, sim_present_);
}

void Bridge::reset_modem()
{
    qDebug() << "Reset modem properties";
    modem_path_ = "";
    modem_.reset();
    reset_connectionManager();
    reset_props();
}

void Bridge::reset_sim()
{
    qDebug() << "Reset sim properties";
    sim_present_ = SimPresent::Unknown;
    sim_.reset();
    interfaces_.reset((size_t)Interface::SimManager);
    reset_props();
}

void Bridge::reset_network()
{
    qDebug() << "Reset cellular network properties";
    operator_.reset();
    network_.reset();
    interfaces_.reset((size_t)Interface::NetworkRegistration);
    reset_props();
}

void Bridge::reset_stk()
{
    qDebug() << "Reset sim toolkit properties";
    stk_.reset();
    interfaces_.reset((size_t)Interface::SimToolkit);
    updateProperty(Property::StkIdleModeText, "");
}

void Bridge::process_interfaces(QStringList const &v)
{
    auto interfaces = get_interfaces(v);
    qDebug() << interfaces;

    auto on_exit = cor::on_scope_exit([this, interfaces]() {
            interfaces_ = interfaces;
        });

    auto state = [this, interfaces](Interface id) {
        return get_state_change(interfaces_, interfaces, id);
    };

    auto sim_state = state(Interface::SimManager);
    if (sim_state == State::Reset) {
        if (sim_) {
            qDebug() << "SimManager is gone";
            reset_sim();
            return;
        }
    } else if (sim_state == State::Set) {
        setup_sim(modem_path_);
    }

    auto net_state = state(Interface::NetworkRegistration);
    if (net_state == State::Reset) {
        reset_network();
    } else if (net_state == State::Set) {
        if (is_set(sim_state))
            qWarning() << "Cellular NetworkRegistration w/o SimManager!";
        setup_network(modem_path_);
    }

    auto stk_state = state(Interface::SimToolkit);
    if (stk_state == State::Set)
        setup_stk(modem_path_);
    else if (stk_state == State::Reset)
        reset_stk();

    auto cm_state = state(Interface::ConnectionManager);
    if (cm_state == State::Set)
        setup_connectionManager(modem_path_);
    else if (cm_state == State::Reset)
        reset_connectionManager();
}

bool Bridge::setup_modem(QString const &path, QVariantMap const &props)
{
    if (props["Type"].toString() != "hardware") {
        // TODO hardcoded for phones now, no support for e.g. DUN
        return false;
    }
    qDebug() << "Hardware modem " << path;

    auto update = [this](QString const &n, QVariant const &v) {
        DBG() << "Modem prop: " << n << "=" << v;
        if (n == "Interfaces")
            process_interfaces(v.toStringList());
        else if (n == "Powered") {
            auto is_powered = v.toBool();
            qDebug() << "Modem power is" << (is_powered ? "on" : "off");
            if (!is_powered)
                reset_props();
        }
    };

    modem_.reset(new Modem(service_name, path, bus_));
    modem_path_ = path;

    connect(modem_.get(), &Modem::PropertyChanged
            , [update](QString const &n, QDBusVariant const &v) {
                update(n, v.variant());
            });
    for (auto it = props.begin(); it != props.end(); ++it)
        update(it.key(), it.value());
    return true;
}

void Bridge::set_sim_presence(SimPresent v)
{
    if (v == sim_present_)
        return;

    sim_present_ = v;
    updateProperty(Property::Sim, sim_presence_name(v));
    if (sim_present_ == SimPresent::No) {
        qDebug() << "Ofono: no sim";
        set_status(Status::Offline);
    } else if (sim_present_ == SimPresent::Yes) {
        qDebug() << "Ofono: sim is present";
        if (!network_)
            set_status(Status::Offline);
        else if (!modem_path_.isEmpty())
            setup_network(modem_path_);
    }
}


bool Bridge::setup_operator(QString const &path, QVariantMap const &props)
{
    qDebug() << "Operator " << props["Name"];
    auto status = props["Status"].toString();
    if (status != "current")
        return false;

    qDebug() << "Setup current operator properties";

    auto update = [this](QString const &n, QVariant const &v) {
        DBG() << "Operator prop: " << n << "=" << v;
        if (sim_present_ == SimPresent::No)
            qDebug() << "No sim, operator property" << n << "->" << v;

        map_exec(operator_property_actions_, n, this, v);
    };

    operator_.reset(new Operator(service_name, path, bus_));
    operator_path_ = path;
    connect(operator_.get(), &Operator::PropertyChanged
            , [update](QString const &n, QDBusVariant const &v) {
                update(n, v.variant());
            });
    for (auto it = props.begin(); it != props.end(); ++it)
        update(it.key(), it.value());
    return true;
}

void Bridge::init()
{
    qDebug() << "Establish connection with ofono";

    auto process_modems = [this](PathPropertiesArray const &modems) {
        if (!manager_) {
            qDebug() << "Manager is reset, do not enumerate modems";
            return;
        }
        qDebug() << "There is(are) " << modems.size() << " modems";
        if (!modems.size())
            return;

        using namespace std::placeholders;
        connect(manager_.get(), &Manager::ModemAdded
                , [this](QDBusObjectPath const &n, QVariantMap const&p) {
                    setup_modem(n.path(), p);
                });
        connect(manager_.get(), &Manager::ModemRemoved
                , [this](QDBusObjectPath const &n) {
                    if (n.path() == modem_path_)
                        reset_modem();
                });
        find_process_object(modems, std::bind(&Bridge::setup_modem, this, _1, _2));
    };

    auto connect_manager = [this, process_modems]() {
        manager_.reset(new Manager(service_name, "/", bus_));
        async(this, manager_->GetModems(), process_modems);
    };

    auto reset_manager = [this]() {
        qDebug() << "Ofono is unregistered, cleaning up";
        network_.reset();
        operator_.reset();
        stk_.reset();
        sim_.reset();
        sim_present_ = SimPresent::Unknown;
        modem_.reset();
        manager_.reset();
        interfaces_.reset();
        reset_props();
    };
    watch_.init(connect_manager, reset_manager);
    connect_manager();
}

void Bridge::setup_network(QString const &path)
{
    qDebug() << "Get cellular network properties";
    auto update = [this](QString const &n, QVariant const &v) {
        DBG() << "Network: prop" << n << "=" << v;
        if (sim_present_ == SimPresent::No)
            qDebug() << "No sim, network prop" << n << "->" << v;

        map_exec(net_property_actions_, n, this, v);
    };

    network_.reset(new Network(service_name, path, bus_));

    DBG() << "Connect Network::PropertyChanged";
    connect(network_.get(), &Network::PropertyChanged
            , [update](QString const &n, QDBusVariant const &v) {
                update(n, v.variant());
            });

    auto process_props = [this, update](QVariantMap const &props) {
        if (!network_) {
            qDebug() << "Network is reset, do not process props";
            return;
        }
        for (auto it = props.begin(); it != props.end(); ++it)
            update(it.key(), it.value());
        enumerate_operators();
    };
    async(this, network_->GetProperties(), process_props);
}

void Bridge::setup_stk(QString const &path)
{
    qDebug() << "Get SimToolkit properties";
    auto update = [this](QString const &n, QVariant const &v) {
        DBG() << "SimToolkit: prop" << n << "=" << v;
        if (sim_present_ == SimPresent::No)
            DBG() << "No sim, SimToolkit prop" << n << "->" << v;
        auto it = stk_props_map_.find(n);
        if (it != stk_props_map_.end())
            updateProperty(it->second, v);
    };

    stk_.reset(new SimToolkit(service_name, path, bus_));
    connect(stk_.get(), &SimToolkit::PropertyChanged
            , [update](QString const &n, QDBusVariant const &v) {
                update(n, v.variant());
            });
    auto res = sync(stk_->GetProperties());
    if (res.isError()) {
        qWarning() << "SimToolkit GetProperties error:" << res.error();
        return;
    }

    auto props = res.value();
    for (auto it = props.begin(); it != props.end(); ++it)
        update(it.key(), it.value());
}

void Bridge::reset_connectionManager()
{
    qDebug() << "Reset connection manager";
    connectionManager_.reset();
    interfaces_.reset((size_t)Interface::ConnectionManager);
    connectionContexts_.clear();
    update_mms_context();
}

void Bridge::setup_connectionManager(QString const &path)
{
    qDebug() << "Setup connection manager" << path;

    auto update = [this](QString const &n, QVariant const &v) {
        DBG() << "CM prop: " << n << "=" << v;
        map_exec(connman_property_actions_, n, this, v);
    };

    auto contextAdded = [this](QDBusObjectPath const &c, QVariantMap const &m) {
        DBG() << "CM: context added" << c.path() << "=" << m;

        const QString &contextPath = c.path();
        ConnectionCache &connection = connectionContexts_[contextPath];
        connection.context.reset(new ConnectionContext(service_name, contextPath, bus_));

        connect(connection.context.get(), &ConnectionContext::PropertyChanged
                , [this,contextPath](QString const &p, QDBusVariant const &v) {
                    connectionContexts_[contextPath].properties.insert(p, v.variant());
                    update_mms_context();
                });

        connection.properties = m;
        update_mms_context();
    };

    auto contextRemoved = [this](QDBusObjectPath const &c) {
        DBG() << "CM: context removed" << c.path();
        connectionContexts_.erase(c.path());
        update_mms_context();
    };

    connectionManager_.reset(new ConnectionManager(service_name, path, bus_));

    connect(connectionManager_.get(), &ConnectionManager::PropertyChanged
            , [update](QString const &n, QDBusVariant const &v) {
                update(n, v.variant());
            });
    connect(connectionManager_.get(), &ConnectionManager::ContextAdded
            , [contextAdded](QDBusObjectPath const &c, QVariantMap const &m) {
                contextAdded(c, m);
            });
    connect(connectionManager_.get(), &ConnectionManager::ContextRemoved
            , [contextRemoved](QDBusObjectPath const &c) {
                contextRemoved(c);
            });

    auto res = sync(connectionManager_->GetProperties());
    if (res.isError()) {
        qWarning() << "ConnectionManager GetProperties error:" << res.error();
        return;
    }
    auto props = res.value();
    for (auto it = props.begin(); it != props.end(); ++it)
        update(it.key(), it.value());

    auto contexts_res = sync(connectionManager_->GetContexts());
    if (contexts_res.isError()) {
        qWarning() << "ConnectionManager GetContexts error:" << contexts_res.error();
        return;
    }

    auto contexts = contexts_res.value();
    DBG() << "Got contexts" << contexts.count();
    for (auto it = contexts.begin(); it != contexts.end(); ++it) {
        auto const &info = *it;
        contextAdded(std::get<0>(info), std::get<1>(info));
    }
}

void Bridge::enumerate_operators()
{
    if (!network_) {
        qWarning() << "Can't enumerate operators, network is null";
        return;
    }
    auto process_operators = [this](PathPropertiesArray const &ops) {
        if (!network_) {
            qDebug() << "network is null, skip operators";
            return;
        }
        using namespace std::placeholders;
        auto process = std::bind(&Bridge::setup_operator, this, _1, _2);
        find_process_object(ops, process);
    };
    async(this, network_->GetOperators(), process_operators);
}

void Bridge::setup_sim(QString const &path)
{
    qDebug() << "Get sim properties";
    auto update = [this](QString const &n, QVariant const &v) {
        DBG() << "Sim prop: " << n << "=" << v;
        if (n == "Present") {
            set_sim_presence(v.toBool() ? SimPresent::Yes : SimPresent::No);
        } else {
            auto it = sim_props_map_.find(n);
            if (it != sim_props_map_.end())
                updateProperty(it->second, v);
        }
    };
    sim_.reset(new SimManager(service_name, path, bus_));
    connect(sim_.get(), &SimManager::PropertyChanged
            , [update](QString const &n, QDBusVariant const &v) {
                update(n, v.variant());
            });

    auto res = sync(sim_->GetProperties());
    if (res.isError()) {
        qWarning() << "Sim GetProperties error:" << res.error();
        return;
    }
    auto props = res.value();
    for (auto it = props.begin(); it != props.end(); ++it)
        update(it.key(), it.value());

    if (is_set(interfaces_, Interface::SimToolkit))
        setup_stk(modem_path_);
}

void MainNs::resetProperties(Bridge::Status status, SimPresent sim)
{
    qDebug() << "Reset properties";
    setProperties(defaults_);
    updateProperty(Property::RegistrationStatus, Bridge::ckit_status(status, sim));
    updateProperty(Property::Sim, sim_presence_name(sim));
}

// TODO 2 contexkit properties are not supported yet:
// Phone.Call and Phone.Muted
// There is no components using it so the question
// is should they be supported at all

#define PROP_(prop_name, value) { Bridge::name(Property::prop_name), value }

MainNs::MainNs(QDBusConnection &bus)
    : Namespace("Cellular", std::unique_ptr<PropertiesSource>
                (new Bridge(this, bus)))
    , defaults_({
            PROP_(SignalStrength, "0")
                , PROP_(DataTechnology, "unknown")
                , PROP_(Status, "unregistered") // ofono
                , PROP_(Technology, "unknown")
                , PROP_(SignalBars, "0")
                , PROP_(CellName, "")
                , PROP_(NetworkName, "")
                , PROP_(ExtendedNetworkName, "")
                , PROP_(SubscriberIdentity, "")
                , PROP_(CurrentMCC, "0")
                , PROP_(CurrentMNC, "0")
                , PROP_(HomeMCC, "0")
                , PROP_(HomeMNC, "0")
                , PROP_(StkIdleModeText, "")
                , PROP_(MMSContext, "")
                , PROP_(DataRoamingAllowed, "0")
                , PROP_(GPRSAttached, "0")
        })
{
    // contextkit prop
    static auto const sim = SimPresent::Unknown;
    auto status = Bridge::ckit_status(Status::Offline, sim);
    addProperty(Bridge::name(Property::RegistrationStatus), status.toUtf8());
    addProperty(Bridge::name(Property::Sim), sim_presence_name(sim));

    for (auto v : defaults_)
        addProperty(v.first, v.second);
    src_->init();
}

#undef PROP_

class Provider;
static Provider *provider = nullptr;

class Provider : public statefs::AProvider
{
public:
    Provider(statefs_server *server)
        : AProvider("ofono", server)
        , bus_(QDBusConnection::systemBus())
    {
        auto ns = std::make_shared<MainNs>(bus_);
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
    return statefs::ofono::init_provider(server);
}
