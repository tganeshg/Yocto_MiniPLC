# Build Guide — meta-miniplc

Commands to build the FlexiDon iX components and image from the Yocto tree.

- **Yocto tree root:** `/home/ganesh/Projects/Raspi/Yocto/yocto`
- **Machine:** `raspberrypi` (Pi B+ Rev 1, ARM1176 / ARMv6)
- **Image:** `rpi-test-image` (extended by `meta-miniplc` bbappend)
- **Device:** `root@10.42.0.252` (passwordless SSH)

## 1. Set up the build environment

Run once per shell. Sourcing the init script drops you into `build/`.

```bash
cd /home/ganesh/Projects/Raspi/Yocto/yocto
source poky/oe-init-build-env build
```

## 2. Layer components (recipes)

| Component        | Recipe            | Notes                                  |
|------------------|-------------------|----------------------------------------|
| Register pool    | `libmdcu-pool`    | 50k-word shared pool (`/dev/shm`)      |
| PLC firmware     | `plc-firmware`    | REST API, ladder runtime, GPIO         |
| On-device HMI    | `miniplc-hmi`     | LVGL dashboard (DRM + touch)           |
| Web UI           | `mini-plc-web`    | React/Vite dashboard                   |

### Build a single recipe

```bash
bitbake plc-firmware
bitbake miniplc-hmi
bitbake libmdcu-pool
bitbake mini-plc-web
```

### Build the full image

```bash
bitbake rpi-test-image
```

## 3. Useful per-task builds

```bash
# Compile + link only (stops before packaging)
bitbake -c compile plc-firmware

# Force a clean rebuild of one recipe (drops shared-state cache)
bitbake -c cleansstate plc-firmware
bitbake plc-firmware

# Inspect the resolved config / dependencies
bitbake -e plc-firmware | grep ^DEPENDS
```

Built binaries land in the recipe work tree, e.g.:

```
build/tmp/work/arm1176jzfshf-vfp-poky-linux-gnueabi/plc-firmware/0.1-r2/build/plc_firmware
build/tmp/work/arm1176jzfshf-vfp-poky-linux-gnueabi/miniplc-hmi/1.0-r11/build/miniplc-hmi
```

### Verify shared-library linkage

```bash
source /opt/poky/4.0.28/environment-setup-arm1176jzfshf-vfp-poky-linux-gnueabi
$OBJDUMP -p <path-to-binary> | grep NEEDED
```

## 4. Known issue — `do_install` pseudo abort (stale state)

`do_install` may abort under pseudo/fakeroot with:

```
abort()ing pseudo client by server request
```

and a pseudo log line like:

```
path mismatch [N links]: ino <N> db '.../package/usr/src/debug/<recipe>/.../main.c'
                                  req '.../<recipe>/.../main.c'.
```

**Cause:** stale pseudo state, not a code error or host limitation. A previous
`do_package` left a source file (e.g. `main.c`) hardlinked between `WORKDIR`
and the leftover `package/usr/src/debug/...` debug-source copy. On a re-run of
`do_install`, pseudo sees the same inode at a different path and cannot
reconcile the hardlinks, so it aborts.

**Fix:** clean the recipe's WORKDIR + pseudo DB, then rebuild:

```bash
bitbake -c clean <recipe>
bitbake <recipe>
```

After the clean rebuild, `do_install` → `do_package` → `do_package_write_deb`
complete normally, and a full `bitbake rpi-test-image` works.

To verify code changes without packaging, `bitbake -c compile <recipe>` also
works (compilation and linking happen there). For quick on-device iteration
without a full image flash, deploy the linked ELF directly (below).

## 5. Deploy a single binary to the device (bypasses packaging)

For iterating on one component without a full image flash:

```bash
BIN=build/tmp/work/arm1176jzfshf-vfp-poky-linux-gnueabi/miniplc-hmi/1.0-r11/build/miniplc-hmi

# Stop the service, back up the current binary, push the new one, restart
ssh root@10.42.0.252 '/etc/init.d/miniplc-hmi stop'
ssh root@10.42.0.252 'cp /usr/bin/miniplc-hmi /usr/bin/miniplc-hmi.bak'
scp "$BIN" root@10.42.0.252:/usr/bin/miniplc-hmi
ssh root@10.42.0.252 '/etc/init.d/miniplc-hmi start'
```

Roll back with the `.bak` copy if needed.
