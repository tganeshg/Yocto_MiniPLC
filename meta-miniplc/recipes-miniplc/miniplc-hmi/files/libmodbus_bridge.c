/**
 * @file libmodbus_bridge.c
 * Isolates the libmodbus include so that modbus.c does not see the
 * conflicting libmodbus symbol declarations.
 */

#include <modbus/modbus.h>
#include "libmodbus_bridge.h"

void *lmb_new_tcp(const char *ip_address, int port)
{
    return (void *)modbus_new_tcp(ip_address, port);
}

int lmb_set_slave(void *ctx, int slave)
{
    return modbus_set_slave((modbus_t *)ctx, slave);
}

void lmb_set_response_timeout(void *ctx, uint32_t to_sec, uint32_t to_usec)
{
    modbus_set_response_timeout((modbus_t *)ctx, to_sec, to_usec);
}

int lmb_connect(void *ctx)
{
    return modbus_connect((modbus_t *)ctx);
}

void lmb_close(void *ctx)
{
    modbus_close((modbus_t *)ctx);
}

void lmb_free(void *ctx)
{
    modbus_free((modbus_t *)ctx);
}

int lmb_read_registers(void *ctx, int addr, int nb, uint16_t *dest)
{
    return modbus_read_registers((modbus_t *)ctx, addr, nb, dest);
}

const char *lmb_strerror(int errnum)
{
    return modbus_strerror(errnum);
}
