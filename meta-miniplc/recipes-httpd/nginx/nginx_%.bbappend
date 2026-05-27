#
# Meta-raspberrypi — MiniPLC web UI replaces the stock nginx welcome page.
# meta-oe nginx enables sites-enabled/default_server (default_server + /var/www/...).
# mini-plc-web ships conf.d/miniplc.conf with root /usr/share/nginx/html — if both
# define default_server, nginx fails to start; drop the stock symlink only.
#
do_install:append() {
    rm -f ${D}${sysconfdir}/nginx/sites-enabled/default_server
}
