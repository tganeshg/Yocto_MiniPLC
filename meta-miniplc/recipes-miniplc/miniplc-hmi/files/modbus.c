/**
 * @file modbus.c
 * Modbus TCP Master wrapper.  All exported function names use the mb_ prefix
 * to avoid colliding with libmodbus's own modbus_* symbols (which the bridge
 * in libmodbus_bridge.c calls through lmb_* shims).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "modbus.h"
#include "libmodbus_bridge.h"

/**********************
 *  STATIC VARIABLES
 **********************/
static modbus_config_t modbus_cfg = {
    .ip_address    = "192.168.1.100",
    .port          = MODBUS_DEFAULT_PORT,
    .slave_id      = MODBUS_DEFAULT_SLAVE_ID,
    .start_address = 0,
    .num_registers = 10,
    .enabled       = false,
    .connected     = false
};

static modbus_data_t modbus_data = {0};
static void         *modbus_ctx  = NULL;
static const char   *CONFIG_FILE = "/tmp/modbus_config.txt";

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
int mb_init(void)
{
    mb_load_config();
    modbus_ctx = NULL;
    memset(&modbus_data, 0, sizeof(modbus_data));
    return 0;
}

void mb_deinit(void)
{
    mb_disconnect();
}

int mb_connect(const modbus_config_t *config)
{
    if (config == NULL)
        return -1;

    if (modbus_cfg.connected)
        mb_disconnect();

    memcpy(&modbus_cfg, config, sizeof(modbus_config_t));

    modbus_ctx = lmb_new_tcp(modbus_cfg.ip_address, (int)modbus_cfg.port);
    if (modbus_ctx == NULL) {
        fprintf(stderr, "[MODBUS] new_tcp failed for %s:%u\n",
                modbus_cfg.ip_address, modbus_cfg.port);
        return -1;
    }

    if (lmb_set_slave(modbus_ctx, modbus_cfg.slave_id) != 0) {
        fprintf(stderr, "[MODBUS] set_slave failed: %s\n", lmb_strerror(errno));
        lmb_free(modbus_ctx);
        modbus_ctx = NULL;
        return -1;
    }

    lmb_set_response_timeout(modbus_ctx, 2, 0);

    if (lmb_connect(modbus_ctx) == -1) {
        fprintf(stderr, "[MODBUS] connect to %s:%u failed: %s\n",
                modbus_cfg.ip_address, modbus_cfg.port, lmb_strerror(errno));
        lmb_free(modbus_ctx);
        modbus_ctx = NULL;
        modbus_cfg.connected = false;
        return -1;
    }

    modbus_cfg.connected = true;
    printf("[MODBUS] Connected to %s:%u slave=%u\n",
           modbus_cfg.ip_address, modbus_cfg.port, modbus_cfg.slave_id);
    return 0;
}

void mb_disconnect(void)
{
    if (modbus_ctx != NULL) {
        lmb_close(modbus_ctx);
        lmb_free(modbus_ctx);
        modbus_ctx = NULL;
    }
    modbus_cfg.connected = false;
}

int mb_read_registers(uint16_t start_addr, uint16_t num_regs, uint16_t *dest)
{
    if (!modbus_cfg.connected || modbus_ctx == NULL || dest == NULL)
        return -1;

    if (num_regs > MODBUS_MAX_REGISTERS)
        num_regs = MODBUS_MAX_REGISTERS;

    int result = lmb_read_registers(modbus_ctx, (int)start_addr, (int)num_regs, dest);
    if (result == -1) {
        modbus_data.error_count++;
        fprintf(stderr, "[MODBUS] read_registers failed: %s\n", lmb_strerror(errno));
        /* Drop the connection so the UI loop retries cleanly. */
        mb_disconnect();
        return -1;
    }

    modbus_data.success_count++;
    modbus_data.last_update_time = (uint32_t)time(NULL);
    for (uint16_t i = 0; i < num_regs && i < MODBUS_MAX_REGISTERS; i++)
        modbus_data.registers[i] = dest[i];

    return 0;
}

int mb_get_config(modbus_config_t *config)
{
    if (config == NULL)
        return -1;
    memcpy(config, &modbus_cfg, sizeof(modbus_config_t));
    return 0;
}

int mb_set_config(const modbus_config_t *config)
{
    if (config == NULL)
        return -1;
    /* Preserve runtime "connected" flag — UI save shouldn't toggle the link. */
    bool was_connected = modbus_cfg.connected;
    memcpy(&modbus_cfg, config, sizeof(modbus_config_t));
    modbus_cfg.connected = was_connected;
    mb_save_config();
    return 0;
}

int mb_get_data(modbus_data_t *data)
{
    if (data == NULL)
        return -1;
    memcpy(data, &modbus_data, sizeof(modbus_data_t));
    return 0;
}

bool mb_is_connected(void)
{
    return modbus_cfg.connected && (modbus_ctx != NULL);
}

void mb_save_config(void)
{
    FILE *f = fopen(CONFIG_FILE, "w");
    if (f == NULL)
        return;
    fprintf(f, "%s\n", modbus_cfg.ip_address);
    fprintf(f, "%d\n", modbus_cfg.port);
    fprintf(f, "%d\n", modbus_cfg.slave_id);
    fprintf(f, "%d\n", modbus_cfg.start_address);
    fprintf(f, "%d\n", modbus_cfg.num_registers);
    fprintf(f, "%d\n", modbus_cfg.enabled ? 1 : 0);
    fclose(f);
}

void mb_load_config(void)
{
    FILE *f = fopen(CONFIG_FILE, "r");
    if (f == NULL) {
        fprintf(stderr, "[MODBUS] load_config: fopen(%s) failed: %s\n",
                CONFIG_FILE, strerror(errno));
        return;
    }

    char line[128];
    if (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        strncpy(modbus_cfg.ip_address, line, MODBUS_MAX_IP_LEN - 1);
        modbus_cfg.ip_address[MODBUS_MAX_IP_LEN - 1] = '\0';
    }
    if (fgets(line, sizeof(line), f)) modbus_cfg.port          = (uint16_t)atoi(line);
    if (fgets(line, sizeof(line), f)) modbus_cfg.slave_id      = (uint8_t)atoi(line);
    if (fgets(line, sizeof(line), f)) modbus_cfg.start_address = (uint16_t)atoi(line);
    if (fgets(line, sizeof(line), f)) modbus_cfg.num_registers = (uint16_t)atoi(line);
    if (fgets(line, sizeof(line), f)) modbus_cfg.enabled       = (atoi(line) != 0);
    fclose(f);
    fprintf(stderr, "[MODBUS] load_config: ip='%s' port=%u slave=%u start=%u nregs=%u en=%d\n",
            modbus_cfg.ip_address, modbus_cfg.port, modbus_cfg.slave_id,
            modbus_cfg.start_address, modbus_cfg.num_registers, modbus_cfg.enabled);
}
