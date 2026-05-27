cmake_minimum_required(VERSION 3.16)
project(miniplc-hmi C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LVGL REQUIRED lvgl)
pkg_check_modules(JSONC REQUIRED json-c)
pkg_check_modules(DRM REQUIRED libdrm)

add_executable(miniplc-hmi main.c)

target_include_directories(miniplc-hmi PRIVATE
    ${LVGL_INCLUDE_DIRS}
    ${JSONC_INCLUDE_DIRS}
    ${DRM_INCLUDE_DIRS}
)

target_compile_options(miniplc-hmi PRIVATE -Wall ${LVGL_CFLAGS_OTHER})

target_link_libraries(miniplc-hmi PRIVATE
    ${LVGL_LIBRARIES}
    ${JSONC_LIBRARIES}
    ${DRM_LIBRARIES}
    m
    pthread
)

include(GNUInstallDirs)
install(TARGETS miniplc-hmi RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
