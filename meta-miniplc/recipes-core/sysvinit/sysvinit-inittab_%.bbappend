# In sysvinit-inittab_%.bbappend

# Use a colon (:) instead of an underscore (_) to modify the do_install task
do_install:append() {
    # Use sed to comment out the tty1 line in the installed inittab file
    # The pattern matches a standard tty1 respawn line and comments it out
    sed -i -e 's/^\(1:.*tty1\)/#\1/' ${D}${sysconfdir}/inittab
}
