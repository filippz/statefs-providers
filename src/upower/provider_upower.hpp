#ifndef _STATEFS_PRIVATE_BLUEZ_HPP_
#define _STATEFS_PRIVATE_BLUEZ_HPP_

#include "qdbusxml2cpp_manager_interface.h"
#include "qdbusxml2cpp_device_interface.h"
#include "qdbusxml2cpp_prop_interface.h"

#include <statefs/provider.hpp>
#include <statefs/property.hpp>
#include <statefs/qt/ns.hpp>
#include <qtaround/dbus.hpp>

#include <QObject>

using qtaround::dbus::ServiceWatch;

namespace statefs { namespace upower {

typedef OrgFreedesktopUPowerInterface Manager;
typedef OrgFreedesktopUPowerDeviceInterface Device;
typedef OrgFreedesktopDBusPropertiesInterface Properties;

class PowerNs;

class Bridge : public QObject, public statefs::qt::PropertiesSource
{
    Q_OBJECT;
public:

    // see upower docs
    enum DeviceState {
        UnknownState = 0,
        Charging,
        Discharging,
        Empty,
        FullyCharged,
        PendingCharge,
        PendingDischarge
    };

    // see upower docs
    enum DeviceType {
        UnknownDevice,
        LinePower,
        Battery,
        Ups,
        Monitor,
        Mouse,
        Keyboard,
        Pda,
        Phone
    };

    enum class Prop {
        Percentage = 0, OnBattery, LowBattery, TimeToEmpty, TimeToFull, State, EOE
    };

    Bridge(PowerNs *, QDBusConnection &bus);

    virtual ~Bridge() {}

    virtual void init();

private slots:
    void update_all_props();
    bool try_get_battery(QString const &);

private:

    void init_manager();
    void reset_device();

    QDBusConnection &bus_;
    QDBusObjectPath defaultAdapter_;

    std::unique_ptr<Manager> manager_;
    std::unique_ptr<Properties> manager_props_;

    std::unique_ptr<Device> device_;
    std::unique_ptr<Properties> device_props_;

    QString device_path_;
    ServiceWatch watch_;

    static const size_t propCount = static_cast<size_t>(Prop::EOE);
    typedef std::array<QVariant, propCount> state_type;
    static const QMap<QString, Prop> state_ids_;
    static const state_type default_state_;
    state_type last_state_;
    state_type new_state_;

    // use std::function as it more flexible while it takes more
    // memory to have std::function with bindings instead of
    // e.g. pointers to member functions
    typedef std::function<void (Prop, QVariant const&)> action_type;
    typedef std::array<action_type, propCount> actions_type;
    actions_type actions_;
    actions_type construct_actions();
};

class PowerNs : public statefs::qt::Namespace
{
public:

    PowerNs(QDBusConnection &bus);
private:
    statefs::qt::DefaultProperties defaults_;
};

}}

#endif // _STATEFS_PRIVATE_BLUEZ_HPP_
