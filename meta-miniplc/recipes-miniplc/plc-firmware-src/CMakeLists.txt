cmake_minimum_required(VERSION 3.16)
project(plc_firmware C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONC REQUIRED json-c)
pkg_check_modules(GPIOD REQUIRED libgpiod)
pkg_check_modules(MDCU_POOL REQUIRED mdcu-pool)

pkg_check_modules(CIVETWEB civetweb)
if(NOT CIVETWEB_FOUND)
    find_library(CIVETWEB_LIBRARIES NAMES civetweb REQUIRED)
    find_path(CIVETWEB_INCLUDE_DIRS NAMES civetweb.h REQUIRED PATH_SUFFIXES civetweb)
endif()

add_executable(plc_firmware
    plc_main.c
    api/rest_api.c
    config/device_config.c
    core/register_map.c
    core/plc_engine.c
    ladder/ladder_vm.c
    io/gpio_gpiod.c
    project/project_store.c
    plugin/plugin_api.c
    plugin/plugin_manager.c
)

target_include_directories(plc_firmware PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CIVETWEB_INCLUDE_DIRS}
    ${JSONC_INCLUDE_DIRS}
    ${GPIOD_INCLUDE_DIRS}
    ${MDCU_POOL_INCLUDE_DIRS}
)

target_compile_options(plc_firmware PRIVATE -Wall -Wextra)

target_link_libraries(plc_firmware PRIVATE
    ${CIVETWEB_LIBRARIES}
    ${JSONC_LIBRARIES}
    ${GPIOD_LIBRARIES}
    ${MDCU_POOL_LIBRARIES}
    Threads::Threads
    m
    dl
)

target_link_directories(plc_firmware PRIVATE
    ${MDCU_POOL_LIBRARY_DIRS}
)

include(GNUInstallDirs)
install(TARGETS plc_firmware RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
