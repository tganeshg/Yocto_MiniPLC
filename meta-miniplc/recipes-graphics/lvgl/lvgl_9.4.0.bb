SUMMARY = "Light and Versatile Graphics Library"
PR = "r6"
DESCRIPTION = "LVGL is a free and open-source graphics library for embedded GUI systems."
HOMEPAGE = "https://lvgl.io"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENCE.txt;md5=4570b6241b4fced1d1d18eb691a0e083"

# Use exact revision for v9.4.0 tag
SRC_URI = "git://github.com/lvgl/lvgl.git;protocol=https;branch=release/v9.4"
SRCREV = "c016f72d4c125098287be5e83c0f1abed4706ee5"

# Custom configuration file (you must keep lv_conf.h beside this recipe)
SRC_URI += "file://lv_conf.h"

S = "${WORKDIR}/git"

inherit cmake

# Enable simple include for lv_conf.h
EXTRA_OECMAKE += "-DLV_CONF_INCLUDE_SIMPLE=ON"

# Dependencies (e.g., framebuffer, DRM)
DEPENDS += "libdrm"

# xf86drmMode.h does #include <drm.h>; libdrm installs .../libdrm/drm.h.
# Must go through OECMAKE_* (cmake.bbclass), not only CFLAGS, so Ninja gets -I on every file.
OECMAKE_C_FLAGS:append = " -I${STAGING_INCDIR}/libdrm"
OECMAKE_CXX_FLAGS:append = " -I${STAGING_INCDIR}/libdrm"

do_configure:prepend() {
    if [ ! -f ${WORKDIR}/lv_conf.h ]; then
        bbfatal "Missing lv_conf.h. Please provide it next to the recipe."
    fi
    cp ${WORKDIR}/lv_conf.h ${S}/lv_conf.h
}

# Manually install lv_conf.h (LVGL doesn’t install it by default)
do_install:append() {
    # Create include destination
    install -d ${D}${includedir}/lvgl

    # Copy the entire LVGL source tree (includes all header files)
    cp -r ${S}/src ${D}${includedir}/lvgl/

    # Copy the main LVGL public header
    install -m 0644 ${S}/lvgl.h ${D}${includedir}/lvgl/

    # Install your configuration header in both places for compatibility
    install -m 0644 ${WORKDIR}/lv_conf.h ${D}${includedir}/
    install -m 0644 ${WORKDIR}/lv_conf.h ${D}${includedir}/lvgl/
}


# Packaging
PACKAGES = "${PN} ${PN}-dev"

FILES:${PN} = "${libdir}/*.a"
FILES:${PN}-dev = "${includedir} ${datadir}/pkgconfig"

INSANE_SKIP:${PN} += "staticdev"

