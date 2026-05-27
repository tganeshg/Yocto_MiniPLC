SUMMARY = "Mini PLC browser configurator (static Vite build installed under nginx)"
PR = "r1"
DESCRIPTION = "Ships pre-built web assets. Regenerate files/web-dist.tar.gz from recipes-miniplc/web (npm ci && npm run build, then tar dist/)."
HOMEPAGE = "https://github.com/"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# Tarball is unpacked by BitBake into ${WORKDIR}/${WEBDIST_SUBDIR}/ (see base.bbclass).
WEBDIST_SUBDIR = "web-dist-root"
SRC_URI = "file://web-dist.tar.gz;subdir=${WEBDIST_SUBDIR} \
           file://miniplc.conf \
           "

S = "${WORKDIR}"

inherit allarch

# nginx is provided by meta-openembedded meta-webserver (add to bblayers.conf)
RDEPENDS:${PN} += "nginx"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${datadir}/nginx/html
    # Do not use cp -a: preserving host UID/GID breaks do_package/sstate (OEOuthashBasic).
    cp -r ${WORKDIR}/${WEBDIST_SUBDIR}/. ${D}${datadir}/nginx/html/
    chown -R root:root ${D}${datadir}/nginx/html

    install -d ${D}${sysconfdir}/nginx/conf.d
    install -m 0644 ${WORKDIR}/miniplc.conf ${D}${sysconfdir}/nginx/conf.d/miniplc.conf
}

FILES:${PN} += "${datadir}/nginx/html ${sysconfdir}/nginx/conf.d/miniplc.conf"
