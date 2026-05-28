/**
 * @file modbus.h
 * Modbus TCP Master wrapper API.
 *
 * NOTE: Functions use the `mb_` prefix (not `modbus_`) so they do not collide
 * with the symbols exported by libmodbus (modbus_connect, modbus_read_registers,
 * etc.).  The bridge in libmodbus_bridge.c calls libmodbus by its real names.
 */

#ifndef _MODBUS_H_
#define _MODBUS_H_

#include <stdint.h>
#include <stdbool.h>

/*********************
 *      DEFINES
 *********************/
#define MODBUS_MAX_IP_LEN        16
#define MODBUS_MAX_REGISTERS     100
#define MODBUS_DEFAULT_PORT      502
#define MODBUS_DEFAULT_SLAVE_ID   1

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    char ip_address[MODBUS_MAX_IP_LEN];
    uint16_t port;
    uint8_t slave_id;
    uint16_t start_address;
    uint16_t num_registers;
    bool enabled;
    bool connected;
} modbus_config_t;

typedef struct {
    uint16_t registers[MODBUS_MAX_REGISTERS];
    uint32_t last_update_time;
    uint32_t error_count;
    uint32_t success_count;
} modbus_data_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
int  mb_init(void);
void mb_deinit(void);
int  mb_connect(const modbus_config_t *config);
void mb_disconnect(void);
int  mb_read_registers(uint16_t start_addr, uint16_t num_regs, uint16_t *dest);
int  mb_get_config(modbus_config_t *config);
int  mb_set_config(const modbus_config_t *config);
int  mb_get_data(modbus_data_t *data);
bool mb_is_connected(void);
void mb_save_config(void);
void mb_load_config(void);

#endif /* _MODBUS_H_ */
