#include "api/rest_api.h"
#include "config/device_config.h"
#include "core/paths.h"
#include "core/plc_engine.h"
#include "core/register_map.h"
#include "ladder/ladder_vm.h"
#include "plugin/plugin_manager.h"
#include "project/project_store.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static pthread_t g_scan_thread;
static struct mg_context *g_http;
static volatile sig_atomic_t g_quit;

static DeviceConfig g_cfg;

static void on_signal(int signo)
{
    (void)signo;
    g_quit = 1;
    rest_api_stop(g_http);
    g_http = NULL;
    plc_engine_request_stop();
}

int main(void)
{
    mkdir("/etc/plc", 0755);
    mkdir("/var/lib/plc", 0755);
    mkdir("/var/lib/plc/hmi", 0755);

    device_config_load_file(MINIPLC_CONFIG_FILE, &g_cfg);

    reg_map_init();
    plc_engine_init(&g_cfg);
    plc_engine_set_scan_ms(g_cfg.scan_ms);

    plugin_manager_init();

    if (project_store_load_persisted_program() == 0 && ladder_vm_program_loaded())
        plc_engine_logic_start();
    else
        plc_engine_logic_stop();

    struct sigaction sa = {.sa_handler = on_signal};
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (pthread_create(&g_scan_thread, NULL, plc_engine_scan_thread_main, NULL) != 0) {
        perror("pthread_create");
        return EXIT_FAILURE;
    }

    g_http = rest_api_start(&g_cfg);
    if (!g_http) {
        fputs("plc_firmware: REST API (civetweb) failed to start\n", stderr);
    }

    while (!g_quit)
        pause();

    pthread_join(g_scan_thread, NULL);
    plugin_manager_shutdown();
    return EXIT_SUCCESS;
}
