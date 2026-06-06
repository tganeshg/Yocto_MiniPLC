SUMMARY  = "MDCU unified register pool (libmdcu_pool)"
DESCRIPTION = "Shared-memory backed 50,000 × 16-bit register pool with typed \
accessors (bit, u8/i8, u16/i16, u32/i32, u64/i64, f32, f64). Used by \
miniplc-hmi, plc-firmware, and protocol pollers as the single source of truth."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PR = "r1"

SRC_URI = "file://Makefile \
           file://mdcu_pool.c \
           file://mdcu_pool.h \
           file://mdcu_regmap.h \
           file://mdcu-pool.pc.in \
           "

S = "${WORKDIR}"

# Pure Makefile build — no autotools, no cmake.
EXTRA_OEMAKE = "PREFIX=/usr LIBDIR=${libdir} INCDIR=${includedir} \
                PKGDIR=${libdir}/pkgconfig CC='${CC}' \
                CFLAGS='${CFLAGS} -fPIC -Wall -Wextra -std=gnu11' \
                LDFLAGS='${LDFLAGS}'"

do_compile() {
    oe_runmake all
}

do_install() {
    oe_runmake DESTDIR="${D}" install
}

PACKAGES = "${PN}-dbg ${PN} ${PN}-dev ${PN}-staticdev"

# Runtime: shared object + soname symlink
FILES:${PN}     = "${libdir}/libmdcu_pool.so.*"
# Dev: headers, link-time symlink, pkg-config
FILES:${PN}-dev = "${includedir}/mdcu_pool.h \
                   ${includedir}/mdcu_regmap.h \
                   ${libdir}/libmdcu_pool.so \
                   ${libdir}/pkgconfig/mdcu-pool.pc"

COMPATIBLE_MACHINE = "^rpi$"
