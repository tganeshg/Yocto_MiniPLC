#include "plugin_api.h"

#include <stdarg.h>
#include <stdio.h>

#include "core/register_map.h"

static void api_log(int level, const char *fmt, ...)
{
    va_list ap;
    (void)level;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static PLCRegisterAPI g_api = {.get_holding = reg_get_holding,
                               .set_holding = reg_set_holding,
                               .get_coil = reg_get_coil,
                               .set_coil = reg_set_coil,
                               .get_discrete_input = reg_get_discrete_input,
                               .get_input_register = reg_get_input_register,
                               .log = api_log};

PLCRegisterAPI *plugin_api_get_table(void)
{
    return &g_api;
}
