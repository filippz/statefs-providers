#ifndef _STATEFS_PRIVATE_CONNMAN_HPP_
#define _STATEFS_PRIVATE_CONNMAN_HPP_

#include "qdbusxml2cpp_profile_interface.h"

#include <statefs/provider.hpp>
#include <statefs/property.hpp>
#include <statefs/qt/ns.hpp>
#include <qtaround/dbus.hpp>

#include <map>
#include <QDBusConnection>
#include <QString>
#include <QVariant>
#include <QObject>

namespace statefs { namespace profile {

typedef ComNokiaProfiledInterface Profile;
using qtaround::dbus::ServiceWatch;

class ProfileNs;

class Bridge : public QObject, public statefs::qt::PropertiesSource
{
    Q_OBJECT;
public:
    Bridge(ProfileNs *, QDBusConnection &bus);

    virtual ~Bridge() {}

    virtual void init();

private:
    void init_conn();

    QDBusConnection &bus_;
    std::unique_ptr<ServiceWatch> watch_;
    std::unique_ptr<Profile> profiled_;
};

class ProfileNs : public statefs::qt::Namespace
{
public:
    ProfileNs(QDBusConnection &bus);

private:
    friend class Bridge;
    void reset_properties();

    statefs::qt::DefaultProperties defaults_;
};


}}

#endif // _STATEFS_PRIVATE_CONNMAN_HPP_
