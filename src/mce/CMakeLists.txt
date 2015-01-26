set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC TRUE)
set(SELF mce)

pkg_check_modules(MCE mce REQUIRED)

include_directories(
  ${Qt5Core_INCLUDE_DIRS}
  ${Qt5DBus_INCLUDE_DIRS}
  ${MCE_INCLUDE_DIRS}
)

link_directories(
  ${Qt5Core_LIBRARY_DIRS}
  ${Qt5DBus_LIBRARY_DIRS}
  ${MCE_LIBRARY_DIRS}
)

set(MCE_IF mce.xml)
set(ALL_IF ${MCE_IF})
# set_source_files_properties(${ALL_IF}
#   PROPERTIES INCLUDE dbus_types.hpp
#   )
set_source_files_properties(${ALL_IF}
  PROPERTIES NO_NAMESPACE TRUE
  )


qt5_add_dbus_interface(SRC ${MCE_IF} qdbusxml2cpp_mce_interface)

add_library(provider-${SELF} SHARED
  ${SRC}
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
