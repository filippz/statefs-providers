#ifndef _STATEFS_PRIVATE_CONNMAN_HPP_
#define _STATEFS_PRIVATE_CONNMAN_HPP_
/**
 * @file provider_ofono.hpp
 * @brief Statefs ofono provider
 * @copyright (C) 2013, 2014 Jolla Ltd.
 * @par License: LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include "manager_interface.h"
#include "net_interface.h"
#include "sim_interface.h"
#include "stk_interface.h"
#include "connectionmanager_interface.h"
#include "connectioncontext_interface.h"

#include <statefs/provider.hpp>
#include <statefs/property.hpp>
#include <statefs/qt/ns.hpp>
#include <qtaround/dbus.hpp>
#include <cor/util.hpp>

#include <QDBusConnection>
#include <QString>
#include <QVariant>
#include <QObject>

#include <map>
#include <bitset>

namespace statefs { namespace ofono {

typedef OrgOfonoManagerInterface Manager;
typedef OrgOfonoNetworkRegistrationInterface Network;
typedef OrgOfonoNetworkOperatorInterface Operator;
typedef OrgOfonoModemInterface Modem;
typedef OrgOfonoSimManagerInterface SimManager;
typedef OrgOfonoSimToolkitInterface SimToolkit;
typedef OrgOfonoConnectionManagerInterface ConnectionManager;
typedef OrgOfonoConnectionContextInterface ConnectionContext;

using qtaround::dbus::ServiceWatch;

enum class Interface {
    AssistedSatelliteNavigation,
    First_ = AssistedSatelliteNavigation,
    AudioSettings,
    CallBarring,
    CallForwarding,
    CallMeter,
    CallSettings,
    CallVolume,
    CellBroadcast,
    ConnectionManager,
    Handsfree,
    LocationReporting,
    MessageManager,
    MessageWaiting,
    NetworkRegistration,
    Phonebook,
    PushNotification,
    RadioSettings,
    SimManager,
    SmartMessaging,
    SimToolkit,
    SupplementaryServices,
    TextTelephony,
    VoiceCallManager,
    Last_ = VoiceCallManager
};

enum class SimPresent { Unknown, No, Yes, Last_ = Yes };

enum class Property {
    SignalStrength, First_ = SignalStrength,
    DataTechnology,
    RegistrationStatus,
    Sim,
    Status,
    Technology,
    SignalBars,
    CellName,
    NetworkName,
    ExtendedNetworkName,
    SubscriberIdentity,
    CurrentMCC,
    CurrentMNC,
    HomeMCC,
    HomeMNC,
    StkIdleModeText,
    MMSContext,
    DataRoamingAllowed,
    GPRSAttached, Last_ = GPRSAttached
};

struct ConnectionCache
{
    std::unique_ptr<ConnectionContext> context;
    QVariantMap properties;
};

typedef std::bitset<cor::enum_size<Interface>()> interfaces_set_type;

class MainNs;
enum class State;
using statefs::qt::PropertiesSource;

class Bridge : public QObject, public PropertiesSource
{
    Q_OBJECT
public:
    Bridge(MainNs *, QDBusConnection &bus);

    virtual ~Bridge() {}

    virtual void init();

    typedef std::function<void(Bridge*, QVariant const&)> property_action_type;
    typedef std::map<QString, property_action_type> property_map_type;

    enum class Status {
        Offline, Registered, Searching, Denied, Unknown, Roaming
            , Last_ = Roaming
            };

    void set_status(Status);
    void set_network_name(QVariant const &);
    void set_operator_name(QVariant const &);
    void set_name_home();
    void set_name_roaming();
    void update_mms_context();

    static Status map_status(QString const&);
    static QString const & ckit_status(Status, SimPresent);
    static QString const & ofono_status(Status);
    static constexpr char const *name(Property);

    template <typename T>
    void updateProperty(Property id, T &&value)
    {
        PropertiesSource::updateProperty(name(id), std::forward<T>(value));
    }

private:

    bool setup_modem(QString const &, QVariantMap const&);
    bool setup_operator(QString const &, QVariantMap const&);
    void setup_sim(QString const &);
    void setup_network(QString const &);
    void setup_stk(QString const &);
    void setup_connectionManager(QString const &);
    void reset_sim();
    void reset_network();
    void reset_modem();
    void reset_stk();
    void reset_connectionManager();
    void reset_props();
    void process_interfaces(QStringList const&);
    void enumerate_operators();
    void set_sim_presence(SimPresent);

    QDBusConnection &bus_;
    interfaces_set_type interfaces_;
    std::unique_ptr<Manager> manager_;
    std::unique_ptr<Modem> modem_;
    std::unique_ptr<Network> network_;
    std::unique_ptr<Operator> operator_;
    std::unique_ptr<SimManager> sim_;
    std::unique_ptr<SimToolkit> stk_;
    std::unique_ptr<ConnectionManager> connectionManager_;
    std::map<QString,ConnectionCache> connectionContexts_;
    ServiceWatch watch_;

    SimPresent sim_present_;
    bool supports_stk_;
    Status status_;
    std::pair<QString, QString> network_name_;
    void (Bridge::*set_name_)();

    QString modem_path_;
    QString operator_path_;
    QString mmsContext_;

    static const property_map_type net_property_actions_;
    static const property_map_type operator_property_actions_;
    static const property_map_type connman_property_actions_;
};

using statefs::qt::Namespace;

class MainNs : public statefs::qt::Namespace
{
public:
    MainNs(QDBusConnection &bus);

    template <typename T>
    void updateProperty(Property id, T &&value)
    {
        Namespace::updateProperty(Bridge::name(id), std::forward<T>(value));
    }

private:
    friend class Bridge;
    void resetProperties(Bridge::Status, SimPresent);

    statefs::qt::DefaultProperties defaults_;
};

}}

#endif // _STATEFS_PRIVATE_CONNMAN_HPP_
