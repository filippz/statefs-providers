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

set(DEV_IF DeviceKit.Power.Device.xml)
set(MGR_IF DeviceKit.Power.xml) 
set(PROP_IF DBus.Properties.xml) 

set_source_files_properties(${MGR_IF} ${DEV_IF} ${PROP_IF}
  PROPERTIES NO_NAMESPACE TRUE
  )

qt5_add_dbus_interface(SRC ${MGR_IF} qdbusxml2cpp_manager_interface)
qt5_add_dbus_interface(SRC ${DEV_IF} qdbusxml2cpp_device_interface)
qt5_add_dbus_interface(SRC ${PROP_IF} qdbusxml2cpp_prop_interface)

add_library(provider-upower SHARED
  ${SRC}
  provider_upower.cpp
  )

target_link_libraries(provider-upower
  statefs-providers-qt5
  ${CMAKE_THREAD_LIBS_INIT}
  ${STATEFS_LIBRARIES}
  ${QTAROUND_DBUS_LIBRARIES}
  ${Qt5Core_LIBRARIES}
  ${Qt5DBus_LIBRARIES}
  )

install(TARGETS provider-upower DESTINATION ${DST_LIB}/statefs)
