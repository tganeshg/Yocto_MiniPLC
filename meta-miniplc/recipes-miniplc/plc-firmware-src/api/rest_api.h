#ifndef REST_API_H
#define REST_API_H

#include <stddef.h>

struct mg_context;
struct DeviceConfig;

struct mg_context *rest_api_start(struct DeviceConfig *cfg);
void rest_api_stop(struct mg_context *ctx);

#endif
