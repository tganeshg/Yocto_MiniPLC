#ifndef PLC_ENGINE_H
#define PLC_ENGINE_H

#include <stdint.h>

#include "config/device_config.h"

#define MINIPLC_FW_VERSION "1.0.0"

void plc_engine_init(const DeviceConfig *cfg);

void plc_engine_set_scan_ms(uint32_t ms);
uint32_t plc_engine_get_scan_ms(void);

/** Stop scan thread (process shutdown). */
void plc_engine_request_stop(void);
int plc_engine_scan_thread_alive(void);

/** PLC logic execution (ladder). */
void plc_engine_logic_start(void);
void plc_engine_logic_stop(void);
int plc_engine_logic_is_running(void);

uint64_t plc_engine_last_cycle_ns(void);
uint64_t plc_engine_avg_cycle_ns(void);
uint64_t plc_engine_overrun_count(void);

void *plc_engine_scan_thread_main(void *arg);

#endif /* PLC_ENGINE_H */
