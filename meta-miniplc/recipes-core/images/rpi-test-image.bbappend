# Add MiniHMI product payload + RTL8192CU WiFi dongle firmware to the
# stock rpi-test-image. The packagegroup pulls in plc-firmware,
# miniplc-hmi, mini-plc-web and their runtime deps.

IMAGE_INSTALL:append = " packagegroup-miniplc"
IMAGE_INSTALL:append = " linux-firmware-rtl8192cu"
