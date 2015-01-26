#ifndef _STATEFS_PRIVATE_CONNMAN_HPP_
#define _STATEFS_PRIVATE_CONNMAN_HPP_

#include "qdbusxml2cpp_mce_interface.h"

#include <statefs/provider.hpp>
#include <statefs/property.hpp>
#include <statefs/qt/ns.hpp>
#include <qtaround/dbus.hpp>

#include <map>
#include <QDBusConnection>
#include <QString>
#include <QVariant>
#include <QObject>

namespace statefs { namespace mce {

typedef ComNokiaMceRequestInterface MceRequest;
typedef ComNokiaMceSignalInterface MceSignal;
using qtaround::dbus::ServiceWatch;

class MceNs;

class Bridge : public QObject, public statefs::qt::PropertiesSource
{
    Q_OBJECT;
public:
    Bridge(MceNs *, QDBusConnection &bus);

    virtual ~Bridge() {}

    virtual void init();

private:
    void init_request();

    QDBusConnection &bus_;
    std::unique_ptr<ServiceWatch> watch_;
    std::unique_ptr<MceRequest> request_;
    std::unique_ptr<MceSignal> signal_;
};

class ScreenNs;

class MceNs : public statefs::qt::Namespace
{
public:
    MceNs(QDBusConnection &bus, std::shared_ptr<ScreenNs> const &);

private:
    friend class Bridge;
    void set_blanked(bool);
    void reset_properties();

    std::shared_ptr<ScreenNs> screen_;
    statefs::qt::DefaultProperties defaults_;
};

class ScreenNs : public statefs::Namespace
{
public:
    ScreenNs();
    virtual ~ScreenNs() {}
    virtual void release() { }
private:
    friend class MceNs;
    void set_blanked(bool);

    statefs::setter_type set_blanked_;
};

}}

#endif // _STATEFS_PRIVATE_CONNMAN_HPP_
