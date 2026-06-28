#include "register_map.h"

#include <pthread.h>
#include <string.h>

static uint8_t coils[REG_COIL_COUNT];
static uint8_t discrete_inputs[REG_DISCRETE_INPUT_COUNT];
static uint16_t input_registers[REG_INPUT_REGISTER_COUNT];
static uint16_t holding_registers[REG_HOLDING_REGISTER_COUNT];

static pthread_mutex_t reg_mutex = PTHREAD_MUTEX_INITIALIZER;

static int valid_u8_idx(uint16_t addr, uint16_t max)
{
    return addr < max;
}

void reg_map_init(void)
{
    pthread_mutex_lock(&reg_mutex);
    memset(coils, 0, sizeof(coils));
    memset(discrete_inputs, 0, sizeof(discrete_inputs));
    memset(input_registers, 0, sizeof(input_registers));
    memset(holding_registers, 0, sizeof(holding_registers));
    pthread_mutex_unlock(&reg_mutex);
}

void reg_set_coil(uint16_t addr, uint8_t val)
{
    if (!valid_u8_idx(addr, REG_COIL_COUNT))
        return;
    pthread_mutex_lock(&reg_mutex);
    coils[addr] = val ? 1u : 0u;
    pthread_mutex_unlock(&reg_mutex);
}

uint8_t reg_get_coil(uint16_t addr)
{
    uint8_t v = 0;
    if (!valid_u8_idx(addr, REG_COIL_COUNT))
        return 0;
    pthread_mutex_lock(&reg_mutex);
    v = coils[addr];
    pthread_mutex_unlock(&reg_mutex);
    return v;
}

void reg_set_discrete_input(uint16_t addr, uint8_t val)
{
    if (!valid_u8_idx(addr, REG_DISCRETE_INPUT_COUNT))
        return;
    pthread_mutex_lock(&reg_mutex);
    discrete_inputs[addr] = val ? 1u : 0u;
    pthread_mutex_unlock(&reg_mutex);
}

uint8_t reg_get_discrete_input(uint16_t addr)
{
    uint8_t v = 0;
    if (!valid_u8_idx(addr, REG_DISCRETE_INPUT_COUNT))
        return 0;
    pthread_mutex_lock(&reg_mutex);
    v = discrete_inputs[addr];
    pthread_mutex_unlock(&reg_mutex);
    return v;
}

void reg_set_input_register(uint16_t addr, uint16_t val)
{
    if (!valid_u8_idx(addr, REG_INPUT_REGISTER_COUNT))
        return;
    pthread_mutex_lock(&reg_mutex);
    input_registers[addr] = val;
    pthread_mutex_unlock(&reg_mutex);
}

uint16_t reg_get_input_register(uint16_t addr)
{
    uint16_t v = 0;
    if (!valid_u8_idx(addr, REG_INPUT_REGISTER_COUNT))
        return 0;
    pthread_mutex_lock(&reg_mutex);
    v = input_registers[addr];
    pthread_mutex_unlock(&reg_mutex);
    return v;
}

void reg_set_holding(uint16_t addr, uint16_t val)
{
    if (!valid_u8_idx(addr, REG_HOLDING_REGISTER_COUNT))
        return;
    pthread_mutex_lock(&reg_mutex);
    holding_registers[addr] = val;
    pthread_mutex_unlock(&reg_mutex);
}

uint16_t reg_get_holding(uint16_t addr)
{
    uint16_t v = 0;
    if (!valid_u8_idx(addr, REG_HOLDING_REGISTER_COUNT))
        return 0;
    pthread_mutex_lock(&reg_mutex);
    v = holding_registers[addr];
    pthread_mutex_unlock(&reg_mutex);
    return v;
}

static int valid_range_u16(uint16_t start, uint16_t count, uint16_t max_elts)
{
    if (count == 0 || (uint32_t)start + (uint32_t)count > (uint32_t)max_elts)
        return 0;
    return 1;
}

void reg_map_read_coils_block(uint16_t start, uint16_t count, uint8_t *out)
{
    if (!out || !valid_range_u16(start, count, REG_COIL_COUNT))
        return;
    pthread_mutex_lock(&reg_mutex);
    memcpy(out, coils + start, count);
    pthread_mutex_unlock(&reg_mutex);
}

void reg_map_read_discrete_block(uint16_t start, uint16_t count, uint8_t *out)
{
    if (!out || !valid_range_u16(start, count, REG_DISCRETE_INPUT_COUNT))
        return;
    pthread_mutex_lock(&reg_mutex);
    memcpy(out, discrete_inputs + start, count);
    pthread_mutex_unlock(&reg_mutex);
}

void reg_map_read_holding_block(uint16_t start, uint16_t count, uint16_t *out)
{
    if (!out || !valid_range_u16(start, count, REG_HOLDING_REGISTER_COUNT))
        return;
    pthread_mutex_lock(&reg_mutex);
    memcpy(out, holding_registers + start, count * sizeof(uint16_t));
    pthread_mutex_unlock(&reg_mutex);
}

void reg_map_read_input_reg_block(uint16_t start, uint16_t count, uint16_t *out)
{
    if (!out || !valid_range_u16(start, count, REG_INPUT_REGISTER_COUNT))
        return;
    pthread_mutex_lock(&reg_mutex);
    memcpy(out, input_registers + start, count * sizeof(uint16_t));
    pthread_mutex_unlock(&reg_mutex);
}

void reg_map_write_holding_block(uint16_t start, const uint16_t *vals, uint16_t count)
{
    if (!vals || !valid_range_u16(start, count, REG_HOLDING_REGISTER_COUNT))
        return;
    pthread_mutex_lock(&reg_mutex);
    memcpy(holding_registers + start, vals, count * sizeof(uint16_t));
    pthread_mutex_unlock(&reg_mutex);
}

void reg_map_write_coils_block(uint16_t start, const uint8_t *vals, uint16_t count)
{
    if (!vals || !valid_range_u16(start, count, REG_COIL_COUNT))
        return;
    pthread_mutex_lock(&reg_mutex);
    for (uint16_t i = 0; i < count; i++)
        coils[start + i] = vals[i] ? 1u : 0u;
    pthread_mutex_unlock(&reg_mutex);
}
