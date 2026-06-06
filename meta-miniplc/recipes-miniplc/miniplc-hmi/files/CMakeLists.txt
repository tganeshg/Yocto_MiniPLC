cmake_minimum_required(VERSION 3.16)
project(miniplc-hmi C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LVGL REQUIRED lvgl)

# libmodbus ships a pkg-config file (libmodbus.pc).
pkg_check_modules(MODBUS REQUIRED libmodbus)

# libdrm — needed because main.c calls lv_linux_drm_*().  lvgl.h transitively
# pulls xf86drmMode.h (which includes <drm.h>) via lv_conf.h LV_USE_LINUX_DRM=1
# and the DRM driver actually uses libdrm symbols at link time.
pkg_check_modules(DRM REQUIRED libdrm)

# Unified register pool — single source of truth shared with plc-firmware,
# protocol pollers, REST API, and the future ladder VM.
pkg_check_modules(MDCU_POOL REQUIRED mdcu-pool)

add_executable(miniplc-hmi
    main.c
    menu.c
    modbus.c
    libmodbus_bridge.c
)

target_include_directories(miniplc-hmi PRIVATE
    ${LVGL_INCLUDE_DIRS}
    ${MODBUS_INCLUDE_DIRS}
    ${DRM_INCLUDE_DIRS}
    ${MDCU_POOL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(miniplc-hmi PRIVATE
    -Wall
    ${LVGL_CFLAGS_OTHER}
)

# LVGL ships several static libs in this build (liblvgl, liblvgl_demos,
# liblvgl_examples, liblvgl_thorvg).  pkg-config only lists liblvgl, so add
# the rest by hand — main.c includes lv_demos.h which has weak refs and at
# the Makefile level we linked all four to keep the symbol table closed.
target_link_libraries(miniplc-hmi PRIVATE
    lvgl_demos
    lvgl_examples
    lvgl_thorvg
    ${LVGL_LIBRARIES}
    ${MODBUS_LIBRARIES}
    ${DRM_LIBRARIES}
    ${MDCU_POOL_LIBRARIES}
    m
    pthread
    rt
)

target_link_directories(miniplc-hmi PRIVATE
    ${LVGL_LIBRARY_DIRS}
    ${MODBUS_LIBRARY_DIRS}
    ${DRM_LIBRARY_DIRS}
    ${MDCU_POOL_LIBRARY_DIRS}
)

include(GNUInstallDirs)
install(TARGETS miniplc-hmi RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
