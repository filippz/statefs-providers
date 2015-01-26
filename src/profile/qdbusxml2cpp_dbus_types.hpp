#ifndef _STATEFS_PROVIDERS_PROFILE_DBUS_TYPEs_HPP_
#define _STATEFS_PROVIDERS_PROFILE_DBUS_TYPEs_HPP_

#include <tuple>
#include <QList>
#include <qtaround/dbus.hpp>

typedef std::tuple<QString, QString, QString> ProfileData;
Q_DECLARE_METATYPE(ProfileData);

typedef QList<ProfileData> ProfileDataList;
Q_DECLARE_METATYPE(ProfileDataList);

static inline void registerDataTypes()
{
    qDBusRegisterMetaType<ProfileData>();
    qDBusRegisterMetaType<ProfileDataList>();
}

#endif // _STATEFS_PROVIDERS_PROFILE_DBUS_TYPEs_HPP_
