#include "plugin_manager.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>

#include "plc_plugin.h"

struct PLCPluginHandle {
    void *dl;
    PLCPlugin *desc;
};

static size_t g_loaded_count;

int plugin_manager_init(void)
{
    g_loaded_count = 0;
    return 0;
}

void plugin_manager_shutdown(void)
{
    /* Caller must unload handles before shutdown in full implementation */
}

int plugin_manager_load(const char *path_so, PLCPluginHandle **out_handle)
{
    if (!path_so || !out_handle)
        return -EINVAL;
    if (g_loaded_count >= PLC_PLUGIN_MAX_LOADED)
        return -ENOMEM;

    void *dl = dlopen(path_so, RTLD_LAZY | RTLD_LOCAL);
    if (!dl)
        return -EIO;

    void *sym = dlsym(dl, PLC_PLUGIN_ENTRY_SYM);
    if (!sym) {
        dlclose(dl);
        return -ENOENT;
    }
    union {
        void *p;
        plc_plugin_entry_fn fn;
    } entry_u = {.p = sym};
    plc_plugin_entry_fn entry = entry_u.fn;

    PLCPlugin *desc = entry();
    if (!desc || desc->api_version != PLC_PLUGIN_API_VERSION) {
        dlclose(dl);
        return -EPROTO;
    }

    PLCPluginHandle *h = calloc(1, sizeof(*h));
    if (!h) {
        dlclose(dl);
        return -ENOMEM;
    }
    h->dl = dl;
    h->desc = desc;
    *out_handle = h;
    g_loaded_count++;
    return 0;
}

PLCPlugin *plugin_manager_describe(PLCPluginHandle *handle)
{
    return handle ? handle->desc : NULL;
}

void plugin_manager_unload(PLCPluginHandle *handle)
{
    if (!handle)
        return;
    if (handle->dl)
        dlclose(handle->dl);
    if (g_loaded_count > 0)
        g_loaded_count--;
    free(handle);
}
