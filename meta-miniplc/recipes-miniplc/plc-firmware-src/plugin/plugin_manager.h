#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "plc_plugin.h"

#define PLC_PLUGIN_ENTRY_SYM "plc_plugin_entry"
#define PLC_PLUGIN_MAX_LOADED 8

typedef struct PLCPluginHandle PLCPluginHandle;

int plugin_manager_init(void);
void plugin_manager_shutdown(void);

/**
 * Load plugin from absolute path; returns 0 on success.
 * ABI mismatch or dlopen failure returns negative errno-style code.
 */
int plugin_manager_load(const char *path_so, PLCPluginHandle **out_handle);

PLCPlugin *plugin_manager_describe(PLCPluginHandle *handle);
void plugin_manager_unload(PLCPluginHandle *handle);

#endif /* PLUGIN_MANAGER_H */
