/**
 * Plugin ABI — shared between firmware and all protocol .so plugins.
 *
 * Enhancement path: incompatible changes require PLC_PLUGIN_API_VERSION bump
 * and coordinated loader + plugin rebuilds. For optional features, prefer
 * new JSON config keys read by newer plugins rather than changing this file.
 */
#ifndef PLC_PLUGIN_H
#define PLC_PLUGIN_H

#include <stdarg.h>
#include <stdint.h>

#define PLC_PLUGIN_API_VERSION 0x0001u

typedef struct PLCRegisterAPI {
    uint16_t (*get_holding)(uint16_t addr);
    void (*set_holding)(uint16_t addr, uint16_t val);
    uint8_t (*get_coil)(uint16_t addr);
    void (*set_coil)(uint16_t addr, uint8_t val);
    uint8_t (*get_discrete_input)(uint16_t addr);
    uint16_t (*get_input_register)(uint16_t addr);
    void (*log)(int level, const char *fmt, ...);
} PLCRegisterAPI;

typedef struct PLCPluginConfig {
    const char *json_config;
    PLCRegisterAPI *reg_api;
} PLCPluginConfig;

typedef struct PLCPlugin {
    uint16_t api_version;
    const char *protocol_id;
    const char *version;
    int (*init)(PLCPluginConfig *cfg);
    int (*start)(void);
    void (*stop)(void);
    void (*destroy)(void);
    int (*reconfigure)(const char *json_config);
    void *reserved[4];
} PLCPlugin;

typedef PLCPlugin *(*plc_plugin_entry_fn)(void);

#endif /* PLC_PLUGIN_H */
