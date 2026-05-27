SUMMARY = "Mini PLC firmware — REST API, ladder runtime, GPIO, project apply (civetweb)"
PR = "r1"
HOMEPAGE = "https://github.com/"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${THISDIR}/../plc_firmware:${THISDIR}/files:"

SRC_URI = " \
    file://CMakeLists.txt \
    file://plc_main.c \
    file://core/paths.h \
    file://core/register_map.c \
    file://core/register_map.h \
    file://core/plc_engine.c \
    file://core/plc_engine.h \
    file://api/rest_api.c \
    file://api/rest_api.h \
    file://config/device_config.c \
    file://config/device_config.h \
    file://io/gpio_gpiod.c \
    file://io/gpio_gpiod.h \
    file://ladder/ladder_vm.c \
    file://ladder/ladder_vm.h \
    file://ladder/opcodes.h \
    file://project/project_store.c \
    file://project/project_store.h \
    file://hmi/hmi_fallback.h \
    file://plugin/plugin_api.c \
    file://plugin/plugin_api.h \
    file://plugin/plugin_manager.c \
    file://plugin/plugin_manager.h \
    file://plugin/plc_plugin.h \
    file://plc-firmware.service \
    file://plc-firmware.init \
    file://miniplc-tmpfiles.conf \
"

S = "${WORKDIR}"

inherit cmake systemd pkgconfig update-rc.d

COMPATIBLE_MACHINE = "^rpi$"

INITSCRIPT_PACKAGES = "${PN}"
INITSCRIPT_NAME:${PN} = "plc-firmware"
INITSCRIPT_PARAMS:${PN} = "defaults 90 10"

DEPENDS += "civetweb json-c libgpiod "
RDEPENDS:${PN} += "civetweb json-c libgpiod unzip "

SYSTEMD_SERVICE:${PN} = "plc-firmware.service"
SYSTEMD_AUTO_ENABLE = "enable"

FILES:${PN} += "${systemd_system_unitdir}/plc-firmware.service ${nonarch_libdir}/tmpfiles.d/miniplc-plc.conf ${sysconfdir}/init.d/plc-firmware"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/plc-firmware.service ${D}${systemd_system_unitdir}/plc-firmware.service

    install -d ${D}${nonarch_libdir}/tmpfiles.d
    install -m 0644 ${WORKDIR}/miniplc-tmpfiles.conf ${D}${nonarch_libdir}/tmpfiles.d/miniplc-plc.conf

    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/plc-firmware.init ${D}${sysconfdir}/init.d/plc-firmware
}
