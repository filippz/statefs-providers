PROJECT(statefs-providers)
cmake_minimum_required(VERSION 2.8)

IF(NOT DEFINED VERSION)
MESSAGE(FATAL_ERROR "Define VERSION")
ENDIF(NOT DEFINED VERSION)

find_package(PkgConfig REQUIRED)
set(prefix ${CMAKE_INSTALL_PREFIX})
find_package(Cor)

find_package(Threads)

find_package(Qt5Core REQUIRED)
find_package(Qt5DBus REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

pkg_check_modules(STATEFS statefs-cpp REQUIRED)
pkg_check_modules(STATEFS_QT5 statefs-qt5 REQUIRED)
pkg_check_modules(QTAROUND_DBUS qtaround-dbus REQUIRED)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${STATEFS_INCLUDES}
  ${STATEFS_QT5_INCLUDES}
  ${QTAROUND_DBUS_INCLUDE_DIRS}
)

link_directories(
  ${STATEFS_LIBS}
  ${STATEFS_QT5_LIBS}
  ${QTAROUND_DBUS_LIBRARY_DIRS}
)

install(DIRECTORY include/statefs DESTINATION include/qt5)

add_subdirectory(src/util)
add_subdirectory(src/bluez)
add_subdirectory(src/bme)
add_subdirectory(src/upower)
add_subdirectory(src/connman)
add_subdirectory(src/ofono)

pkg_check_modules(MCE mce)
if (MCE_FOUND)
  add_subdirectory(src/mce)
endif()
add_subdirectory(src/profile)
add_subdirectory(src/keyboard_generic)
add_subdirectory(src/udev)
add_subdirectory(src/back_cover)
add_subdirectory(tests)
