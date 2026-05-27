#include "rest_api.h"

#include "config/device_config.h"
#include "../core/paths.h"
#include "../core/plc_engine.h"
#include "../core/register_map.h"
#include "../io/gpio_gpiod.h"
#include "../ladder/ladder_vm.h"
#include "../project/project_store.h"

#include <civetweb.h>

#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static DeviceConfig *s_cfg;

static int handle_registers_get(struct mg_connection *conn, void *cbdata);
static int handle_registers_post(struct mg_connection *conn, void *cbdata);
static int handle_config_get(struct mg_connection *conn, void *cbdata);
static int handle_config_post(struct mg_connection *conn, void *cbdata);
static int handle_system_time_get(struct mg_connection *conn, void *cbdata);
static int handle_system_time_post(struct mg_connection *conn, void *cbdata);

static void send_headers_json(struct mg_connection *conn, int code)
{
    mg_printf(conn,
              "HTTP/1.1 %d OK\r\n"
              "Content-Type: application/json; charset=utf-8\r\n"
              "Connection: close\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
              "Access-Control-Allow-Headers: Content-Type, X-Filename\r\n"
              "\r\n",
              code);
}

static void send_empty(struct mg_connection *conn, int code)
{
    mg_printf(conn,
              "HTTP/1.1 %d OK\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "\r\n",
              code);
}

static int handle_options(struct mg_connection *conn)
{
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "OPTIONS") != 0)
        return 0;
    send_empty(conn, 204);
    return 204;
}

static void reply_json_obj(struct mg_connection *conn, int http_code, struct json_object *obj)
{
    const char *js = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
    send_headers_json(conn, http_code);
    mg_printf(conn, "%s", js);
    json_object_put(obj);
}

static void reply_json_error(struct mg_connection *conn, int http_code, const char *msg)
{
    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "error", json_object_new_string(msg));
    reply_json_obj(conn, http_code, o);
}

static int miniplc_save_post_body(struct mg_connection *conn, const char *path)
{
    const struct mg_request_info *ri = mg_get_request_info(conn);
    long long cl = ri->content_length;
    if (cl < 0 || cl > 32 * 1024 * 1024)
        return -EINVAL;

    FILE *out = fopen(path, "wb");
    if (!out)
        return -errno;

    long long left = cl;
    char buf[8192];
    while (left > 0) {
        size_t chunk = (size_t)(left > (long long)sizeof(buf) ? sizeof(buf) : left);
        int n = mg_read(conn, buf, chunk);
        if (n <= 0)
            break;
        if (fwrite(buf, 1, (size_t)n, out) != (size_t)n) {
            fclose(out);
            unlink(path);
            return -EIO;
        }
        left -= n;
    }
    fclose(out);
    return 0;
}

static void parse_query_regs(const char *qs, char *type, size_t type_sz, uint16_t *start, uint16_t *count)
{
    strncpy(type, "holding", type_sz - 1);
    type[type_sz - 1] = 0;
    *start = 0;
    *count = 16;

    if (!qs || !*qs)
        return;

    char buf[384];
    strncpy(buf, qs, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    char *save = NULL;
    for (char *p = strtok_r(buf, "&", &save); p; p = strtok_r(NULL, "&", &save)) {
        char *eq = strchr(p, '=');
        if (!eq)
            continue;
        *eq++ = 0;
        if (!strcmp(p, "type"))
            snprintf(type, type_sz, "%s", eq);
        else if (!strcmp(p, "start"))
            *start = (uint16_t)strtoul(eq, NULL, 10);
        else if (!strcmp(p, "count"))
            *count = (uint16_t)strtoul(eq, NULL, 10);
    }
}

static int handle_status(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") != 0) {
        reply_json_error(conn, 405, "method not allowed");
        return 405;
    }

    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "plc_run", json_object_new_boolean(plc_engine_logic_is_running()));
    json_object_object_add(o, "logic_run", json_object_new_boolean(plc_engine_logic_is_running()));
    json_object_object_add(o, "program_loaded", json_object_new_boolean(ladder_vm_program_loaded()));
    json_object_object_add(o, "scan_ms", json_object_new_int((int32_t)plc_engine_get_scan_ms()));
    json_object_object_add(o, "cycle_ns_last", json_object_new_int64((int64_t)plc_engine_last_cycle_ns()));
    json_object_object_add(o, "cycle_ns_avg", json_object_new_int64((int64_t)plc_engine_avg_cycle_ns()));
    json_object_object_add(o, "overruns", json_object_new_int64((int64_t)plc_engine_overrun_count()));
    json_object_object_add(o, "gpio_ok", json_object_new_boolean(gpio_gpiod_available()));
    json_object_object_add(o, "fw_version", json_object_new_string(MINIPLC_FW_VERSION));
    reply_json_obj(conn, 200, o);
    return 200;
}

static int handle_program_start(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    plc_engine_logic_start();
    send_empty(conn, 204);
    return 204;
}

static int handle_program_stop(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    plc_engine_logic_stop();
    send_empty(conn, 204);
    return 204;
}

static int handle_registers(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (!strcmp(ri->request_method, "GET"))
        return handle_registers_get(conn, cbdata);
    if (!strcmp(ri->request_method, "POST"))
        return handle_registers_post(conn, cbdata);
    reply_json_error(conn, 405, "method not allowed");
    return 405;
}

static int handle_registers_get(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    char type[32];
    uint16_t start, count;
    parse_query_regs(ri->query_string, type, sizeof(type), &start, &count);
    if (count == 0 || count > 512)
        count = 16;

    struct json_object *arr = json_object_new_array();
    if (!strcmp(type, "coil") || !strcmp(type, "coils")) {
        uint8_t tmp[512];
        reg_map_read_coils_block(start, count, tmp);
        for (uint16_t i = 0; i < count; i++)
            json_object_array_add(arr, json_object_new_int(tmp[i]));
    } else if (!strcmp(type, "discrete") || !strcmp(type, "discrete_input")) {
        uint8_t tmp[512];
        reg_map_read_discrete_block(start, count, tmp);
        for (uint16_t i = 0; i < count; i++)
            json_object_array_add(arr, json_object_new_int(tmp[i]));
    } else if (!strcmp(type, "holding")) {
        uint16_t tmp[512];
        reg_map_read_holding_block(start, count, tmp);
        for (uint16_t i = 0; i < count; i++)
            json_object_array_add(arr, json_object_new_int(tmp[i]));
    } else if (!strcmp(type, "input_register") || !strcmp(type, "input")) {
        uint16_t tmp[512];
        reg_map_read_input_reg_block(start, count, tmp);
        for (uint16_t i = 0; i < count; i++)
            json_object_array_add(arr, json_object_new_int(tmp[i]));
    }

    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "type", json_object_new_string(type));
    json_object_object_add(o, "start", json_object_new_int(start));
    json_object_object_add(o, "values", arr);
    reply_json_obj(conn, 200, o);
    return 200;
}

static int handle_registers_post(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    long long cl = ri->content_length;
    if (cl <= 0 || cl > 65536) {
        reply_json_error(conn, 400, "bad content length");
        return 400;
    }

    char *buf = (char *)malloc((size_t)cl + 1);
    if (!buf) {
        reply_json_error(conn, 500, "alloc");
        return 500;
    }

    long long got = 0;
    while (got < cl) {
        int n = mg_read(conn, buf + got, (size_t)(cl - got));
        if (n <= 0)
            break;
        got += n;
    }
    buf[got] = 0;

    json_object *root = json_tokener_parse(buf);
    free(buf);
    if (!root) {
        reply_json_error(conn, 400, "invalid json");
        return 400;
    }

    json_object *hold = NULL;
    json_object *coils = NULL;
    if (json_object_object_get_ex(root, "holding", &hold) && json_object_is_type(hold, json_type_array)) {
        int n = json_object_array_length(hold);
        uint16_t tmp[512];
        uint16_t m = (uint16_t)(n > 512 ? 512 : n);
        for (uint16_t i = 0; i < m; i++) {
            json_object *el = json_object_array_get_idx(hold, i);
            tmp[i] = (uint16_t)json_object_get_int(el);
        }
        reg_map_write_holding_block(0, tmp, m);
    }
    if (json_object_object_get_ex(root, "coils", &coils) && json_object_is_type(coils, json_type_array)) {
        int n = json_object_array_length(coils);
        uint8_t tmp[512];
        uint16_t m = (uint16_t)(n > 512 ? 512 : n);
        for (uint16_t i = 0; i < m; i++) {
            json_object *el = json_object_array_get_idx(coils, i);
            tmp[i] = json_object_get_int(el) ? 1u : 0u;
        }
        reg_map_write_coils_block(0, tmp, m);
    }

    json_object_put(root);
    send_empty(conn, 204);
    return 204;
}

static int handle_io_get(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    if (strcmp(mg_get_request_info(conn)->request_method, "GET") != 0) {
        reply_json_error(conn, 405, "method not allowed");
        return 405;
    }

    struct json_object *di = json_object_new_array();
    struct json_object *co = json_object_new_array();
    for (int i = 0; i < 4; i++) {
        json_object_array_add(di, json_object_new_int(reg_get_discrete_input((uint16_t)i)));
        json_object_array_add(co, json_object_new_int(reg_get_coil((uint16_t)i)));
    }
    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "di", di);
    json_object_object_add(o, "do", co);
    reply_json_obj(conn, 200, o);
    return 200;
}

static int handle_io_do_post(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    long long cl = mg_get_request_info(conn)->content_length;
    if (cl <= 0 || cl > 4096) {
        reply_json_error(conn, 400, "bad body");
        return 400;
    }
    char *buf = (char *)malloc((size_t)cl + 1);
    if (!buf)
        return 500;
    long long got = 0;
    while (got < cl) {
        int n = mg_read(conn, buf + got, (size_t)(cl - got));
        if (n <= 0)
            break;
        got += n;
    }
    buf[got] = 0;
    json_object *root = json_tokener_parse(buf);
    free(buf);
    if (!root) {
        reply_json_error(conn, 400, "invalid json");
        return 400;
    }
    json_object *idxo = NULL;
    json_object *valo = NULL;
    if (json_object_object_get_ex(root, "index", &idxo) && json_object_object_get_ex(root, "value", &valo)) {
        int idx = json_object_get_int(idxo);
        int val = json_object_get_int(valo);
        if (idx >= 0 && idx < 4)
            reg_set_coil((uint16_t)idx, val ? 1u : 0u);
    }
    json_object_put(root);
    send_empty(conn, 204);
    return 204;
}

static int handle_project_upload(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    if (strcmp(mg_get_request_info(conn)->request_method, "POST") != 0) {
        reply_json_error(conn, 405, "method not allowed");
        return 405;
    }

    if (miniplc_save_post_body(conn, MINIPLC_UPLOAD_ZIP) != 0) {
        reply_json_error(conn, 400, "upload failed");
        return 400;
    }
    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "ok", json_object_new_boolean(1));
    reply_json_obj(conn, 200, o);
    return 200;
}

static int handle_project_apply(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    int r = project_store_apply_zip();
    if (r != 0) {
        reply_json_error(conn, 400, "apply failed");
        return 400;
    }
    plc_engine_logic_start();
    (void)system("systemctl try-restart miniplc-hmi.service 2>/dev/null");
    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "ok", json_object_new_boolean(1));
    reply_json_obj(conn, 200, o);
    return 200;
}

static int handle_config(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (!strcmp(ri->request_method, "GET"))
        return handle_config_get(conn, cbdata);
    if (!strcmp(ri->request_method, "POST"))
        return handle_config_post(conn, cbdata);
    reply_json_error(conn, 405, "method not allowed");
    return 405;
}

static int handle_config_get(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    struct json_object *root = json_object_new_object();
    json_object *gpio = json_object_new_object();
    json_object_object_add(gpio, "chip", json_object_new_string(s_cfg->gpio_chip));
    json_object *di = json_object_new_array();
    json_object *dol = json_object_new_array();
    for (int i = 0; i < MINIPLC_GPIO_CHANNELS; i++) {
        json_object_array_add(di, json_object_new_int(s_cfg->di_line[i]));
        json_object_array_add(dol, json_object_new_int(s_cfg->do_line[i]));
    }
    json_object_object_add(gpio, "di_lines", di);
    json_object_object_add(gpio, "do_lines", dol);
    json_object_object_add(root, "gpio", gpio);
    json_object_object_add(root, "scan_ms", json_object_new_int((int32_t)s_cfg->scan_ms));
    reply_json_obj(conn, 200, root);
    return 200;
}

static int handle_config_post(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    long long cl = mg_get_request_info(conn)->content_length;
    if (cl <= 0 || cl > 65536) {
        reply_json_error(conn, 400, "bad content length");
        return 400;
    }
    char *buf = (char *)malloc((size_t)cl + 1);
    if (!buf) {
        reply_json_error(conn, 500, "alloc");
        return 500;
    }
    long long got = 0;
    while (got < cl) {
        int n = mg_read(conn, buf + got, (size_t)(cl - got));
        if (n <= 0)
            break;
        got += n;
    }
    buf[got] = 0;
    json_object *root = json_tokener_parse(buf);
    free(buf);
    if (!root) {
        reply_json_error(conn, 400, "invalid json");
        return 400;
    }

    json_object *scan = NULL;
    if (json_object_object_get_ex(root, "scan_ms", &scan) && json_object_is_type(scan, json_type_int)) {
        uint32_t sm = (uint32_t)json_object_get_int(scan);
        if (sm >= 1u && sm <= 100u) {
            s_cfg->scan_ms = sm;
            plc_engine_set_scan_ms(sm);
        }
    }

    json_object *gpio = NULL;
    if (json_object_object_get_ex(root, "gpio", &gpio) && json_object_is_type(gpio, json_type_object)) {
        json_object *chip = NULL;
        if (json_object_object_get_ex(gpio, "chip", &chip) && json_object_is_type(chip, json_type_string))
            snprintf(s_cfg->gpio_chip, sizeof(s_cfg->gpio_chip), "%s", json_object_get_string(chip));

        json_object *di = NULL;
        json_object *dol = NULL;
        if (json_object_object_get_ex(gpio, "di_lines", &di) && json_object_is_type(di, json_type_array)) {
            int n = json_object_array_length(di);
            for (int i = 0; i < MINIPLC_GPIO_CHANNELS && i < n; i++)
                s_cfg->di_line[i] = json_object_get_int(json_object_array_get_idx(di, i));
        }
        if (json_object_object_get_ex(gpio, "do_lines", &dol) && json_object_is_type(dol, json_type_array)) {
            int n = json_object_array_length(dol);
            for (int i = 0; i < MINIPLC_GPIO_CHANNELS && i < n; i++)
                s_cfg->do_line[i] = json_object_get_int(json_object_array_get_idx(dol, i));
        }
        gpio_gpiod_shutdown();
        gpio_gpiod_init(s_cfg);
    }

    mkdir("/etc/plc", 0755);
    device_config_save_file(MINIPLC_CONFIG_FILE, s_cfg);
    json_object_put(root);

    send_empty(conn, 204);
    return 204;
}

static int handle_plugins(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    struct json_object *arr = json_object_new_array();
    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "plugins", arr);
    reply_json_obj(conn, 200, o);
    return 200;
}

static int handle_plugin_status(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "id", json_object_new_string("none"));
    json_object_object_add(o, "state", json_object_new_string("not_implemented"));
    reply_json_obj(conn, 200, o);
    return 200;
}

static int handle_system_time(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (!strcmp(ri->request_method, "GET"))
        return handle_system_time_get(conn, cbdata);
    if (!strcmp(ri->request_method, "POST"))
        return handle_system_time_post(conn, cbdata);
    reply_json_error(conn, 405, "method not allowed");
    return 405;
}

static int handle_plugins_tree(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    const char *uri = ri->local_uri ? ri->local_uri : "";
    if (!strcmp(uri, "/api/plugins"))
        return handle_plugins(conn, cbdata);
    if (strstr(uri, "/status") != NULL)
        return handle_plugin_status(conn, cbdata);
    reply_json_error(conn, 404, "not found");
    return 404;
}

static int handle_system_version(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;

    char pretty[256] = "unknown";
    char ver[128] = "";
    char line[512];
    FILE *f = fopen("/etc/os-release", "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                sscanf(line, "PRETTY_NAME=\"%255[^\"]\"", pretty);
            }
            if (strncmp(line, "VERSION_ID=", 11) == 0) {
                sscanf(line, "VERSION_ID=\"%127[^\"]\"", ver);
            }
        }
        fclose(f);
    }

    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "firmware", json_object_new_string(MINIPLC_FW_VERSION));
    json_object_object_add(o, "os", json_object_new_string(pretty));
    json_object_object_add(o, "version_id", json_object_new_string(ver));
    reply_json_obj(conn, 200, o);
    return 200;
}

static int handle_system_time_get(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    gmtime_r(&ts.tv_sec, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);

    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "utc", json_object_new_string(buf));
    reply_json_obj(conn, 200, o);
    return 200;
}

static int handle_system_time_post(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    long long cl = mg_get_request_info(conn)->content_length;
    if (cl <= 0 || cl > 512)
        return 400;
    char *buf = (char *)malloc((size_t)cl + 1);
    long long got = 0;
    while (got < cl) {
        int n = mg_read(conn, buf + got, (size_t)(cl - got));
        if (n <= 0)
            break;
        got += n;
    }
    buf[got] = 0;
    json_object *root = json_tokener_parse(buf);
    free(buf);
    if (!root)
        return 400;
    json_object *utc = NULL;
    if (json_object_object_get_ex(root, "utc", &utc) && json_object_is_type(utc, json_type_string)) {
        /* Minimal: parse ISO date — optional on embedded */
        (void)json_object_get_string(utc);
    }
    json_object_put(root);
    send_empty(conn, 204);
    return 204;
}

static int handle_update_status(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "state", json_object_new_string("idle"));
    reply_json_obj(conn, 200, o);
    return 200;
}

static int handle_hmi_status(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;

    struct json_object *mf = json_object_from_file(MINIPLC_PROJECT_MANIFEST);
    int screens = 0;
    if (mf) {
        json_object *sc = NULL;
        if (json_object_object_get_ex(mf, "screens", &sc) && json_object_is_type(sc, json_type_array))
            screens = json_object_array_length(sc);
        json_object_put(mf);
    }

    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "manifest_loaded", json_object_new_boolean(access(MINIPLC_PROJECT_MANIFEST, R_OK) == 0));
    json_object_object_add(o, "screens", json_object_new_int(screens));
    reply_json_obj(conn, 200, o);
    return 200;
}

static int handle_hmi_reload(struct mg_connection *conn, void *cbdata)
{
    (void)cbdata;
    if (handle_options(conn))
        return 204;
    (void)system("systemctl try-restart miniplc-hmi.service 2>/dev/null");
    send_empty(conn, 204);
    return 204;
}

struct mg_context *rest_api_start(DeviceConfig *cfg)
{
    s_cfg = cfg;

    const char *opts[] = {
        "listening_ports",
        MINIPLC_HTTP_PORT,
        "num_threads",
        "4",
        "request_size_limit_bytes",
        "33554432",
        NULL,
    };

    struct mg_context *ctx = mg_start(NULL, NULL, opts);
    if (!ctx)
        return NULL;

    mg_set_request_handler(ctx, "/api/status", handle_status, NULL);
    mg_set_request_handler(ctx, "/api/program/start", handle_program_start, NULL);
    mg_set_request_handler(ctx, "/api/program/stop", handle_program_stop, NULL);
    mg_set_request_handler(ctx, "/api/registers", handle_registers, NULL);
    mg_set_request_handler(ctx, "/api/io", handle_io_get, NULL);
    mg_set_request_handler(ctx, "/api/io/do", handle_io_do_post, NULL);
    mg_set_request_handler(ctx, "/api/project/upload", handle_project_upload, NULL);
    mg_set_request_handler(ctx, "/api/project/apply", handle_project_apply, NULL);
    mg_set_request_handler(ctx, "/api/config", handle_config, NULL);
    mg_set_request_handler(ctx, "/api/plugins", handle_plugins_tree, NULL);
    mg_set_request_handler(ctx, "/api/system/version", handle_system_version, NULL);
    mg_set_request_handler(ctx, "/api/system/time", handle_system_time, NULL);
    mg_set_request_handler(ctx, "/api/system/update/status", handle_update_status, NULL);
    mg_set_request_handler(ctx, "/api/hmi/status", handle_hmi_status, NULL);
    mg_set_request_handler(ctx, "/api/hmi/reload", handle_hmi_reload, NULL);

    return ctx;
}

void rest_api_stop(struct mg_context *ctx)
{
    if (ctx)
        mg_stop(ctx);
}
