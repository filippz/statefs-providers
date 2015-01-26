#ifndef _STATEFS_PRIVATE_CONNMAN_HPP_
#define _STATEFS_PRIVATE_CONNMAN_HPP_

#include "qdbusxml2cpp_manager_interface.h"
#include "qdbusxml2cpp_service_interface.h"
#include "qdbusxml2cpp_technology_interface.h"

#include <statefs/provider.hpp>
#include <statefs/property.hpp>
#include <statefs/qt/ns.hpp>
#include <qtaround/dbus.hpp>

#include <map>
#include <set>
#include <QDBusConnection>
#include <QString>
#include <QVariant>
#include <QObject>

namespace statefs { namespace connman {

typedef NetConnmanManagerInterface Manager;
typedef NetConnmanServiceInterface Service;
typedef NetConnmanTechnologyInterface Technology;
using qtaround::dbus::ServiceWatch;

class InternetNs;

enum class Status { Offline, Online, EOE };

class Bridge : public QObject, public statefs::qt::PropertiesSource
{
    Q_OBJECT;
public:
    Bridge(InternetNs *, QDBusConnection &bus);

    virtual ~Bridge() {}

    virtual void init();

private:

    void process_manager_props(QVariantMap const&);
    void process_technology(QString const&, QVariantMap const&);
    void process_services();
    void process_technologies();
    Status process_service(QString const&, QVariantMap const &);
    void reset_manager();
    void reset_properties();

    QDBusConnection &bus_;
    std::unique_ptr<ServiceWatch> watch_;
    std::unique_ptr<Manager> manager_;
    std::unique_ptr<Service> service_;
    std::map<QString, std::unique_ptr<Technology> > technologies_;

    QString current_service_;
    std::map<QString, QString> net_type_map_;
    std::map<QString, Status> state_map_;
    std::vector<QString> states_;

    QSet<QString> tethering_;
};

class InternetNs : public statefs::qt::Namespace
{
public:
    InternetNs(QDBusConnection &bus);
private:
    friend class Bridge;
    void reset_properties();
    statefs::qt::DefaultProperties defaults_;
};

}}

#endif // _STATEFS_PRIVATE_CONNMAN_HPP_
