#include "ladder_vm.h"
#include "opcodes.h"

#include "../core/register_map.h"

#include <string.h>

static uint8_t g_instr[LADDER_MAX_INSTRUCTIONS * 5u];
static uint16_t g_instr_count;
static int g_loaded;

static uint32_t crc32_ieee(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++)
            crc = crc & 1u ? (crc >> 1) ^ 0xedb88320u : (crc >> 1);
    }
    return crc ^ 0xffffffffu;
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint8_t read_bool(uint16_t mem_class, uint16_t addr)
{
    if (mem_class == MEM_DISCRETE_INPUT)
        return reg_get_discrete_input(addr);
    return reg_get_coil(addr);
}

void ladder_vm_clear_program(void)
{
    g_loaded = 0;
    g_instr_count = 0;
    memset(g_instr, 0, sizeof(g_instr));
}

int ladder_vm_program_loaded(void)
{
    return g_loaded;
}

int ladder_vm_load_blob(const uint8_t *data, size_t len)
{
    ladder_vm_clear_program();
    if (len < 12)
        return -1;
    uint32_t magic = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
    if (magic != PLCBIN_MAGIC)
        return -1;
    uint16_t count = rd16(data + 6);
    uint32_t crc_expect = (uint32_t)data[8] | ((uint32_t)data[9] << 8) | ((uint32_t)data[10] << 16) | ((uint32_t)data[11] << 24);
    size_t payload_len = (size_t)count * 5u;
    if (len < 12 + payload_len)
        return -1;
    const uint8_t *payload = data + 12;
    if (crc32_ieee(payload, payload_len) != crc_expect)
        return -1;
    if (count > LADDER_MAX_INSTRUCTIONS)
        return -1;
    memcpy(g_instr, payload, payload_len);
    g_instr_count = count;
    g_loaded = 1;
    return 0;
}

static int push(uint8_t *stack, int *sp, uint8_t v)
{
    if (*sp >= LADDER_STACK_DEPTH - 1)
        return -1;
    stack[++(*sp)] = v ? 1u : 0u;
    return 0;
}

static uint8_t pop(uint8_t *stack, int *sp)
{
    if (*sp < 0)
        return 0;
    return stack[(*sp)--];
}

void ladder_vm_execute_scan(void)
{
    if (!g_loaded || g_instr_count == 0)
        return;

    uint8_t stack[LADDER_STACK_DEPTH];
    int sp = -1;

    for (uint16_t i = 0; i < g_instr_count; i++) {
        const uint8_t *p = g_instr + (size_t)i * 5u;
        uint8_t op = p[0];
        uint16_t o1 = rd16(p + 1);
        uint16_t o2 = rd16(p + 3);

        switch (op) {
        case OPC_LD: {
            uint8_t v = read_bool(o1, o2);
            push(stack, &sp, v);
            break;
        }
        case OPC_LD_NOT: {
            uint8_t v = read_bool(o1, o2) ? 0u : 1u;
            push(stack, &sp, v);
            break;
        }
        case OPC_AND: {
            uint8_t b = pop(stack, &sp);
            uint8_t a = pop(stack, &sp);
            push(stack, &sp, (uint8_t)(a && b));
            break;
        }
        case OPC_AND_NOT: {
            uint8_t v = read_bool(o1, o2) ? 0u : 1u;
            uint8_t a = pop(stack, &sp);
            push(stack, &sp, (uint8_t)(a && v));
            break;
        }
        case OPC_OR: {
            uint8_t b = pop(stack, &sp);
            uint8_t a = pop(stack, &sp);
            push(stack, &sp, (uint8_t)(a || b));
            break;
        }
        case OPC_OR_NOT: {
            uint8_t v = read_bool(o1, o2) ? 0u : 1u;
            uint8_t a = pop(stack, &sp);
            push(stack, &sp, (uint8_t)(a || v));
            break;
        }
        case OPC_OUT: {
            uint8_t v = pop(stack, &sp);
            reg_set_coil(o2, v);
            break;
        }
        case OPC_OUT_NOT: {
            uint8_t v = pop(stack, &sp);
            reg_set_coil(o2, v ? 0u : 1u);
            break;
        }
        case OPC_SET: {
            uint8_t v = pop(stack, &sp);
            if (v)
                reg_set_coil(o2, 1);
            break;
        }
        case OPC_RST: {
            uint8_t v = pop(stack, &sp);
            if (v)
                reg_set_coil(o2, 0);
            break;
        }
        case OPC_MOV:
        case OPC_CMP_EQ:
        case OPC_CMP_GT:
        case OPC_TON:
        case OPC_TOF:
        case OPC_CTU:
            break;
        case OPC_END:
            return;
        default:
            break;
        }
    }
}
