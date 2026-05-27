# Trim the stock packagegroup-rpi-test of things we don't ship, and add
# the runtime utilities the MiniHMI product expects on target.
#
# Removed:
#   - raspi-gpio / rpio / rpi-gpio / pi-blaster — we use libgpiod
#   - python3-adafruit-* / python3-rtimu       — no Python sensor stack
#   - connman / connman-client                 — replaced by dhcpcd + ifupdown
#   - bluez5                                   — no Bluetooth requirement
#
# Added:
#   - lvgl, evtest                — HMI runtime + input debugging
#   - openssh + sftp + server     — remote shell / file deploy
#   - mosquitto-clients           — MQTT publisher test tool
#   - curl, bash                  — operator convenience
#   - libmodbus                   — required by miniplc-hmi
#   - libsqlite3                  — used by plc-firmware

RDEPENDS:${PN}:remove = " \
    raspi-gpio \
    rpio \
    rpi-gpio \
    pi-blaster \
    python3-adafruit-circuitpython-register \
    python3-adafruit-platformdetect \
    python3-adafruit-pureio \
    python3-rtimu \
    connman \
    connman-client \
    bluez5 \
"

RDEPENDS:${PN}:append = " \
    mosquitto-clients \
    lvgl \
    evtest \
    openssh \
    openssh-sftp \
    openssh-sftp-server \
    curl \
    libmodbus \
    libsqlite3 \
    bash \
"
