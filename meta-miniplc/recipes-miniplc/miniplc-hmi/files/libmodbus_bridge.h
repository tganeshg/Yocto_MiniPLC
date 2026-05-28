/**
 * @file libmodbus_bridge.h
 * Thin shim so modbus.c can call libmodbus without name conflicts.
 * (libmodbus exports modbus_connect/modbus_read_registers with different
 *  signatures than our own wrapper API.)
 */

#ifndef LIBMODBUS_BRIDGE_H
#define LIBMODBUS_BRIDGE_H

#include <stdint.h>

void *      lmb_new_tcp(const char *ip_address, int port);
int         lmb_set_slave(void *ctx, int slave);
void        lmb_set_response_timeout(void *ctx, uint32_t to_sec, uint32_t to_usec);
int         lmb_connect(void *ctx);
void        lmb_close(void *ctx);
void        lmb_free(void *ctx);
int         lmb_read_registers(void *ctx, int addr, int nb, uint16_t *dest);
const char *lmb_strerror(int errnum);

#endif /* LIBMODBUS_BRIDGE_H */
