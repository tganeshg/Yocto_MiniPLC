#include "gpio_gpiod.h"

#include "../core/register_map.h"

#include <gpiod.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

static struct gpiod_chip *chip;
static struct gpiod_line *di_lines[MINIPLC_GPIO_CHANNELS];
static struct gpiod_line *do_lines[MINIPLC_GPIO_CHANNELS];
static int gpio_ok;

static void release_lines(void)
{
    for (int i = 0; i < MINIPLC_GPIO_CHANNELS; i++) {
        if (di_lines[i]) {
            gpiod_line_release(di_lines[i]);
            di_lines[i] = NULL;
        }
        if (do_lines[i]) {
            gpiod_line_release(do_lines[i]);
            do_lines[i] = NULL;
        }
    }
    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }
}

void gpio_gpiod_init(const DeviceConfig *cfg)
{
    gpio_gpiod_shutdown();
    gpio_ok = 0;

    chip = gpiod_chip_open(cfg->gpio_chip);
    if (!chip) {
        fprintf(stderr, "plc_firmware: gpiod_chip_open(%s): %s\n", cfg->gpio_chip, strerror(errno));
        return;
    }

    for (int i = 0; i < MINIPLC_GPIO_CHANNELS; i++) {
        struct gpiod_line *dl = gpiod_chip_get_line(chip, (unsigned int)cfg->di_line[i]);
        if (!dl || gpiod_line_request_input(dl, "plc_firmware") < 0) {
            fprintf(stderr, "plc_firmware: DI line %d request failed\n", cfg->di_line[i]);
            release_lines();
            return;
        }
        di_lines[i] = dl;

        struct gpiod_line *ol = gpiod_chip_get_line(chip, (unsigned int)cfg->do_line[i]);
        if (!ol || gpiod_line_request_output(ol, "plc_firmware", 0) < 0) {
            fprintf(stderr, "plc_firmware: DO line %d request failed\n", cfg->do_line[i]);
            release_lines();
            return;
        }
        do_lines[i] = ol;
    }

    gpio_ok = 1;
}

void gpio_gpiod_shutdown(void)
{
    release_lines();
    gpio_ok = 0;
}

int gpio_gpiod_available(void)
{
    return gpio_ok;
}

void gpio_gpiod_sample_inputs(void)
{
    if (!gpio_ok)
        return;

    for (int i = 0; i < MINIPLC_GPIO_CHANNELS; i++) {
        int v = gpiod_line_get_value(di_lines[i]);
        if (v < 0)
            v = 0;
        reg_set_discrete_input((uint16_t)i, (uint8_t)v);
    }
}

void gpio_gpiod_apply_outputs(void)
{
    if (!gpio_ok)
        return;

    for (int i = 0; i < MINIPLC_GPIO_CHANNELS; i++) {
        uint8_t v = reg_get_coil((uint16_t)i);
        (void)gpiod_line_set_value(do_lines[i], v ? 1 : 0);
    }
}
