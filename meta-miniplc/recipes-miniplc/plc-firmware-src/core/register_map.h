#ifndef REGISTER_MAP_H
#define REGISTER_MAP_H

#include <stdint.h>

#define REG_COIL_COUNT 512
#define REG_DISCRETE_INPUT_COUNT 512
#define REG_INPUT_REGISTER_COUNT 512
#define REG_HOLDING_REGISTER_COUNT 512

void reg_map_init(void);

void reg_set_coil(uint16_t addr, uint8_t val);
uint8_t reg_get_coil(uint16_t addr);

void reg_set_discrete_input(uint16_t addr, uint8_t val);
uint8_t reg_get_discrete_input(uint16_t addr);

void reg_set_input_register(uint16_t addr, uint16_t val);
uint16_t reg_get_input_register(uint16_t addr);

void reg_set_holding(uint16_t addr, uint16_t val);
uint16_t reg_get_holding(uint16_t addr);

void reg_map_read_coils_block(uint16_t start, uint16_t count, uint8_t *out);
void reg_map_read_discrete_block(uint16_t start, uint16_t count, uint8_t *out);
void reg_map_read_holding_block(uint16_t start, uint16_t count, uint16_t *out);
void reg_map_read_input_reg_block(uint16_t start, uint16_t count, uint16_t *out);

void reg_map_write_holding_block(uint16_t start, const uint16_t *vals, uint16_t count);
void reg_map_write_coils_block(uint16_t start, const uint8_t *vals, uint16_t count);

#endif /* REGISTER_MAP_H */
