cmake_minimum_required(VERSION 3.24.0)

project(dbus_examples)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSYSTEMD REQUIRED libsystemd)

add_executable (dbus_server main_server.cpp)
add_executable (dbus_client main_client.cpp)
add_executable (dbus_publisher main_publisher.cpp)
add_executable (dbus_listener main_listener.cpp)

target_link_libraries(dbus_server ${LIBSYSTEMD_LIBRARIES})
target_link_libraries(dbus_client ${LIBSYSTEMD_LIBRARIES})
target_link_libraries(dbus_publisher ${LIBSYSTEMD_LIBRARIES})
target_link_libraries(dbus_listener ${LIBSYSTEMD_LIBRARIES})
