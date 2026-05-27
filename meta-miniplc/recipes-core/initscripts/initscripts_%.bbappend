do_install:append() {
    # This appends the command to the end of the banner script
    echo "echo 0 > /sys/class/graphics/fbcon/cursor_blink" >> ${D}${sysconfdir}/init.d/banner.sh
}
