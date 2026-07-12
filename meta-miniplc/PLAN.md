# FlexiDon iX — Implementation Plan

> Living plan for **FlexiDon iX** (Flexible Data Collector & Controller) —
> the smart industrial HMI / mini-PLC / Data Concentrator Unit product
> built on Raspberry Pi B+ Rev 1 (ARM1176JZF-S, 512 MB RAM) with a 5"/7"
> capacitive touchscreen.  (`MDCU` / `MiniHMI` / `MiniPLC` are legacy
> codenames still used in recipe/package names.)  This document tracks
> scope and progress; the source of truth for code is this layer
> (`meta-miniplc`).

---

## Components

| Component | Location in this layer | Role |
|-----------|------------------------|------|
| **Register Pool** (`libmdcu-pool`) | `recipes-miniplc/libmdcu-pool/` | Shared 50k×16-bit pool — single source of truth |
| **LVGL HMI** (`miniplc-hmi`) | `recipes-miniplc/miniplc-hmi/files/` | Local touchscreen UI |
| **PLC Firmware** (`plc-firmware`) | `recipes-miniplc/plc-firmware-src/` | REST API + ladder runtime |
| **Web UI** (`mini-plc-web`) | `recipes-miniplc/web/` (source) → `mini-plc-web/` (recipe) | Browser-based dashboard / PLC programmer |

**Protocol role on the RPi:** CLIENT / MASTER for all industrial protocols
(polls field devices).  Priority order: Modbus TCP first, then DLMS/COSEM.

---

## Architecture

```
Browser (port 80)
  └─ nginx
       ├─ /api/*  →  plc-firmware (civetweb :5080)
       │               ├─ Ladder VM
       │               ├─ Register Pool (libmdcu-pool, 50k×16-bit)
       │               │     ├─ GET /api/regs?start=&count=   (bulk read)   ✅
       │               │     └─ GET /api/regmap               (metadata)    ✅
       │               ├─ GPIO (libgpiod)
       │               └─ Protocol Plugins [Phase 3]
       │                     ├─ Modbus TCP client
       │                     └─ DLMS/COSEM client
       └─ /*      →  React/Vite SPA (reads /api/regs live)     ✅ Phase 4 (partial)

Register Pool (libmdcu-pool)  ← shared /dev/shm, linked by HMI + firmware

Touchscreen (LVGL HMI)
  └─ menu.c tabs
       ├─ Overview  (device, CPU/temp/mem gauges, network,
       │             protocol status, Fieldbus RS-232/485/CAN)  ✅
       ├─ PLC       (state, cycle, I/O, program)          ✅
       ├─ Protocols (Modbus TCP — config + live regs)     ✅ Phase 1
       ├─ DLMS      [Phase 2]                             ❌
       ├─ IoT       [stub → flesh out]                    ⚠️
       ├─ Settings  [stub → flesh out]                    ⚠️
       └─ About     [stub → flesh out]                    ⚠️
```

---

## Phase status overview

| # | Phase | Status |
|---|-------|--------|
| 0 | Layer bootstrap + silent boot + HMI source migration into meta-miniplc | ✅ DONE |
| 0.5 | Display fix — switch fbdev → DRM for VC4 KMS 7" DSI panel | ✅ DONE |
| 1 | Modbus TCP backend in LVGL app | ✅ DONE & VERIFIED ON HARDWARE |
| 2 | DLMS/COSEM module in LVGL app | ❌ NOT STARTED |
| 3 | Protocol plugins in plc-firmware (REST API) | ⚠️ PARTIAL — register-pool REST (`/api/regs`, `/api/regmap`) done; protocol clients not started |
| 4 | Web frontend wiring | ⚠️ PARTIAL — React source migrated in-layer, reads `/api/regs` live; protocol-config pages not wired |
| 5 | Yocto integration & build cleanup | partial — gurux-dlms recipe still needed |

**Also landed (cross-cutting):** `libmdcu-pool` shared register pool
recipe; HMI Overview redesign (device/gauges/network/protocol/Fieldbus
cards); FlexiDon iX branding + logo on HMI header and web sidebar.

---

## Phase 0 — Layer bootstrap ✅ DONE

Done in commits `87d686b`, `01b3dd3`, `7b69844`.

- meta-miniplc layer registered (priority 20)
- `rpi-test-image.bbappend` pulls in `packagegroup-miniplc`
- All three product packages (`miniplc-hmi`, `plc-firmware`, `mini-plc-web`) build
- HMI source migrated from `~/Projects/LVGL/Yocto_LVGL_Experiments/` →
  `recipes-miniplc/miniplc-hmi/files/`.  Yocto_LVGL_Experiments is now
  considered abandoned; all application-level changes live in this layer.
- Silent boot restored after the meta-raspberrypi cutover:
    - `rpi-config_git.bbappend` — `disable_splash=1 avoid_warnings=1`,
      `dtoverlay=vc4-kms-dsi-7inch` when VC4
    - `rpi-cmdline.bbappend` — `DISABLE_RPI_BOOT_LOGO=1`,
      `CMDLINE_DEBUG="quiet loglevel=3 vt.global_cursor_default=0 consoleblank=0"`

---

## Phase 0.5 — Display fix ✅ DONE

**Symptom:** 7" DSI panel rendered mostly black; touch worked.

**Root cause:** LVGL fbdev driver on VC4 KMS (`/dev/fb0` provided by
`vc4drmfb`) leaves the panel mostly black even with
`LV_LINUX_FBDEV_RENDER_MODE_DIRECT`.  Our own `lv_conf.h` already flagged
this with a comment recommending the DRM driver.

**Fix:**
- `main.c` — `disp_init()` now tries `lv_linux_drm_create()` on
  `/dev/dri/card0` first; falls back to fbdev only if KMS is missing
- `miniplc-hmi_1.0.bb` — `RDEPENDS:${PN} += "libdrm"`,
  `OECMAKE_C_FLAGS:append = " -I${STAGING_INCDIR}/libdrm"`
- `CMakeLists.txt` — `pkg_check_modules(DRM REQUIRED libdrm)` + link
- `miniplc-hmi.init` — don't bail when only `/dev/dri/card0` is present;
  redirect stdout+stderr to `/var/log/miniplc-hmi.log` via a `sh -c`
  wrapper (busybox `start-stop-daemon` has no `--stdout/--stderr`)

**Verified on hardware** with the 7" panel — clean render at 800×480.

---

## Phase 1 — Modbus TCP backend ✅ DONE & VERIFIED

**Goal:** Make the Protocols-tab Modbus client actually talk to field devices.

**What landed:**
- `files/modbus.c` — all libmodbus calls via `lmb_*` bridge:
  `new_tcp` → `set_slave` → `set_response_timeout(2s)` → `connect` →
  `read_registers` (FC 0x03).  Auto-disconnect on read failure so UI
  retries cleanly.
- `files/libmodbus_bridge.c` — thin shim isolating libmodbus's
  `modbus_*` symbols from our `mb_*` API.  Avoids the name clash that
  would otherwise force us to rename our public functions.
- `files/modbus.h` — public API (`mb_init/connect/read/get_data/...`).
- Config persistence to `/tmp/modbus_config.txt` (6 lines: ip, port,
  slave, start, nregs, enabled).  Load on `mb_init`, save on modal apply.
- `files/menu.c`:
    - `protocols_update()` polls registers on the Protocols tab,
      auto-reconnects when `enabled && !connected`
    - Settings modal save callback writes config + reconnects
    - `menu_loop()` dispatches `protocols_update()` when that tab is active

**End-to-end test** lives in `tests/modbus/`:
- `modbus_sim.py` — pymodbus 3.13 SimData/SimDevice TCP slave, seeds
  R0..R9 with `[1111, 2222, ..., 10000]`
- `run_phase1_test.py` — starts sim on host, writes config on Pi over
  SSH, restarts HMI, scrapes `/var/log/miniplc-hmi.log` for connect +
  matching register values, asserts PASS

```bash
python3 meta-miniplc/tests/modbus/run_phase1_test.py
```

### Phase 1 — known follow-ups (not blockers)

- **Config in `/tmp` is volatile.**  Reboot wipes settings.  Move to
  `/etc/mdcu/modbus.conf` or `/var/lib/miniplc/modbus.conf` when there's
  a natural break — needs a 2-line patch + recipe `CONFFILES` entry.
- **Polling only when Protocols tab is open.**  `menu_loop()` dispatches
  on `current_mmId`, so success/error counters freeze on other tabs.
  Acceptable for an HMI; revisit if we want true background polling.

---

## Phase 2 — DLMS/COSEM ❌ NOT STARTED

**Goal:** Add DLMS client tab so the HMI can read IEC 62056 smart meters.

### New files

| Path | Purpose |
|------|---------|
| `recipes-miniplc/miniplc-hmi/files/dlms.h` | Config struct, data struct, prototypes |
| `recipes-miniplc/miniplc-hmi/files/dlms.c` | Implementation using gurux.dlms.c |
| `recipes-miniplc/miniplc-hmi/files/dlms_gurux_bridge.c/h` | Shim around gurux's C API (same pattern as `libmodbus_bridge.c`) |
| `recipes-protocols/gurux-dlms/gurux-dlms_1.0.bb` | New Yocto recipe for gurux.dlms.c (MIT, IEC 62056) |

### DLMS config struct (mirror modbus.h)

```c
typedef struct {
    char ip_address[16];
    uint16_t port;           // default 4059 (DLMS/TCP)
    uint8_t  client_id;      // DLMS client address
    uint8_t  logical_device; // server logical device
    bool     enabled;
    bool     connected;
} dlms_config_t;
```

### LVGL tab work

- Add `mm_dlms_id` to `MAIN_MENU_ID` enum in `menu.h`
- Add `dlms_create()` in `menu.c` (mirrors `protocols_create()`)
- Tiles: connection status, meter address, last read timestamp, OBIS values
- `dlms_update()` polled from `menu_loop()` the same way as Modbus

### Layer wiring

- `miniplc-hmi_1.0.bb` — add `dlms.c/h` + bridge to `SRC_URI`,
  `DEPENDS += gurux-dlms`, `RDEPENDS:${PN} += gurux-dlms`
- `CMakeLists.txt` — `pkg_check_modules(GURUX REQUIRED gurux-dlms)` +
  link `${GURUX_LIBRARIES}`
- `packagegroup-miniplc.bb` — `RDEPENDS` `gurux-dlms`

### Verification

- New `tests/dlms/` folder with a Gurux Director-compatible server stub
  (or a recorded HDLC trace replay), `run_phase2_test.py` mirroring the
  Modbus test orchestration.

---

## Phase 3 — Protocol plugins in plc-firmware (REST API) ⚠️ PARTIAL

**Goal:** Wire Modbus TCP and DLMS into `plc-firmware` so the web UI can
configure and monitor them via REST.

**Done so far:** The shared register pool (`libmdcu-pool`) is exposed over
REST — `GET /api/regs?start=&count=` (bulk read) and `GET /api/regmap`
(pool metadata), implemented in `plc-firmware-src/api/rest_api.c`
(commit `540fe40`). `plc-firmware` links `libmdcu_pool.so`. The protocol
polling clients below are still not started.

### Files to change / add

| Path | Change |
|------|--------|
| `plc-firmware-src/api/rest_api.c` | Add routes (see below) |
| `plc-firmware-src/plugin/plugin_api.c` | Implement `GET /api/plugins` to list registered protocols |
| `plc-firmware-src/protocols/modbus_client.c` (new) | libmodbus polling loop, reuses Phase 1 patterns |
| `plc-firmware-src/protocols/dlms_client.c` (new) | gurux.dlms polling loop |

### REST routes

| Method | Path | Body / Action |
|--------|------|---------------|
| `GET`  | `/api/protocols` | List instances + status |
| `POST` | `/api/protocols/modbus` | Save config → `/etc/mdcu/modbus.json` |
| `GET`  | `/api/protocols/modbus/data` | Latest register values |
| `POST` | `/api/protocols/dlms` | Save DLMS config |
| `GET`  | `/api/protocols/dlms/data` | Latest OBIS values |

### Recipe

- `plc-firmware_0.1.bb` — add `protocols/modbus_client.c` and
  `protocols/dlms_client.c` to `SRC_URI` / CMake `add_executable()`

### Verification

- Extend `tests/modbus/` with a `run_phase3_test.py` that POSTs config
  and GETs `/api/protocols/modbus/data`, verifying the same SEED values.

---

## Phase 4 — Web frontend wiring ⚠️ PARTIAL

**Goal:** Connect existing React protocol-config pages to the new REST endpoints.

**Done so far:** The React/Vite source is now migrated in-layer at
`recipes-miniplc/web/` (no longer an opaque tarball only — the recipe
`mini-plc-web` still packages a pre-built `web-dist.tar.gz` regenerated
from it). The dashboard reads the register pool live via `/api/regs`
(`src/api/regs.js`, `src/hooks/useSystemStatus.js`, `src/domain/sysmap.js`).
FlexiDon iX branding + logo added to the sidebar. Still to do: wire the
protocol-config pages (Modbus/DLMS) to their REST endpoints.

### Files to change (under `recipes-miniplc/web/src/`)

| File | Change |
|------|--------|
| `api/plc_api.js` | Add `getProtocols`, `setModbusConfig`, `getModbusData`, `setDlmsConfig`, `getDlmsData` |
| `pages/ProtocolPage.jsx` | Load live data from `/api/protocols` |
| `protocol/ModbusTCPConfig.jsx` | Wire Save button → `setModbusConfig` |
| `protocol/DLMSConfig.jsx` | Wire Save button → `setDlmsConfig` |
| `monitor/RegisterTable.jsx` | Poll `/api/protocols/modbus/data` |

### Build

```bash
cd recipes-miniplc/web
npm ci && npm run build
tar -czf web-dist.tar.gz -C dist .
# replaces recipes-miniplc/mini-plc-web/files/web-dist.tar.gz
```

---

## Phase 5 — Yocto integration & build cleanup

| File | Change | Status |
|------|--------|--------|
| `recipes-protocols/gurux-dlms/gurux-dlms_1.0.bb` | **NEW** — build gurux.dlms.c | ❌ |
| `recipes-core/packagegroups/packagegroup-miniplc.bb` | Add `gurux-dlms` to RDEPENDS | ❌ |
| `recipes-graphics/lvgl/lvgl_9.4.0.bb` | Verify `lv_conf.h` for both 5" and 7" panels | ⚠️ partial |
| `recipes-miniplc/miniplc-hmi/miniplc-hmi_1.0.bb` | Add `dlms.c/h` to SRC_URI | ❌ |
| `recipes-miniplc/plc-firmware/plc-firmware_0.1.bb` | Add protocol clients to CMake | ❌ |

---

## Hardware test harness

Lives in `tests/` at the layer root.  Assumes:

- Pi reachable at `10.42.0.252` (NetworkManager shared link, host = `10.42.0.1`)
- Passwordless SSH to `root@<pi>` (default for `rpi-test-image`)
- Serial console at `/dev/ttyUSB0` @ 115200

Overridable per-script via `PI_IP`, `HOST_IP`, `SIM_PORT`, `PI_SERIAL`
environment variables.

| Folder | Tests | Status |
|--------|-------|--------|
| `tests/modbus/` | Phase 1 (modbus_sim + run_phase1_test) | ✅ |
| `tests/dlms/` | Phase 2 (planned) | — |

---

## Critical-files index (post-Phase-1)

### Register pool — `recipes-miniplc/libmdcu-pool/files/`
- `mdcu_pool.c/h` — 50k×16-bit shared-memory pool + typed accessors
- `mdcu_regmap.h` — register-map layout (shared with firmware + HMI)
- `libmdcu-pool_0.1.bb` — Makefile build → `libmdcu_pool.so` + pkg-config

### Application — `recipes-miniplc/miniplc-hmi/files/`
- `main.c` — DRM-first display init, evdev touch attach
- `menu.c/h` — tabbed UI, modbus integration, Overview redesign + Fieldbus
- `flexidon_logo.c` — compiled-in ARGB8888 brand logo (LVGL has no PNG decoder)
- `modbus.c/h` — Modbus TCP master (Phase 1 done)
- `libmodbus_bridge.c/h` — libmodbus name-clash shim
- `dlms.c/h` — **Phase 2**
- `CMakeLists.txt` — wires lvgl + libmodbus + libdrm + libmdcu-pool; needs gurux later

### PLC firmware — `recipes-miniplc/plc-firmware-src/`
- `api/rest_api.c` — `/api/regs` + `/api/regmap` done; **Phase 3** protocol routes TODO
- `plugin/plugin_api.c` — **Phase 3** plugin listing
- `protocols/modbus_client.c` — **Phase 3 NEW**
- `protocols/dlms_client.c` — **Phase 3 NEW**

### Web — `recipes-miniplc/web/` (source) + `mini-plc-web/` (recipe)
- React/Vite source now lives in-layer at `recipes-miniplc/web/src/`.
- `src/api/regs.js` / `src/hooks/useSystemStatus.js` — live `/api/regs` polling.
- `mini-plc-web` recipe packages a pre-built `web-dist.tar.gz` built from
  that source.  See **Phase 4**.

### Yocto
- `recipes-protocols/gurux-dlms/gurux-dlms_1.0.bb` — **Phase 5 NEW**
- `recipes-core/packagegroups/packagegroup-miniplc.bb` — add gurux-dlms
