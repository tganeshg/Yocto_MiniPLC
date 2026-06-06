/**
 * @file mdcu_pool.c
 * @brief Implementation of libmdcu-pool — see mdcu_pool.h.
 */
#define _GNU_SOURCE
#include "mdcu_pool.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MDCU_MAGIC      0x4D444355U   /* 'MDCU' */
#define MDCU_VERSION    0x00010000U   /* major.minor */

/* On-shm layout.  Bump MDCU_VERSION if this struct changes. */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t n_regs;
    uint32_t pool_bytes;
    uint32_t seq;            /* global seqlock for multi-word ops */
    uint32_t _rsv[3];        /* align regs[] to 32-byte boundary */
    uint16_t regs[MDCU_POOL_REGS];
} mdcu_shm_t;

static mdcu_shm_t *g_shm = NULL;
static int         g_fd  = -1;

/* ===== seqlock for multi-word reads/writes ============================= */
static inline uint32_t seq_begin_read(void) {
    uint32_t s;
    do {
        s = __atomic_load_n(&g_shm->seq, __ATOMIC_ACQUIRE);
    } while (s & 1U);
    return s;
}
static inline int seq_retry(uint32_t s) {
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    return __atomic_load_n(&g_shm->seq, __ATOMIC_RELAXED) != s;
}
static inline void seq_begin_write(void) {
    __atomic_fetch_add(&g_shm->seq, 1U, __ATOMIC_ACQ_REL);
}
static inline void seq_end_write(void) {
    __atomic_fetch_add(&g_shm->seq, 1U, __ATOMIC_RELEASE);
}

/* ===== lifecycle ====================================================== */
int mdcu_pool_is_open(void) { return g_shm != NULL; }

int mdcu_pool_open(void)
{
    if (g_shm) return 0;

    int fd = shm_open(MDCU_SHM_NAME, O_RDWR | O_CREAT, 0666);
    if (fd < 0) return -1;

    /* shm_open honours umask; make the segment world-readable for cross-uid use. */
    (void)fchmod(fd, 0666);

    struct stat st;
    if (fstat(fd, &st) < 0) { int e = errno; close(fd); errno = e; return -1; }

    int first = (st.st_size < (off_t)sizeof(mdcu_shm_t));
    if (first) {
        if (ftruncate(fd, sizeof(mdcu_shm_t)) < 0) {
            int e = errno; close(fd); errno = e; return -1;
        }
    }

    void *p = mmap(NULL, sizeof(mdcu_shm_t), PROT_READ | PROT_WRITE,
                   MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { int e = errno; close(fd); errno = e; return -1; }

    g_shm = (mdcu_shm_t *)p;
    g_fd  = fd;

    /* First-time init, or re-init if magic mismatches (e.g. layout change). */
    if (first || g_shm->magic != MDCU_MAGIC || g_shm->version != MDCU_VERSION) {
        memset(g_shm, 0, sizeof(*g_shm));
        g_shm->magic      = MDCU_MAGIC;
        g_shm->version    = MDCU_VERSION;
        g_shm->n_regs     = MDCU_POOL_REGS;
        g_shm->pool_bytes = MDCU_POOL_BYTES;
    }
    return 0;
}

void mdcu_pool_close(void)
{
    if (g_shm) { munmap(g_shm, sizeof(*g_shm)); g_shm = NULL; }
    if (g_fd >= 0) { close(g_fd); g_fd = -1; }
}

/* ===== bounds-check helpers ============================================ */
#define BOUNDS_RW(n) do {                                                 \
    if (!g_shm) { errno = ENODEV; return -1; }                            \
    if ((uint64_t)reg + (uint64_t)(n) > (uint64_t)MDCU_POOL_REGS) {       \
        errno = ERANGE; return -1;                                        \
    }                                                                     \
} while (0)

#define BOUNDS_R(n, zero) do {                                            \
    if (!g_shm) { errno = ENODEV; return (zero); }                        \
    if ((uint64_t)reg + (uint64_t)(n) > (uint64_t)MDCU_POOL_REGS) {       \
        errno = ERANGE; return (zero);                                    \
    }                                                                     \
} while (0)

/* ===== single-word reads (no seqlock — single load is atomic) ========= */
bool mdcu_get_bit(uint32_t reg, uint8_t bit)
{
    BOUNDS_R(1, false);
    if (bit > 15) { errno = ERANGE; return false; }
    uint16_t w = __atomic_load_n(&g_shm->regs[reg], __ATOMIC_ACQUIRE);
    return (w >> bit) & 1U;
}
uint8_t mdcu_get_u8(uint32_t reg, uint8_t half)
{
    BOUNDS_R(1, 0);
    if (half > 1) { errno = ERANGE; return 0; }
    uint16_t w = __atomic_load_n(&g_shm->regs[reg], __ATOMIC_ACQUIRE);
    return half ? (uint8_t)(w >> 8) : (uint8_t)(w & 0xFFU);
}
int8_t mdcu_get_i8(uint32_t reg, uint8_t half) {
    return (int8_t)mdcu_get_u8(reg, half);
}
uint16_t mdcu_get_u16(uint32_t reg)
{
    BOUNDS_R(1, 0);
    return __atomic_load_n(&g_shm->regs[reg], __ATOMIC_ACQUIRE);
}
int16_t mdcu_get_i16(uint32_t reg) { return (int16_t)mdcu_get_u16(reg); }

/* ===== multi-word reads (seqlock-protected) =========================== */
uint32_t mdcu_get_u32(uint32_t reg)
{
    BOUNDS_R(2, 0);
    uint32_t s, lo, hi;
    do {
        s  = seq_begin_read();
        lo = __atomic_load_n(&g_shm->regs[reg],   __ATOMIC_RELAXED);
        hi = __atomic_load_n(&g_shm->regs[reg+1], __ATOMIC_RELAXED);
    } while (seq_retry(s));
    return ((uint32_t)hi << 16) | (uint32_t)lo;
}
int32_t mdcu_get_i32(uint32_t reg) {
    return (int32_t)mdcu_get_u32(reg);
}
float mdcu_get_f32(uint32_t reg) {
    uint32_t u = mdcu_get_u32(reg);
    float f;
    memcpy(&f, &u, sizeof(f));
    return f;
}
uint64_t mdcu_get_u64(uint32_t reg)
{
    BOUNDS_R(4, 0);
    uint32_t s;
    uint16_t w[4];
    do {
        s = seq_begin_read();
        w[0] = __atomic_load_n(&g_shm->regs[reg],   __ATOMIC_RELAXED);
        w[1] = __atomic_load_n(&g_shm->regs[reg+1], __ATOMIC_RELAXED);
        w[2] = __atomic_load_n(&g_shm->regs[reg+2], __ATOMIC_RELAXED);
        w[3] = __atomic_load_n(&g_shm->regs[reg+3], __ATOMIC_RELAXED);
    } while (seq_retry(s));
    return  ((uint64_t)w[3] << 48) |
            ((uint64_t)w[2] << 32) |
            ((uint64_t)w[1] << 16) |
             (uint64_t)w[0];
}
int64_t mdcu_get_i64(uint32_t reg) {
    return (int64_t)mdcu_get_u64(reg);
}
double mdcu_get_f64(uint32_t reg) {
    uint64_t u = mdcu_get_u64(reg);
    double d;
    memcpy(&d, &u, sizeof(d));
    return d;
}

/* ===== single-word writes ============================================= */
int mdcu_set_bit(uint32_t reg, uint8_t bit, bool v)
{
    BOUNDS_RW(1);
    if (bit > 15) { errno = ERANGE; return -1; }
    uint16_t m = (uint16_t)(1U << bit);
    if (v) __atomic_or_fetch (&g_shm->regs[reg],  m,           __ATOMIC_RELEASE);
    else   __atomic_and_fetch(&g_shm->regs[reg], (uint16_t)~m, __ATOMIC_RELEASE);
    return 0;
}
int mdcu_set_u8(uint32_t reg, uint8_t half, uint8_t v)
{
    BOUNDS_RW(1);
    if (half > 1) { errno = ERANGE; return -1; }
    /* CAS-loop on the containing word to preserve the unchanged byte half. */
    uint16_t old = __atomic_load_n(&g_shm->regs[reg], __ATOMIC_RELAXED);
    uint16_t nw;
    do {
        nw = half
            ? (uint16_t)((old & 0x00FFU) | ((uint16_t)v << 8))
            : (uint16_t)((old & 0xFF00U) |  (uint16_t)v);
    } while (!__atomic_compare_exchange_n(&g_shm->regs[reg], &old, nw, 1,
                                          __ATOMIC_RELEASE, __ATOMIC_RELAXED));
    return 0;
}
int mdcu_set_i8(uint32_t reg, uint8_t half, int8_t v) {
    return mdcu_set_u8(reg, half, (uint8_t)v);
}
int mdcu_set_u16(uint32_t reg, uint16_t v)
{
    BOUNDS_RW(1);
    __atomic_store_n(&g_shm->regs[reg], v, __ATOMIC_RELEASE);
    return 0;
}
int mdcu_set_i16(uint32_t reg, int16_t v) {
    return mdcu_set_u16(reg, (uint16_t)v);
}

/* ===== multi-word writes (seqlock-protected) ========================== */
int mdcu_set_u32(uint32_t reg, uint32_t v)
{
    BOUNDS_RW(2);
    seq_begin_write();
    __atomic_store_n(&g_shm->regs[reg],   (uint16_t)( v        & 0xFFFFU), __ATOMIC_RELAXED);
    __atomic_store_n(&g_shm->regs[reg+1], (uint16_t)((v >> 16) & 0xFFFFU), __ATOMIC_RELAXED);
    seq_end_write();
    return 0;
}
int mdcu_set_i32(uint32_t reg, int32_t v) {
    return mdcu_set_u32(reg, (uint32_t)v);
}
int mdcu_set_f32(uint32_t reg, float v) {
    uint32_t u;
    memcpy(&u, &v, sizeof(u));
    return mdcu_set_u32(reg, u);
}
int mdcu_set_u64(uint32_t reg, uint64_t v)
{
    BOUNDS_RW(4);
    seq_begin_write();
    __atomic_store_n(&g_shm->regs[reg],   (uint16_t)( v        & 0xFFFFU), __ATOMIC_RELAXED);
    __atomic_store_n(&g_shm->regs[reg+1], (uint16_t)((v >> 16) & 0xFFFFU), __ATOMIC_RELAXED);
    __atomic_store_n(&g_shm->regs[reg+2], (uint16_t)((v >> 32) & 0xFFFFU), __ATOMIC_RELAXED);
    __atomic_store_n(&g_shm->regs[reg+3], (uint16_t)((v >> 48) & 0xFFFFU), __ATOMIC_RELAXED);
    seq_end_write();
    return 0;
}
int mdcu_set_i64(uint32_t reg, int64_t v) {
    return mdcu_set_u64(reg, (uint64_t)v);
}
int mdcu_set_f64(uint32_t reg, double v) {
    uint64_t u;
    memcpy(&u, &v, sizeof(u));
    return mdcu_set_u64(reg, u);
}

/* ===== bulk ============================================================ */
int mdcu_read_block(uint32_t reg, uint16_t *dst, uint32_t n)
{
    if (!dst) { errno = EINVAL; return -1; }
    BOUNDS_RW(n);
    if (n == 0) return 0;
    uint32_t s;
    do {
        s = seq_begin_read();
        for (uint32_t i = 0; i < n; ++i)
            dst[i] = __atomic_load_n(&g_shm->regs[reg+i], __ATOMIC_RELAXED);
    } while (seq_retry(s));
    return 0;
}
int mdcu_write_block(uint32_t reg, const uint16_t *src, uint32_t n)
{
    if (!src) { errno = EINVAL; return -1; }
    BOUNDS_RW(n);
    if (n == 0) return 0;
    seq_begin_write();
    for (uint32_t i = 0; i < n; ++i)
        __atomic_store_n(&g_shm->regs[reg+i], src[i], __ATOMIC_RELAXED);
    seq_end_write();
    return 0;
}

/* ===== type-tagged accessors ========================================== */
int mdcu_get(uint32_t reg, uint8_t sub, mdcu_type_t type, mdcu_value_t *out)
{
    if (!out) { errno = EINVAL; return -1; }
    out->type = type;
    switch (type) {
    case MDCU_T_BIT: out->v.b   = mdcu_get_bit(reg, sub); break;
    case MDCU_T_U8:  out->v.u8  = mdcu_get_u8 (reg, sub); break;
    case MDCU_T_I8:  out->v.i8  = mdcu_get_i8 (reg, sub); break;
    case MDCU_T_U16: out->v.u16 = mdcu_get_u16(reg); break;
    case MDCU_T_I16: out->v.i16 = mdcu_get_i16(reg); break;
    case MDCU_T_U32: out->v.u32 = mdcu_get_u32(reg); break;
    case MDCU_T_I32: out->v.i32 = mdcu_get_i32(reg); break;
    case MDCU_T_F32: out->v.f32 = mdcu_get_f32(reg); break;
    case MDCU_T_U64: out->v.u64 = mdcu_get_u64(reg); break;
    case MDCU_T_I64: out->v.i64 = mdcu_get_i64(reg); break;
    case MDCU_T_F64: out->v.f64 = mdcu_get_f64(reg); break;
    default: errno = EINVAL; return -1;
    }
    return 0;
}
int mdcu_set(uint32_t reg, uint8_t sub, const mdcu_value_t *in)
{
    if (!in) { errno = EINVAL; return -1; }
    switch (in->type) {
    case MDCU_T_BIT: return mdcu_set_bit(reg, sub, in->v.b);
    case MDCU_T_U8:  return mdcu_set_u8 (reg, sub, in->v.u8);
    case MDCU_T_I8:  return mdcu_set_i8 (reg, sub, in->v.i8);
    case MDCU_T_U16: return mdcu_set_u16(reg, in->v.u16);
    case MDCU_T_I16: return mdcu_set_i16(reg, in->v.i16);
    case MDCU_T_U32: return mdcu_set_u32(reg, in->v.u32);
    case MDCU_T_I32: return mdcu_set_i32(reg, in->v.i32);
    case MDCU_T_F32: return mdcu_set_f32(reg, in->v.f32);
    case MDCU_T_U64: return mdcu_set_u64(reg, in->v.u64);
    case MDCU_T_I64: return mdcu_set_i64(reg, in->v.i64);
    case MDCU_T_F64: return mdcu_set_f64(reg, in->v.f64);
    default: errno = EINVAL; return -1;
    }
}
