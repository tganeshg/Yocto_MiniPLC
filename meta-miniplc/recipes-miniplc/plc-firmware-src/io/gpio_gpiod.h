#ifndef GPIO_GPIOD_H
#define GPIO_GPIOD_H

#include "config/device_config.h"

void gpio_gpiod_init(const DeviceConfig *cfg);
void gpio_gpiod_shutdown(void);

/** Read DI into discrete_inputs [0..3]; no-op if GPIO unavailable. */
void gpio_gpiod_sample_inputs(void);

/** Write coils [0..3] to physical DO; no-op if unavailable. */
void gpio_gpiod_apply_outputs(void);

int gpio_gpiod_available(void);

#endif
