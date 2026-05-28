# MiniHMI kernel cmdline overrides — keep the boot console silent so the
# LVGL HMI gets a clean framebuffer with no kernel logo, no log spam, and
# no blinking text cursor before the UI takes over.
#
# Upstream rpi-cmdline.bb (meta-raspberrypi) already supports
# DISABLE_RPI_BOOT_LOGO; we also stuff the extra silencing knobs into
# CMDLINE_DEBUG (which is an empty placeholder upstream).

DISABLE_RPI_BOOT_LOGO = "1"

# quiet            - drop kernel printk to console below KERN_WARNING
# loglevel=3       - belt-and-braces: only KERN_ERR and worse on console
# vt.global_cursor_default=0 - hide the text-mode cursor on tty
# consoleblank=0   - never blank the console (HMI manages the display)
CMDLINE_DEBUG = "quiet loglevel=3 vt.global_cursor_default=0 consoleblank=0"
