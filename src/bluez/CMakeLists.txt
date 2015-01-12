set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC TRUE)

include_directories(
  ${Qt5Core_INCLUDE_DIRS}
  ${Qt5DBus_INCLUDE_DIRS}
)

link_directories(
  ${Qt5Core_LIBRARY_DIRS}
  ${Qt5DBus_LIBRARY_DIRS}
)

set(MGR_IF org.bluez.Manager.xml)
set(ADA_IF org.bluez.Adapter.xml) 
set(DEV_IF org.bluez.Device.xml)
#set(ALL_IF org.bluez.all.xml)

set_source_files_properties(${MGR_IF} ${ADA_IF} ${DEV_IF}
  PROPERTIES INCLUDE common.h
  )

qt5_add_dbus_interface(SRC ${MGR_IF} manager_interface)
qt5_add_dbus_interface(SRC ${ADA_IF} adapter_interface)
qt5_add_dbus_interface(SRC ${DEV_IF} device_interface)
#qt5_add_dbus_interface(SRC ${ALL_IF} all)

add_library(provider-bluez SHARED
  ${SRC}
  provider_bluez.cpp
  )

target_link_libraries(provider-bluez
  statefs-providers-qt5
  ${CMAKE_THREAD_LIBS_INIT}
  ${STATEFS_LIBRARIES}
  ${QTAROUND_DBUS_LIBRARIES}
  ${Qt5Core_LIBRARIES}
  ${Qt5DBus_LIBRARIES}
  )

install(TARGETS provider-bluez DESTINATION ${DST_LIB}/statefs)
