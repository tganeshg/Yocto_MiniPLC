# Append the 7-inch DSI overlay to the boot config.txt produced by
# meta-raspberrypi's rpi-config recipe.
#
# The stock rpi-config_git.bb writes ${DEPLOYDIR}/${BOOTFILES_DIR_NAME}/config.txt
# during do_deploy. We tack on the dtoverlay line only when VC4 KMS is
# active (i.e. the panel is actually wired up via the DSI display
# pipeline). If you swap to an HDMI-only build, set VC4GRAPHICS = "0"
# and this no-ops.

do_deploy:append() {
    CONFIG="${DEPLOYDIR}/${BOOTFILES_DIR_NAME}/config.txt"
    if [ -f "${CONFIG}" ]; then
        # Silent boot: no rainbow splash, no firmware warnings on screen.
        # HMI takes over the framebuffer — we don't want anything else
        # drawing on it.
        echo "" >> ${CONFIG}
        echo "# MiniHMI silent boot (meta-miniplc)" >> ${CONFIG}
        echo "disable_splash=1" >> ${CONFIG}
        echo "avoid_warnings=1" >> ${CONFIG}
    fi
    if [ "${VC4GRAPHICS}" = "1" ] && [ -f "${CONFIG}" ]; then
        echo "" >> ${CONFIG}
        echo "# MiniHMI 7-inch DSI panel overlay (meta-miniplc)" >> ${CONFIG}
        echo "dtoverlay=vc4-kms-dsi-7inch" >> ${CONFIG}
    fi
}
