cmake_minimum_required(VERSION 3.24.0)

project(dbus_examples)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSYSTEMD REQUIRED libsystemd)

add_subdirectory(basic_examples)
add_subdirectory(systemd_examples)
