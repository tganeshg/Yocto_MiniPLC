#include "device_config.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <json-c/json.h>

void device_config_init_defaults(DeviceConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->gpio_chip, sizeof(cfg->gpio_chip), "/dev/gpiochip0");
    /* BCM lines — override in /etc/plc/config.json for your wiring */
    cfg->di_line[0] = 17;
    cfg->di_line[1] = 18;
    cfg->di_line[2] = 27;
    cfg->di_line[3] = 22;
    cfg->do_line[0] = 23;
    cfg->do_line[1] = 24;
    cfg->do_line[2] = 25;
    cfg->do_line[3] = 5;
    cfg->scan_ms = 10u;
}

static int read_int_array(json_object *arr, int *out, int n)
{
    if (!json_object_is_type(arr, json_type_array))
        return -1;
    int len = json_object_array_length(arr);
    for (int i = 0; i < n; i++) {
        if (i >= len)
            return -1;
        json_object *el = json_object_array_get_idx(arr, i);
        if (!el || !json_object_is_type(el, json_type_int))
            return -1;
        out[i] = json_object_get_int(el);
    }
    return 0;
}

void device_config_load_file(const char *path, DeviceConfig *cfg)
{
    device_config_init_defaults(cfg);

    json_object *root = json_object_from_file(path);
    if (!root)
        return;

    json_object *o;

    if (json_object_object_get_ex(root, "gpio", &o) && json_object_is_type(o, json_type_object)) {
        json_object *chip, *di, *doarr;
        if (json_object_object_get_ex(o, "chip", &chip) && json_object_is_type(chip, json_type_string))
            snprintf(cfg->gpio_chip, sizeof(cfg->gpio_chip), "%s", json_object_get_string(chip));

        if (json_object_object_get_ex(o, "di_lines", &di)) {
            if (read_int_array(di, cfg->di_line, MINIPLC_GPIO_CHANNELS) != 0)
                device_config_init_defaults(cfg);
        }
        if (json_object_object_get_ex(o, "do_lines", &doarr)) {
            if (read_int_array(doarr, cfg->do_line, MINIPLC_GPIO_CHANNELS) != 0)
                device_config_init_defaults(cfg);
        }
    }

    if (json_object_object_get_ex(root, "scan_ms", &o) && json_object_is_type(o, json_type_int)) {
        uint32_t v = (uint32_t)json_object_get_int(o);
        if (v >= 1u && v <= 100u)
            cfg->scan_ms = v;
    }

    json_object_put(root);
}

int device_config_save_file(const char *path, const DeviceConfig *cfg)
{
    json_object *root = json_object_new_object();

    json_object *gpio = json_object_new_object();
    json_object_object_add(gpio, "chip", json_object_new_string(cfg->gpio_chip));

    json_object *di = json_object_new_array();
    json_object *dol = json_object_new_array();
    for (int i = 0; i < MINIPLC_GPIO_CHANNELS; i++) {
        json_object_array_add(di, json_object_new_int(cfg->di_line[i]));
        json_object_array_add(dol, json_object_new_int(cfg->do_line[i]));
    }
    json_object_object_add(gpio, "di_lines", di);
    json_object_object_add(gpio, "do_lines", dol);
    json_object_object_add(root, "gpio", gpio);

    json_object_object_add(root, "scan_ms", json_object_new_int((int32_t)cfg->scan_ms));

    int fd = json_object_to_file_ext(path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return fd == -1 ? -errno : 0;
}
