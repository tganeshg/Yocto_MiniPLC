/**
 * @file  mdcu_pool.h
 * @brief MDCU unified register pool — typed accessors over shared memory.
 *
 * One process-image table shared by every subsystem on the MDCU:
 *   - Modbus / DLMS / future protocol pollers (writers to their assigned range)
 *   - Local I/O (GPIO, ADC)
 *   - Ladder VM
 *   - LVGL HMI
 *   - REST API / web UI
 *
 * Backing store: /dev/shm/mdcu_pool (POSIX shm), mmap'd by every consumer.
 * Storage layout: 50,000 × uint16_t in native (little-endian on ARMv6) order.
 * All wire-format byte/word swaps happen at the protocol boundary — never
 * inside the pool.
 *
 * Addressing:
 *   reg   ∈ [0..49999]   — 16-bit register index (the natural unit)
 *   bit   ∈ [0..15]      — bit within a register (for boolean values)
 *   half  ∈ {0=low, 1=high} — byte within a register (for 8-bit values)
 *
 * Type → footprint:
 *   bit             1 bit  (sub-word)
 *   u8/i8           1 byte (sub-word)
 *   u16/i16         1 register
 *   u32/i32/f32     2 consecutive registers (reg, reg+1)
 *   u64/i64/f64     4 consecutive registers (reg .. reg+3)
 *
 * Concurrency:
 *   - Single-word ops (bit/u8/u16) are atomic at the CPU level (ARMv6 LDREX*).
 *   - Multi-word ops (u32/u64/f32/f64 and bulk) are protected by a single
 *     global seqlock — readers spin on mismatch; writers bump twice.
 *   - One writer per range is enforced by config, not by the library.
 */
#ifndef MDCU_POOL_H
#define MDCU_POOL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MDCU_POOL_REGS     50000U
#define MDCU_POOL_BYTES    (MDCU_POOL_REGS * 2U)
#define MDCU_SHM_NAME      "/mdcu_pool"

/* ===== lifecycle ======================================================= */

/** Map (and create if absent) the shared register pool. Idempotent.
 *  Returns 0 on success, -1 + errno on failure. */
int  mdcu_pool_open(void);

/** Unmap and close the pool. */
void mdcu_pool_close(void);

/** True if the pool is currently mapped in this process. */
int  mdcu_pool_is_open(void);

/* ===== reads =========================================================== */
/* On bounds error: return 0/false and set errno=ERANGE. */

bool     mdcu_get_bit (uint32_t reg, uint8_t bit);   /* bit ∈ [0..15]  */
uint8_t  mdcu_get_u8  (uint32_t reg, uint8_t half);  /* half ∈ {0,1}   */
int8_t   mdcu_get_i8  (uint32_t reg, uint8_t half);
uint16_t mdcu_get_u16 (uint32_t reg);
int16_t  mdcu_get_i16 (uint32_t reg);
uint32_t mdcu_get_u32 (uint32_t reg);                /* reg, reg+1     */
int32_t  mdcu_get_i32 (uint32_t reg);
float    mdcu_get_f32 (uint32_t reg);
uint64_t mdcu_get_u64 (uint32_t reg);                /* reg .. reg+3   */
int64_t  mdcu_get_i64 (uint32_t reg);
double   mdcu_get_f64 (uint32_t reg);

/* ===== writes ========================================================== */
/* Return 0 on success, -1 + errno=ERANGE on out-of-bounds. */

int mdcu_set_bit (uint32_t reg, uint8_t bit,  bool v);
int mdcu_set_u8  (uint32_t reg, uint8_t half, uint8_t v);
int mdcu_set_i8  (uint32_t reg, uint8_t half, int8_t  v);
int mdcu_set_u16 (uint32_t reg, uint16_t v);
int mdcu_set_i16 (uint32_t reg, int16_t  v);
int mdcu_set_u32 (uint32_t reg, uint32_t v);
int mdcu_set_i32 (uint32_t reg, int32_t  v);
int mdcu_set_f32 (uint32_t reg, float    v);
int mdcu_set_u64 (uint32_t reg, uint64_t v);
int mdcu_set_i64 (uint32_t reg, int64_t  v);
int mdcu_set_f64 (uint32_t reg, double   v);

/* ===== bulk (for protocol pollers and Modbus FC03/FC16-style ops) ====== */

int mdcu_read_block (uint32_t reg, uint16_t *dst, uint32_t n_regs);
int mdcu_write_block(uint32_t reg, const uint16_t *src, uint32_t n_regs);

/* ===== type-tagged accessor (REST API / scripting / ladder VM) ========= */

typedef enum {
    MDCU_T_BIT = 0,
    MDCU_T_U8,  MDCU_T_I8,
    MDCU_T_U16, MDCU_T_I16,
    MDCU_T_U32, MDCU_T_I32, MDCU_T_F32,
    MDCU_T_U64, MDCU_T_I64, MDCU_T_F64,
} mdcu_type_t;

typedef struct {
    mdcu_type_t type;
    union {
        bool     b;
        uint8_t  u8;   int8_t  i8;
        uint16_t u16;  int16_t i16;
        uint32_t u32;  int32_t i32;  float  f32;
        uint64_t u64;  int64_t i64;  double f64;
    } v;
} mdcu_value_t;

/** Typed read. `sub` is the bit index (MDCU_T_BIT) or byte half (MDCU_T_U8/I8);
 *  ignored for wider types. */
int mdcu_get(uint32_t reg, uint8_t sub, mdcu_type_t type, mdcu_value_t *out);

/** Typed write. `sub` as above. */
int mdcu_set(uint32_t reg, uint8_t sub, const mdcu_value_t *in);

#ifdef __cplusplus
}
#endif
#endif /* MDCU_POOL_H */
