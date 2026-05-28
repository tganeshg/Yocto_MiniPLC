# meta-miniplc / tests

Out-of-band integration tests for the MiniPLC HMI.  These run on the **dev
host**, drive the Pi over SSH (LAN) and the serial console (USB-UART on
`/dev/ttyUSB0`), and exercise the on-device binaries against synthetic
peers that the host runs locally.

| Folder | What it tests | How |
|--------|---------------|-----|
| `modbus/` | Phase 1 — Modbus TCP master in `miniplc-hmi` | Spins a pymodbus TCP slave on the host, writes `/tmp/modbus_config.txt` on the Pi pointing at the host, restarts the HMI, scrapes `/var/log/miniplc-hmi.log` for connect + read success. |

## Conventions

- Tests assume the Pi is reachable at `10.42.0.252` (NetworkManager shared
  link, host = `10.42.0.1`).  Override with the env var `PI_IP`.
- SSH is passwordless `root` via the key Yocto's `rpi-test-image` accepts
  by default.
- Serial console at `/dev/ttyUSB0` @ 115200 (override with `PI_SERIAL`).
- Test scripts are self-contained — pure Python 3 + the host packages
  named in each script's docstring.
