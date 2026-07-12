# meta-miniplc

Product layer for **FlexiDon iX** — a Flexible Data Collector &
Controller (industrial HMI + mini-PLC + Data Concentrator Unit) running
on Raspberry Pi (B+ today, larger boards later). `MDCU` / `MiniHMI` /
`Mini PLC` are legacy codenames still visible in some recipe and package
names. All customizations needed to turn a generic Yocto / Raspberry Pi
image into the FlexiDon iX product live here.

Designed to sit on top of upstream `meta-raspberrypi` **without
modifying it**. That keeps the BSP tree clean for future kernel/firmware
bumps and lets us reuse this layer on any future board variant just by
flipping `MACHINE`.

---

## What's in here

```
meta-miniplc/
├── conf/
│   └── layer.conf                              Layer registration
├── COPYING.MIT                                 License (MIT)
│
├── recipes-bsp/
│   └── bootfiles/
│       └── rpi-config_git.bbappend             Append DSI overlay to config.txt
│
├── recipes-core/
│   ├── images/
│   │   └── rpi-test-image.bbappend             Add packagegroup-miniplc + RTL8192CU fw
│   ├── init-ifupdown/
│   │   ├── init-ifupdown_%.bbappend            Drop our /etc/network/interfaces
│   │   └── files/interfaces
│   ├── initscripts/
│   │   └── initscripts_%.bbappend              SysV init tweaks
│   ├── packagegroups/
│   │   └── packagegroup-rpi-test.bbappend      Trim Adafruit/ConnMan, add HMI deps
│   └── sysvinit/
│       └── sysvinit-inittab_%.bbappend         Console on /dev/tty1
│
├── recipes-graphics/
│   └── lvgl/
│       ├── lvgl_9.4.0.bb                       Replaces meta-oe's 8.1.0
│       └── lvgl/lv_conf.h                      Configured for 800×480 5" panel
│
├── recipes-httpd/
│   └── nginx/
│       └── nginx_%.bbappend                    Reverse proxy /api → civetweb
│
├── recipes-miniplc/
│   ├── libmdcu-pool/                           Shared 50k×16-bit register pool recipe
│   ├── miniplc-hmi/                            LVGL touchscreen UI recipe
│   ├── plc-firmware/                           REST API + ladder runtime recipe
│   ├── plc-firmware-src/                       plc-firmware C/C++ source tree
│   ├── web/                                    React/Vite web UI source (in-layer)
│   ├── mini-plc-web/                           Web packaging recipe (Vite build → nginx)
│   └── packagegroups/
│       └── packagegroup-miniplc.bb             Meta-package: pulls the product in
│
└── scripts/
    └── miniplc-smoke-test.sh                   On-target sanity check
```

### Four product recipes

| Recipe          | What it builds                                            |
|-----------------|-----------------------------------------------------------|
| `libmdcu-pool`  | `libmdcu_pool.so` — shared 50k×16-bit register pool        |
| `plc-firmware`  | C/C++ REST API + ladder-logic VM (civetweb on :5080)      |
| `miniplc-hmi`   | LVGL 9.4.0 touchscreen app (`/usr/bin/miniplc-hmi`)        |
| `mini-plc-web`  | React/Vite SPA served by nginx at `/`                     |

`libmdcu-pool` is the shared register pool both `plc-firmware` and
`miniplc-hmi` link against as the single source of truth. The React
source for `mini-plc-web` now lives in-layer at `recipes-miniplc/web/`;
the recipe ships a pre-built `web-dist.tar.gz` regenerated from it
(`npm ci && npm run build`, then tar `dist/`).

The `packagegroup-miniplc` ties them together so `IMAGE_INSTALL += "packagegroup-miniplc"` is enough to pull the whole product in.

---

## Layer config

```
LAYERDEPENDS_miniplc = "core raspberrypi openembedded-layer networking-layer webserver"
LAYERSERIES_COMPAT_miniplc = "kirkstone"
BBFILE_PRIORITY_miniplc = "20"
```

Priority **20** is above meta-oe (6) and meta-raspberrypi (9), so our
`.bbappend` files (and the higher-versioned `lvgl_9.4.0.bb` overriding
meta-oe's `lvgl_8.1.0.bb`) win cleanly.

---

## Using it

### From a fresh setup

If you used `bin/setup_yocto.sh` in the parent `yocto/` directory, it
already adds this layer to `bblayers.conf`. Just:

```bash
source poky/oe-init-build-env build
bitbake rpi-test-image
```

### From a hand-rolled setup

Add to your `bblayers.conf`:

```
BBLAYERS += " /path/to/yocto/meta-miniplc "
```

---

## Targeting a different Raspberry Pi

Nothing in this layer is hard-pinned to the RPi B+ — set `MACHINE` in
`local.conf` to any value `meta-raspberrypi` understands:

```
MACHINE = "raspberrypi4-64"
```

Two caveats:

- `lv_conf.h` is sized for an **800×480** 5" panel. For a different
  display, edit `recipes-graphics/lvgl/lvgl/lv_conf.h` (the screen
  resolution lives there, search for `LV_HOR_RES_MAX`).
- The DSI overlay added by `rpi-config_git.bbappend` assumes the
  official 7-inch DSI panel. If you're driving HDMI instead, set
  `VC4GRAPHICS = "0"` in `local.conf` to skip the overlay.

---

## History — why this layer exists

The product originally lived inside a private fork of
`meta-raspberrypi/` (recipes-miniplc, lv_conf.h, bbappends, the works).
That coupled product code to BSP code and meant every upstream kernel
bump turned into a merge exercise.

The layer was extracted (and `meta-raspberrypi/` reset to pristine
upstream) so that:

1. `meta-raspberrypi/` is read-only and pinned by `setup_yocto.sh` —
   bumping it for a security fix or new firmware is a one-line edit.
2. Product evolution happens here without touching BSP code.
3. Future boards (RPi 4, CM4, even non-Pi if it comes to that) only
   need a different BSP — this layer rides along unchanged.

The cutover is **done** — there is no longer any product code in
`meta-raspberrypi/`. If anything product-related ever shows up there
again, it's a bug; move it back here.

---

## Adding a new recipe to this layer

1. Put it under `recipes-<category>/<recipe-name>/<recipe-name>_<ver>.bb`
2. Add it to `packagegroup-miniplc.bb` under `RDEPENDS:${PN}` if it
   should ship by default.
3. Rebuild: `bitbake rpi-test-image`.

For new bbappends on upstream recipes, mirror the directory structure of
the layer the recipe lives in (e.g. an `nginx` bbappend goes in
`recipes-httpd/nginx/nginx_%.bbappend`).

---

## License

Recipes and helper scripts in this layer are MIT (see `COPYING.MIT`).
Each recipe pins its own upstream `LICENSE` for the software it builds.
