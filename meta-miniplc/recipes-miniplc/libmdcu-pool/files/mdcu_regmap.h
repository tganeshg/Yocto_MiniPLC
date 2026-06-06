/**
 * @file  mdcu_regmap.h
 * @brief Canonical address-map for the 50,000-register MDCU pool.
 *
 * One writer per range.  Enforced by configuration, not by the library.
 *
 *   [    0 ..    99]   system           : CPU temp, uptime, RTC, build info
 *   [  100 ..   999]   local I/O        : GPIO digital + analog
 *   [ 1000 .. 19999]   Modbus pollers   : up to N slave instances
 *   [20000 .. 29999]   DLMS pollers     : smart-meter OBIS values
 *   [30000 .. 39999]   Ladder VM        : coils + scratch
 *   [40000 .. 44999]   MQTT / IoT       : reserved for cloud-side mirroring
 *   [45000 .. 49999]   Retentive        : persisted to /etc/mdcu/retentive.bin
 */
#ifndef MDCU_REGMAP_H
#define MDCU_REGMAP_H

/* ---- range bases ---- */
#define MDCU_RNG_SYS_BASE         0U
#define MDCU_RNG_SYS_LEN          100U

#define MDCU_RNG_LOCALIO_BASE     100U
#define MDCU_RNG_LOCALIO_LEN      900U

#define MDCU_RNG_MODBUS_BASE      1000U
#define MDCU_RNG_MODBUS_LEN       19000U

#define MDCU_RNG_DLMS_BASE        20000U
#define MDCU_RNG_DLMS_LEN         10000U

#define MDCU_RNG_LADDER_BASE      30000U
#define MDCU_RNG_LADDER_LEN       10000U

#define MDCU_RNG_MQTT_BASE        40000U
#define MDCU_RNG_MQTT_LEN         5000U

#define MDCU_RNG_RETENT_BASE      45000U
#define MDCU_RNG_RETENT_LEN       5000U

/* ---- friendly slot offsets within the SYS range (0..99) ----
 * NB: i32 chosen for CPU temp because i16 millicelsius overflows at 32.7 °C.
 *     The pool reserves four consecutive 16-bit regs for each i32/u32 slot. */
/* CPU temperature in millicelsius, signed 32-bit, at SYS+0..1 */
#define MDCU_SYS_CPU_TEMP_MILLIC  (MDCU_RNG_SYS_BASE + 0U)
/* System uptime seconds, u32 at SYS+2..3 */
#define MDCU_SYS_UPTIME_SEC       (MDCU_RNG_SYS_BASE + 2U)
/* Wall-clock epoch seconds, u32 at SYS+4..5 */
#define MDCU_SYS_EPOCH            (MDCU_RNG_SYS_BASE + 4U)
/* Heartbeat counter, u16 at SYS+6 (bumped every HMI tick) */
#define MDCU_SYS_HEARTBEAT        (MDCU_RNG_SYS_BASE + 6U)

#endif /* MDCU_REGMAP_H */
