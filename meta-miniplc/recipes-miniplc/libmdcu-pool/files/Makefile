# libmdcu-pool — unified 50,000-register process image for the MDCU
#
# Builds:   libmdcu_pool.so.<MAJ>.<MIN>.<PATCH>
# Installs: headers + .so + symlinks + pkg-config

CC      ?= gcc
AR      ?= ar
PREFIX  ?= /usr
LIBDIR  ?= $(PREFIX)/lib
INCDIR  ?= $(PREFIX)/include
PKGDIR  ?= $(LIBDIR)/pkgconfig

# Caller (Yocto) may override CFLAGS/LDFLAGS; keep defaults sane for hand-build.
CFLAGS  ?= -O2 -g -Wall -Wextra -fPIC -std=gnu11
LDFLAGS ?=

SOVER_MAJ = 0
SOVER     = 0.1.0
SONAME    = libmdcu_pool.so.$(SOVER_MAJ)
SOFILE    = libmdcu_pool.so.$(SOVER)
LINK      = libmdcu_pool.so

HDRS    = mdcu_pool.h mdcu_regmap.h
SRCS    = mdcu_pool.c
PC_IN   = mdcu-pool.pc.in
PC      = mdcu-pool.pc

.PHONY: all install clean

all: $(SOFILE) $(PC)

$(SOFILE): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -fPIC -shared -Wl,-soname,$(SONAME) -o $@ $(SRCS) $(LDFLAGS) -lrt

$(PC): $(PC_IN)
	sed -e 's|@PREFIX@|$(PREFIX)|g'   \
	    -e 's|@LIBDIR@|$(LIBDIR)|g'   \
	    -e 's|@INCDIR@|$(INCDIR)|g'   \
	    -e 's|@VERSION@|$(SOVER)|g'   $(PC_IN) > $(PC)

install: all
	install -d $(DESTDIR)$(LIBDIR)
	install -m 0755 $(SOFILE) $(DESTDIR)$(LIBDIR)/$(SOFILE)
	ln -sf $(SOFILE) $(DESTDIR)$(LIBDIR)/$(SONAME)
	ln -sf $(SOFILE) $(DESTDIR)$(LIBDIR)/$(LINK)
	install -d $(DESTDIR)$(INCDIR)
	install -m 0644 $(HDRS) $(DESTDIR)$(INCDIR)/
	install -d $(DESTDIR)$(PKGDIR)
	install -m 0644 $(PC) $(DESTDIR)$(PKGDIR)/

clean:
	rm -f $(SOFILE) $(PC)
