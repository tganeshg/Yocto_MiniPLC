#include "plc_engine.h"

#include "../io/gpio_gpiod.h"
#include "../ladder/ladder_vm.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static atomic_uint_fast32_t g_scan_ms = ATOMIC_VAR_INIT(10);
static atomic_int_fast32_t g_scan_alive = ATOMIC_VAR_INIT(1);
static atomic_int_fast32_t g_logic_run = ATOMIC_VAR_INIT(0);

static atomic_uint_fast64_t g_cycle_ns_last = ATOMIC_VAR_INIT(0);
static atomic_uint_fast64_t g_cycle_ns_avg = ATOMIC_VAR_INIT(0);
static atomic_uint_fast64_t g_overruns = ATOMIC_VAR_INIT(0);

static uint64_t timespec_ns(const struct timespec *ts)
{
    return (uint64_t)ts->tv_sec * 1000000000ull + (uint64_t)ts->tv_nsec;
}

static void sleep_until_next_cycle(const struct timespec *t_start, uint32_t scan_ms)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t elapsed_us = (timespec_ns(&now) - timespec_ns(t_start)) / 1000ull;
    uint64_t budget_us = (uint64_t)scan_ms * 1000ull;
    if (elapsed_us < budget_us)
        usleep((useconds_t)((budget_us - elapsed_us)));
}

void plc_engine_init(const DeviceConfig *cfg)
{
    (void)cfg;
    gpio_gpiod_init(cfg);
}

void plc_engine_set_scan_ms(uint32_t ms)
{
    if (ms >= 1u && ms <= 100u)
        atomic_store_explicit(&g_scan_ms, ms, memory_order_relaxed);
}

uint32_t plc_engine_get_scan_ms(void)
{
    return (uint32_t)atomic_load_explicit(&g_scan_ms, memory_order_relaxed);
}

void plc_engine_request_stop(void)
{
    atomic_store_explicit(&g_scan_alive, 0, memory_order_relaxed);
}

int plc_engine_scan_thread_alive(void)
{
    return atomic_load_explicit(&g_scan_alive, memory_order_relaxed) != 0;
}

void plc_engine_logic_start(void)
{
    atomic_store_explicit(&g_logic_run, 1, memory_order_relaxed);
}

void plc_engine_logic_stop(void)
{
    atomic_store_explicit(&g_logic_run, 0, memory_order_relaxed);
}

int plc_engine_logic_is_running(void)
{
    return atomic_load_explicit(&g_logic_run, memory_order_relaxed) != 0;
}

uint64_t plc_engine_last_cycle_ns(void)
{
    return atomic_load_explicit(&g_cycle_ns_last, memory_order_relaxed);
}

uint64_t plc_engine_avg_cycle_ns(void)
{
    return atomic_load_explicit(&g_cycle_ns_avg, memory_order_relaxed);
}

uint64_t plc_engine_overrun_count(void)
{
    return atomic_load_explicit(&g_overruns, memory_order_relaxed);
}

void *plc_engine_scan_thread_main(void *arg)
{
    (void)arg;

    while (atomic_load_explicit(&g_scan_alive, memory_order_relaxed)) {
        struct timespec t_start;
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        gpio_gpiod_sample_inputs();

        if (plc_engine_logic_is_running() && ladder_vm_program_loaded())
            ladder_vm_execute_scan();

        gpio_gpiod_apply_outputs();

        struct timespec t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        uint64_t dur = timespec_ns(&t_end) - timespec_ns(&t_start);
        atomic_store_explicit(&g_cycle_ns_last, dur, memory_order_relaxed);

        uint64_t prev_avg = atomic_load_explicit(&g_cycle_ns_avg, memory_order_relaxed);
        uint64_t next_avg = prev_avg == 0 ? dur : (prev_avg * 15ull + dur) / 16ull;
        atomic_store_explicit(&g_cycle_ns_avg, next_avg, memory_order_relaxed);

        uint32_t scan_ms = plc_engine_get_scan_ms();
        struct timespec t_after_work;
        clock_gettime(CLOCK_MONOTONIC, &t_after_work);
        uint64_t wall = timespec_ns(&t_after_work) - timespec_ns(&t_start);
        if (wall > (uint64_t)scan_ms * 1000000ull)
            atomic_fetch_add_explicit(&g_overruns, 1, memory_order_relaxed);

        sleep_until_next_cycle(&t_start, scan_ms);
    }
    return NULL;
}
