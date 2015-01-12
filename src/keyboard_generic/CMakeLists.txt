set(SELF keyboard_generic)
find_package(Cor REQUIRED)
pkg_check_modules(STATEFS_UTIL statefs-util REQUIRED)

add_definitions(-DBOOST_ASIO_DISABLE_EPOLL)
find_package(Boost COMPONENTS system REQUIRED)
set(Boost_USE_MULTITHREADED ON)
set(Boost_COMPILER -gcc)

include_directories(
  ${COR_INCLUDE_DIRS}
  ${CORUDEV_INCLUDE_DIRS}
  ${BOOST_INCLUDE_DIR}
  ${STATEFS_UTIL_INCLUDE_DIRS}
)

link_directories(
  ${COR_LIBRARY_DIRS}
  ${CORUDEV_LIBRARY_DIRS}
  ${BOOST_LIBRARY_DIRS}
  ${STATEFS_UTIL_LIBRARY_DIRS}
)

add_library(provider-${SELF} SHARED
  provider_${SELF}.cpp
  )

target_link_libraries(provider-${SELF}
  ${COR_LIBRARIES}
  ${CORUDEV_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${STATEFS_LIBRARIES}
  ${STATEFS_UTIL_LIBRARIES}
  ${Boost_SYSTEM_LIBRARY}
  )

install(TARGETS provider-${SELF} DESTINATION ${DST_LIB}/statefs)
