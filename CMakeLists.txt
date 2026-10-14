cmake_minimum_required(VERSION 3.16)
project(ble_indication_central C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GATTLIB REQUIRED IMPORTED_TARGET gattlib)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)

add_executable(ble_indication_central src/ble_indication_central.c)
target_link_libraries(ble_indication_central PRIVATE
    PkgConfig::GATTLIB
    PkgConfig::GLIB
    Threads::Threads
)
