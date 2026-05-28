SUMMARY = "MiniPLC LVGL local HMI (framebuffer)"
PR = "r7"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://CMakeLists.txt \
           file://main.c \
           file://menu.c \
           file://menu.h \
           file://modbus.c \
           file://modbus.h \
           file://libmodbus_bridge.c \
           file://libmodbus_bridge.h \
           file://miniplc-hmi.service \
           file://miniplc-hmi.init \
           file://mdcu-net-apply \
           file://network.conf \
           file://mdcu-network.init \
           "

S = "${WORKDIR}"

inherit cmake systemd pkgconfig update-rc.d

# Our app is fbdev-only (see main.c), but lvgl.h transitively pulls
# xf86drmMode.h (lv_conf.h has LV_USE_LINUX_DRM=1), and that header
# #include <drm.h> — which libdrm ships under .../libdrm/drm.h, not on
# the default search path.  So we still need libdrm at build time.
DEPENDS += "lvgl libmodbus libdrm"
RDEPENDS:${PN} += "libmodbus libdrm init-ifupdown dhcpcd iproute2"

# Same workaround the lvgl recipe uses for its own translation units.
# Must go through OECMAKE_* so Ninja gets the -I on every compile.
OECMAKE_C_FLAGS:append = " -I${STAGING_INCDIR}/libdrm"

SYSTEMD_SERVICE:${PN} = "miniplc-hmi.service"
SYSTEMD_AUTO_ENABLE = "enable"

# Two init scripts in this package; update-rc.d.bbclass only registers one
# directly, so we register the HMI here and add a postinst for the network
# helper below.
INITSCRIPT_PACKAGES = "${PN}"
INITSCRIPT_NAME:${PN} = "miniplc-hmi"
INITSCRIPT_PARAMS:${PN} = "defaults 91 09"

FILES:${PN} += "${systemd_system_unitdir}/miniplc-hmi.service \
                ${sysconfdir}/init.d/miniplc-hmi \
                ${sysconfdir}/init.d/mdcu-network \
                ${bindir}/mdcu-net-apply \
                ${sysconfdir}/mdcu/network.conf \
               "

CONFFILES:${PN} += "${sysconfdir}/mdcu/network.conf"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/miniplc-hmi.service ${D}${systemd_system_unitdir}/miniplc-hmi.service

    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/miniplc-hmi.init    ${D}${sysconfdir}/init.d/miniplc-hmi
    install -m 0755 ${WORKDIR}/mdcu-network.init   ${D}${sysconfdir}/init.d/mdcu-network

    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/mdcu-net-apply ${D}${bindir}/mdcu-net-apply

    install -d ${D}${sysconfdir}/mdcu
    install -m 0644 ${WORKDIR}/network.conf ${D}${sysconfdir}/mdcu/network.conf
}

# Register the mdcu-network init script alongside the HMI one.  Runs early
# (S 05) so the interface is up before everything else, and stops late.
pkg_postinst_ontarget:${PN}:append() {
    if [ -x /etc/init.d/mdcu-network ] ; then
        update-rc.d -f mdcu-network remove >/dev/null 2>&1 || true
        update-rc.d mdcu-network start 05 2 3 4 5 . stop 95 0 1 6 . >/dev/null 2>&1 || true
    fi
}

pkg_prerm:${PN}:append() {
    if [ -x /etc/init.d/mdcu-network ] ; then
        update-rc.d -f mdcu-network remove >/dev/null 2>&1 || true
    fi
}

COMPATIBLE_MACHINE = "^rpi$"
