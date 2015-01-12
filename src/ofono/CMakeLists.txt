set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC TRUE)
set(SELF ofono)

include_directories(
  ${Qt5Core_INCLUDE_DIRS}
  ${Qt5DBus_INCLUDE_DIRS}
)

link_directories(
  ${Qt5Core_LIBRARY_DIRS}
  ${Qt5DBus_LIBRARY_DIRS}
)

set(MGR_IF org.ofono.manager.xml)
set(NET_IF org.ofono.networkregistration.xml) 
set(SIM_IF org.ofono.simmanager.xml)
set(STK_IF org.ofono.simtoolkit.xml)
set(CONMGR_IF org.ofono.connectionmanager.xml)
set(CONCXT_IF org.ofono.connectioncontext.xml)
set(ALL_IF ${MGR_IF} ${NET_IF} ${SIM_IF} ${CONMGR_IF} ${CONCXT_IF})
set_source_files_properties(${ALL_IF}
  PROPERTIES INCLUDE dbus_types.hpp
  )
set_source_files_properties(${ALL_IF}
  PROPERTIES NO_NAMESPACE TRUE
  )

qt5_add_dbus_interface(SRC ${MGR_IF} manager_interface)
qt5_add_dbus_interface(SRC ${NET_IF} net_interface)
qt5_add_dbus_interface(SRC ${SIM_IF} sim_interface)
qt5_add_dbus_interface(SRC ${STK_IF} stk_interface)
qt5_add_dbus_interface(SRC ${CONMGR_IF} connectionmanager_interface)
qt5_add_dbus_interface(SRC ${CONCXT_IF} connectioncontext_interface)
# qt5_wrap_cpp(STATEFS_QT_SRC ${STATEFS_QT_HEADERS})

add_library(provider-${SELF} SHARED
  ${SRC}
  # ${STATEFS_QT_SRC}
  provider_${SELF}.cpp
  )

target_link_libraries(provider-${SELF}
  statefs-providers-qt5
  ${CMAKE_THREAD_LIBS_INIT}
  ${STATEFS_LIBRARIES}
  ${QTAROUND_DBUS_LIBRARIES}
  ${Qt5Core_LIBRARIES}
  ${Qt5DBus_LIBRARIES}
  )

install(TARGETS provider-${SELF} DESTINATION ${DST_LIB}/statefs)


