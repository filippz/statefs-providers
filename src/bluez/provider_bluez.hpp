#ifndef _STATEFS_PRIVATE_BLUEZ_HPP_
#define _STATEFS_PRIVATE_BLUEZ_HPP_

#include "qdbusxml2cpp_manager_interface.h"
#include "qdbusxml2cpp_adapter_interface.h"
#include "qdbusxml2cpp_device_interface.h"

#include <statefs/provider.hpp>
#include <statefs/property.hpp>
#include <statefs/qt/ns.hpp>
#include <set>

#include <QObject>

namespace statefs { namespace bluez {

typedef OrgBluezManagerInterface Manager;
typedef OrgBluezAdapterInterface Adapter;
typedef OrgBluezDeviceInterface Device;

using qtaround::dbus::ServiceWatch;

class BlueZ;

class Bridge : public QObject, public statefs::qt::PropertiesSource
{
    Q_OBJECT
public:

    Bridge(BlueZ *, QDBusConnection &);

    virtual ~Bridge() {}

    virtual void init();

private slots:
    void defaultAdapterChanged(const QDBusObjectPath &);
    void addDevice(const QDBusObjectPath &v);
    void removeDevice(const QDBusObjectPath &v);

private:

    QDBusConnection &bus_;
    QDBusObjectPath defaultAdapter_;
    std::unique_ptr<Manager> manager_;
    std::unique_ptr<Adapter> adapter_;
    std::map<QDBusObjectPath,std::unique_ptr<Device> > devices_;
    std::set<QDBusObjectPath> connected_;
    ServiceWatch watch_;
};

class BlueZ : public statefs::qt::Namespace
{
public:

    BlueZ(QDBusConnection &bus);

private:
    friend class Bridge;
    void reset_properties();
    statefs::qt::DefaultProperties defaults_;
};

}}

#endif // _STATEFS_PRIVATE_BLUEZ_HPP_
