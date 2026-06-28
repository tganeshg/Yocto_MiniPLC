#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdint.h>

#define MINIPLC_GPIO_CHANNELS 4

typedef struct DeviceConfig {
    char gpio_chip[128];
    int di_line[MINIPLC_GPIO_CHANNELS];
    int do_line[MINIPLC_GPIO_CHANNELS];
    uint32_t scan_ms;
} DeviceConfig;

void device_config_init_defaults(DeviceConfig *cfg);
/** Load /etc/plc/config.json; on missing/parse error, keep defaults. */
void device_config_load_file(const char *path, DeviceConfig *cfg);
int device_config_save_file(const char *path, const DeviceConfig *cfg);

#endif
