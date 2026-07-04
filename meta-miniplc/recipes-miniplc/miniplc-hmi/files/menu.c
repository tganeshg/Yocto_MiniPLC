/**
 * @file menu.c
 *
 * MDCU HMI — Template A "Tile Dashboard"
 *   - Dark industrial theme
 *   - 60 px top header (logo + product title + clock)
 *   - Horizontal tab bar via lv_tabview (LV_DIR_TOP, 50 px)
 *   - Each tab hosts a 3x2 grid of tiles (title + LED + big value + subtitle)
 */

#include "menu.h"
#include <lvgl/lvgl_private.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mdcu_pool.h>
#include <mdcu_regmap.h>

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN && LV_MEM_SIZE < (38ul * 1024ul)
    #error Insufficient memory for lv_demo_widgets. Please set LV_MEM_SIZE to at least 38KB (38ul * 1024ul).
#endif

/*********************
 *      DEFINES
 *********************/
#define HEADER_H          60
#define TAB_BAR_H         50

#define THEME_FILE        "/tmp/theme.txt"
#define NETCONF_FILE      "/etc/mdcu/network.conf"
#define NETCONF_APPLY_CMD "/usr/bin/mdcu-net-apply &"

/* Palette colors — runtime-switchable between dark and light themes.
 * Assigned by theme_apply().  Initial values are dark (set in menu_create). */
static lv_color_t COL_BG_SCREEN;
static lv_color_t COL_BG_HEADER;
static lv_color_t COL_BG_TABBAR;
static lv_color_t COL_BG_TAB_ACT;
static lv_color_t COL_TILE_BG;
static lv_color_t COL_TILE_BORDER;
static lv_color_t COL_TILE_PRESSED;
static lv_color_t COL_ACCENT;
static lv_color_t COL_TEXT;
static lv_color_t COL_TEXT_MUTED;
static lv_color_t COL_LED_GREEN;
static lv_color_t COL_LED_YELLOW;
static lv_color_t COL_LED_RED;
static lv_color_t COL_LED_OFF;

static bool theme_dark = true;

/* Tile geometry — 3 cols x 2 rows fitting 800x ~360 */
#define TILE_W            252
#define TILE_H            160
#define TILE_GAP          8
#define TILE_PAD          8

#define MODBUS_POLL_INTERVAL_MS 1000

/* Device identity — placeholders until wired to firmware/API. */
#define DEV_MODEL   "FXD-iX-R1"
#define DEV_FW      "v0.4.1"
#define DEV_UI      "v0.1.0"
#define DEV_SERIAL  "FX24-000137"

/* Memory gauge accent (violet) — matches the web Overview. */
#define COL_MEM_HEX 0x7a63e0

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t *root;
    lv_obj_t *title;
    lv_obj_t *value;
    lv_obj_t *subtitle;
    lv_obj_t *led;
} tile_t;

/* Overview arc-ring gauge (CPU load / temp / memory). */
typedef struct {
    lv_obj_t *arc;
    lv_obj_t *value;   /* big number in the ring centre */
    lv_obj_t *unit;    /* small unit under the number   */
} gauge_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static const lv_font_t * font_large;
static const lv_font_t * font_normal;

static lv_style_t style_tile;
static lv_style_t style_tile_title;
static lv_style_t style_tile_value;
static lv_style_t style_tile_subtitle;
static lv_style_t style_screen;

static MAIN_MENU mMenu[mm_count] = {
    { mm_overview_id,  "Overview"  },
    { mm_plc_id,       "PLC"       },
    { mm_protocols_id, "Modbus"    },
    { mm_dlms_id,      "DLMS"      },
    { mm_iot_id,       "IoT"       },
    { mm_settings_id,  "Settings"  },
    { mm_about_id,     "About"     }
};

static MAIN_MENU_INST mmInst;
static lv_obj_t *rtc_date_lbl  = NULL;
static lv_obj_t *tabview       = NULL;
static lv_obj_t *tab_pages[mm_count];

/* Overview — device card + arc gauges + network/protocol strip */
static gauge_t   ov_g_load, ov_g_temp, ov_g_mem;
static lv_obj_t *ov_dev_vals = NULL;   /* device card values (incl. live uptime) */
static lv_obj_t *ov_net_vals = NULL;   /* network 4-row values label */
static lv_obj_t *ov_net_led  = NULL;   /* LAN link status LED */

/* PLC tiles */
static tile_t plc_state, plc_cycle, plc_inputs, plc_outputs, plc_program, plc_runtime;

/* Protocols (Modbus) tiles */
static tile_t pr_status, pr_target, pr_success, pr_errors, pr_regs, pr_settings;

/* DLMS tiles */
static tile_t dl_status, dl_meter, dl_obis, dl_lastread, dl_errors, dl_settings;

/* IoT tiles */
static tile_t iot_mqtt, iot_cloud, iot_msgs, iot_lastup, iot_topic, iot_cfg;

/* Settings tiles */
static tile_t st_network, st_time, st_display, st_logging, st_update, st_reboot;

/* About tiles */
static tile_t ab_version, ab_build, ab_kernel, ab_hw, ab_license, ab_credits;

/* Network config modal */
static lv_obj_t *netcfg_modal      = NULL;
static lv_obj_t *netcfg_mode_dd    = NULL;   /* DHCP / Static */
static lv_obj_t *netcfg_iface_ta   = NULL;
static lv_obj_t *netcfg_ip_ta      = NULL;
static lv_obj_t *netcfg_mask_ta    = NULL;
static lv_obj_t *netcfg_gw_ta      = NULL;
static lv_obj_t *netcfg_dns1_ta    = NULL;
static lv_obj_t *netcfg_dns2_ta    = NULL;
static lv_obj_t *netcfg_status_lbl = NULL;
static lv_obj_t *netcfg_keyboard   = NULL;

static uint32_t modbus_last_poll_time = 0;

/* RTC scratch */
time_t rawtime;
struct tm *timeinfo = NULL;

/**********************
 *   STATIC PROTOTYPES
 **********************/
static void theme_apply(bool dark);
static void theme_save(bool dark);
static bool theme_load(void);
static void ui_build(void);
static void ui_rebuild(void);
static void styles_init(void);
static void header_create(lv_obj_t *parent);
static tile_t create_tile(lv_obj_t *parent, int32_t col, int32_t row,
                          const char *title, const char *value, const char *subtitle);
static void set_tile_value(tile_t *t, const char *txt);
static void set_tile_subtitle(tile_t *t, const char *txt);
static void set_tile_led(tile_t *t, lv_color_t c);

static void overview_create(lv_obj_t *parent);
static void plc_create(lv_obj_t *parent);
static void protocols_create(lv_obj_t *parent);
static void dlms_create(lv_obj_t *parent);
static void iot_create(lv_obj_t *parent);
static void settings_create(lv_obj_t *parent);
static void about_create(lv_obj_t *parent);

static void overview_update(void);
static void protocols_update(void);

static void tabview_event_cb(lv_event_t *e);
static void theme_toggle_event_cb(lv_event_t *e);

static void netcfg_tile_event_cb(lv_event_t *e);
static void netcfg_save_event_cb(lv_event_t *e);
static void netcfg_close_event_cb(lv_event_t *e);
static void netcfg_ta_event_cb(lv_event_t *e);
static void netcfg_mode_changed_cb(lv_event_t *e);

static int read_cpu_temperature_c(float *temp_c);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static int read_cpu_temperature_c(float *temp_c)
{
    FILE *f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (f == NULL) return -1;
    long milli_c = 0;
    if (fscanf(f, "%ld", &milli_c) != 1) { fclose(f); return -1; }
    fclose(f);
    *temp_c = (float)milli_c / 1000.0f;
    return 0;
}

/* Parse the IPv4 default gateway from /proc/net/route.
 * The Gateway field is stored as a little-endian hex string. */
static int read_default_gateway(char *out, size_t len)
{
    FILE *f = fopen("/proc/net/route", "r");
    if (!f) return -1;
    char line[256];
    /* skip header */
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    while (fgets(line, sizeof(line), f)) {
        char iface[32];
        unsigned long dest = 0, gw = 0, flags = 0;
        if (sscanf(line, "%31s %lx %lx %lx", iface, &dest, &gw, &flags) >= 4) {
            if (dest == 0 && (flags & 0x2)) {     /* RTF_GATEWAY */
                snprintf(out, len, "%lu.%lu.%lu.%lu",
                         (gw >>  0) & 0xff,
                         (gw >>  8) & 0xff,
                         (gw >> 16) & 0xff,
                         (gw >> 24) & 0xff);
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return -1;
}

/* Find first global IPv4 address (skip loopback / link-local).
 * Also returns the interface name in `iface_out` (may be NULL). */
static int read_primary_ipv4(char *ip_out, size_t ip_len,
                             char *iface_out, size_t iface_len)
{
    struct ifaddrs *ifa = NULL, *p;
    if (getifaddrs(&ifa) != 0) return -1;
    int ret = -1;
    for (p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        if ((p->ifa_flags & 0x1 /* IFF_UP */) == 0) continue;
        struct sockaddr_in *sin = (struct sockaddr_in *)p->ifa_addr;
        uint32_t a = ntohl(sin->sin_addr.s_addr);
        if ((a >> 24) == 127) continue;          /* loopback 127/8 */
        if ((a & 0xFFFF0000) == 0xA9FE0000) continue; /* link-local 169.254/16 */
        inet_ntop(AF_INET, &sin->sin_addr, ip_out, ip_len);
        if (iface_out && iface_len && p->ifa_name)
            snprintf(iface_out, iface_len, "%s", p->ifa_name);
        ret = 0;
        break;
    }
    freeifaddrs(ifa);
    return ret;
}

/* operstate from /sys/class/net/<iface>/operstate: "up", "down", "unknown" */
static int read_link_state(const char *iface, char *out, size_t len)
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", iface);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(out, len, f)) { fclose(f); return -1; }
    fclose(f);
    out[strcspn(out, "\n")] = 0;
    /* uppercase the first character so "up" → "UP" reads as a status */
    for (char *c = out; *c; c++) {
        if (*c >= 'a' && *c <= 'z') *c -= 32;
    }
    return 0;
}

/* carrier (cable plugged in) from /sys/class/net/<iface>/carrier: 1/0 */
static int read_link_carrier(const char *iface)
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/carrier", iface);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int v = 0;
    if (fscanf(f, "%d", &v) != 1) { fclose(f); return -1; }
    fclose(f);
    return v;
}

/* First *useful* nameserver from /etc/resolv.conf.
 * Prefers a non-loopback entry; falls back to the first if all are loopback. */
static int read_dns_server(char *out, size_t len)
{
    FILE *f = fopen("/etc/resolv.conf", "r");
    if (!f) return -1;
    char line[256];
    char first[64] = {0};
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "nameserver", 10) != 0) continue;
        const char *p = line + 10;
        while (*p == ' ' || *p == '\t') p++;
        char ip[64] = {0};
        size_t i = 0;
        while (*p && *p != ' ' && *p != '\n' && i + 1 < sizeof(ip)) ip[i++] = *p++;
        ip[i] = 0;
        if (!ip[0]) continue;
        if (!first[0]) snprintf(first, sizeof(first), "%s", ip);
        /* prefer a non-loopback */
        if (strcmp(ip, "127.0.0.1") != 0 && strcmp(ip, "::1") != 0) {
            snprintf(out, len, "%s", ip);
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    if (first[0]) { snprintf(out, len, "%s", first); return 0; }
    return -1;
}

/*****************************
 *      THEME (dark/light)
 *****************************/
static void theme_apply(bool dark)
{
    theme_dark = dark;
    if (dark) {
        COL_BG_SCREEN    = lv_color_hex(0x12151a);
        COL_BG_HEADER    = lv_color_hex(0x1a1d23);
        COL_BG_TABBAR    = lv_color_hex(0x1a1d23);
        COL_BG_TAB_ACT   = lv_color_hex(0x252932);
        COL_TILE_BG      = lv_color_hex(0x222730);
        COL_TILE_BORDER  = lv_color_hex(0x2f3641);
        COL_TILE_PRESSED = lv_color_hex(0x2a3340);
        COL_ACCENT       = lv_color_hex(0x3b9eff);
        COL_TEXT         = lv_color_hex(0xe6e8ee);
        COL_TEXT_MUTED   = lv_color_hex(0x7a8190);
        COL_LED_OFF      = lv_color_hex(0x3a3f4a);
    } else {
        COL_BG_SCREEN    = lv_color_hex(0xeef1f5);
        COL_BG_HEADER    = lv_color_hex(0xffffff);
        COL_BG_TABBAR    = lv_color_hex(0xffffff);
        COL_BG_TAB_ACT   = lv_color_hex(0xe8f0fa);
        COL_TILE_BG      = lv_color_hex(0xffffff);
        COL_TILE_BORDER  = lv_color_hex(0xd8dde4);
        COL_TILE_PRESSED = lv_color_hex(0xdce6f2);
        COL_ACCENT       = lv_color_hex(0x1565c0);
        COL_TEXT         = lv_color_hex(0x1a1d23);
        COL_TEXT_MUTED   = lv_color_hex(0x6b7280);
        COL_LED_OFF      = lv_color_hex(0xc8cdd6);
    }
    /* status LEDs same on both themes */
    COL_LED_GREEN  = lv_color_hex(0x4caf50);
    COL_LED_YELLOW = lv_color_hex(0xffb74d);
    COL_LED_RED    = lv_color_hex(0xf44336);
}

static void theme_save(bool dark)
{
    FILE *f = fopen(THEME_FILE, "w");
    if (!f) return;
    fputs(dark ? "dark\n" : "light\n", f);
    fclose(f);
}

static bool theme_load(void)
{
    FILE *f = fopen(THEME_FILE, "r");
    if (!f) return true; /* default = dark */
    char buf[16] = {0};
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return true; }
    fclose(f);
    return strncmp(buf, "light", 5) != 0;
}

static void styles_init(void)
{
    /* Screen */
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, COL_BG_SCREEN);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_text_color(&style_screen, COL_TEXT);

    /* Tile container */
    lv_style_init(&style_tile);
    lv_style_set_bg_color(&style_tile, COL_TILE_BG);
    lv_style_set_bg_opa(&style_tile, LV_OPA_COVER);
    lv_style_set_border_color(&style_tile, COL_TILE_BORDER);
    lv_style_set_border_width(&style_tile, 1);
    lv_style_set_radius(&style_tile, 8);
    lv_style_set_pad_all(&style_tile, 10);
    lv_style_set_text_color(&style_tile, COL_TEXT);

    /* Tile title */
    lv_style_init(&style_tile_title);
    lv_style_set_text_color(&style_tile_title, COL_TEXT_MUTED);
    lv_style_set_text_font(&style_tile_title, font_normal);

    /* Tile big value */
    lv_style_init(&style_tile_value);
    lv_style_set_text_color(&style_tile_value, COL_ACCENT);
    lv_style_set_text_font(&style_tile_value, font_large);

    /* Tile subtitle */
    lv_style_init(&style_tile_subtitle);
    lv_style_set_text_color(&style_tile_subtitle, COL_TEXT_MUTED);
    lv_style_set_text_font(&style_tile_subtitle, font_normal);
}

static tile_t create_tile(lv_obj_t *parent, int32_t col, int32_t row,
                          const char *title, const char *value, const char *subtitle)
{
    tile_t t;
    int32_t x = TILE_PAD + col * (TILE_W + TILE_GAP);
    int32_t y = TILE_PAD + row * (TILE_H + TILE_GAP);

    t.root = lv_obj_create(parent);
    lv_obj_remove_style_all(t.root);
    lv_obj_add_style(t.root, &style_tile, 0);
    lv_obj_set_size(t.root, TILE_W, TILE_H);
    lv_obj_set_pos(t.root, x, y);
    lv_obj_clear_flag(t.root, LV_OBJ_FLAG_SCROLLABLE);

    /* Title (top-left) */
    t.title = lv_label_create(t.root);
    lv_obj_add_style(t.title, &style_tile_title, 0);
    lv_label_set_text(t.title, title ? title : "");
    lv_obj_align(t.title, LV_ALIGN_TOP_LEFT, 0, 0);

    /* LED (top-right) */
    t.led = lv_obj_create(t.root);
    lv_obj_remove_style_all(t.led);
    lv_obj_set_size(t.led, 12, 12);
    lv_obj_set_style_radius(t.led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(t.led, COL_LED_OFF, 0);
    lv_obj_set_style_bg_opa(t.led, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(t.led, 0, 0);
    lv_obj_align(t.led, LV_ALIGN_TOP_RIGHT, 0, 4);

    /* Big value (center) */
    t.value = lv_label_create(t.root);
    lv_obj_add_style(t.value, &style_tile_value, 0);
    lv_label_set_long_mode(t.value, LV_LABEL_LONG_DOT);
    lv_obj_set_width(t.value, TILE_W - 20);
    lv_label_set_text(t.value, value ? value : "--");
    lv_obj_align(t.value, LV_ALIGN_CENTER, 0, 0);

    /* Subtitle (bottom) */
    t.subtitle = lv_label_create(t.root);
    lv_obj_add_style(t.subtitle, &style_tile_subtitle, 0);
    lv_label_set_long_mode(t.subtitle, LV_LABEL_LONG_DOT);
    lv_obj_set_width(t.subtitle, TILE_W - 20);
    lv_label_set_text(t.subtitle, subtitle ? subtitle : "");
    lv_obj_align(t.subtitle, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    return t;
}

static void set_tile_value(tile_t *t, const char *txt)
{
    if (t && t->value && txt) lv_label_set_text(t->value, txt);
}

static void set_tile_subtitle(tile_t *t, const char *txt)
{
    if (t && t->subtitle && txt) lv_label_set_text(t->subtitle, txt);
}

static void set_tile_led(tile_t *t, lv_color_t c)
{
    if (t && t->led) lv_obj_set_style_bg_color(t->led, c, 0);
}

/*****************************
 *        HEADER BAR
 *****************************/
static void header_create(lv_obj_t *parent)
{
    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_size(hdr, LV_HOR_RES, HEADER_H);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, COL_BG_HEADER, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(hdr, COL_ACCENT, 0);
    lv_obj_set_style_border_width(hdr, 2, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    /* LVGL logo on the left */
    lv_obj_t *logo = lv_image_create(hdr);
    LV_IMAGE_DECLARE(img_lvgl_logo);
    lv_image_set_src(logo, &img_lvgl_logo);
    lv_obj_align(logo, LV_ALIGN_LEFT_MID, 12, 0);

    /* Product title — two-tone "FlexiDon iX" (Don = accent). */
    lv_obj_t *b1 = lv_label_create(hdr);
    lv_obj_set_style_text_font(b1, font_large, 0);
    lv_obj_set_style_text_color(b1, COL_TEXT, 0);
    lv_label_set_text(b1, "Flexi");
    lv_obj_align(b1, LV_ALIGN_LEFT_MID, 80, -10);

    lv_obj_t *b2 = lv_label_create(hdr);
    lv_obj_set_style_text_font(b2, font_large, 0);
    lv_obj_set_style_text_color(b2, COL_ACCENT, 0);
    lv_label_set_text(b2, "Don");
    lv_obj_align_to(b2, b1, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

    lv_obj_t *b3 = lv_label_create(hdr);
    lv_obj_set_style_text_font(b3, font_large, 0);
    lv_obj_set_style_text_color(b3, COL_TEXT, 0);
    lv_label_set_text(b3, " iX");
    lv_obj_align_to(b3, b2, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

    lv_obj_t *sub = lv_label_create(hdr);
    lv_obj_set_style_text_color(sub, COL_TEXT_MUTED, 0);
    lv_label_set_text(sub, "Flexible Data Collector & Controller");
    lv_obj_align(sub, LV_ALIGN_LEFT_MID, 80, 12);

    /* Clock on the right */
    rtc_date_lbl = lv_label_create(hdr);
    lv_obj_set_style_text_font(rtc_date_lbl, font_large, 0);
    lv_obj_set_style_text_color(rtc_date_lbl, COL_ACCENT, 0);
    lv_label_set_text(rtc_date_lbl, "00:00:00");
    lv_obj_align(rtc_date_lbl, LV_ALIGN_RIGHT_MID, -16, -10);

    lv_obj_t *date_sub = lv_label_create(hdr);
    lv_obj_set_style_text_color(date_sub, COL_TEXT_MUTED, 0);
    lv_label_set_text(date_sub, "--/--/----");
    lv_obj_align(date_sub, LV_ALIGN_RIGHT_MID, -16, 12);
    /* Stash so the loop can update both */
    lv_obj_set_user_data(rtc_date_lbl, date_sub);
}

/*****************************
 *         TABS
 *****************************/
/* ---- Overview widgets (arc gauges + cards) ------------------------- */
static gauge_t create_gauge(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t w, int32_t h, const char *label,
                            const char *unit, lv_color_t color)
{
    gauge_t g = { 0 };

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &style_tile, 0);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *arc = lv_arc_create(card);
    lv_obj_set_size(arc, 116, 116);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 4);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, COL_TILE_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);

    g.value = lv_label_create(card);
    lv_obj_set_style_text_font(g.value, font_large, 0);
    lv_obj_set_style_text_color(g.value, COL_TEXT, 0);
    lv_label_set_text(g.value, "--");
    lv_obj_align_to(g.value, arc, LV_ALIGN_CENTER, 0, -8);

    g.unit = lv_label_create(card);
    lv_obj_set_style_text_color(g.unit, COL_TEXT_MUTED, 0);
    lv_label_set_text(g.unit, unit ? unit : "");
    lv_obj_align_to(g.unit, arc, LV_ALIGN_CENTER, 0, 16);

    lv_obj_t *lbl = lv_label_create(card);
    lv_obj_set_style_text_color(lbl, COL_TEXT_MUTED, 0);
    lv_label_set_text(lbl, label ? label : "");
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, 0);

    g.arc = arc;
    return g;
}

static void set_gauge(gauge_t *g, int pct, const char *val, lv_color_t col)
{
    if (!g || !g->arc) return;
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    lv_arc_set_value(g->arc, pct);
    lv_obj_set_style_arc_color(g->arc, col, LV_PART_INDICATOR);
    if (g->value && val) {
        lv_label_set_text(g->value, val);
        lv_obj_align_to(g->value, g->arc, LV_ALIGN_CENTER, 0, -8);
    }
}

static lv_obj_t *create_card(lv_obj_t *parent, int32_t x, int32_t y,
                             int32_t w, int32_t h, const char *clabel)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &style_tile, 0);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    if (clabel) {
        lv_obj_t *l = lv_label_create(card);
        lv_obj_add_style(l, &style_tile_title, 0);
        lv_label_set_text(l, clabel);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 0);
    }
    return card;
}

static void create_proto_tile(lv_obj_t *parent, int32_t x, int32_t y,
                              int32_t w, int32_t h, const char *name,
                              const char *status, lv_color_t led)
{
    lv_obj_t *t = lv_obj_create(parent);
    lv_obj_remove_style_all(t);
    lv_obj_set_size(t, w, h);
    lv_obj_set_pos(t, x, y);
    lv_obj_set_style_bg_color(t, COL_BG_SCREEN, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(t, COL_TILE_BORDER, 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_radius(t, 7, 0);
    lv_obj_set_style_pad_all(t, 7, 0);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *nm = lv_label_create(t);
    lv_obj_set_style_text_color(nm, COL_TEXT, 0);
    lv_label_set_text(nm, name);
    lv_obj_align(nm, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *dot = lv_obj_create(t);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 9, 9);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, led, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_align(dot, LV_ALIGN_BOTTOM_LEFT, 0, -2);

    lv_obj_t *st = lv_label_create(t);
    lv_obj_set_style_text_color(st, COL_TEXT_MUTED, 0);
    lv_label_set_text(st, status);
    lv_obj_align(st, LV_ALIGN_BOTTOM_LEFT, 16, 0);
}

static void overview_create(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, COL_BG_SCREEN, 0);

    /* ---- Hero row: device identity card + 3 arc gauges ---- */
    lv_obj_t *dev = create_card(parent, 8, 8, 300, 194, NULL);

    lv_obj_t *ic = lv_obj_create(dev);
    lv_obj_remove_style_all(ic);
    lv_obj_set_size(ic, 36, 36);
    lv_obj_set_style_radius(ic, 9, 0);
    lv_obj_set_style_bg_color(ic, COL_ACCENT, 0);
    lv_obj_set_style_bg_opa(ic, LV_OPA_COVER, 0);
    lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *icl = lv_label_create(ic);
    lv_obj_set_style_text_color(icl, lv_color_white(), 0);
    lv_label_set_text(icl, LV_SYMBOL_LIST);
    lv_obj_center(icl);

    lv_obj_t *dt = lv_label_create(dev);
    lv_obj_set_style_text_font(dt, font_large, 0);
    lv_obj_set_style_text_color(dt, COL_TEXT, 0);
    lv_label_set_text(dt, "Device");
    lv_obj_align(dt, LV_ALIGN_TOP_LEFT, 46, -2);

    lv_obj_t *dsub = lv_label_create(dev);
    lv_obj_set_style_text_color(dsub, COL_TEXT_MUTED, 0);
    lv_label_set_text(dsub, "FlexiDon iX");
    lv_obj_align(dsub, LV_ALIGN_TOP_LEFT, 46, 28);

    lv_obj_t *keys = lv_label_create(dev);
    lv_obj_set_style_text_color(keys, COL_TEXT_MUTED, 0);
    lv_obj_set_style_text_line_space(keys, 6, 0);
    lv_label_set_text(keys, "Model\nFirmware\nUI\nSerial\nUptime");
    lv_obj_align(keys, LV_ALIGN_TOP_LEFT, 0, 60);

    ov_dev_vals = lv_label_create(dev);
    lv_obj_set_style_text_color(ov_dev_vals, COL_TEXT, 0);
    lv_obj_set_style_text_line_space(ov_dev_vals, 6, 0);
    lv_obj_set_width(ov_dev_vals, 190);
    lv_obj_set_style_text_align(ov_dev_vals, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(ov_dev_vals,
                      DEV_MODEL "\n" DEV_FW "\n" DEV_UI "\n" DEV_SERIAL "\n--:--:--");
    lv_obj_align(ov_dev_vals, LV_ALIGN_TOP_RIGHT, 0, 60);

    ov_g_load = create_gauge(parent, 316, 8, 152, 194, "CPU LOAD", "LOAD", COL_ACCENT);
    ov_g_temp = create_gauge(parent, 476, 8, 152, 194, "CPU TEMP", "TEMP", COL_LED_GREEN);
    ov_g_mem  = create_gauge(parent, 636, 8, 156, 194, "MEMORY",   "RAM",
                             lv_color_hex(COL_MEM_HEX));

    /* ---- Bottom strip: network / uplink + protocol status ---- */
    lv_obj_t *net = create_card(parent, 8, 210, 384, 156, "NETWORK / UPLINK");
    ov_net_led = lv_obj_create(net);
    lv_obj_remove_style_all(ov_net_led);
    lv_obj_set_size(ov_net_led, 10, 10);
    lv_obj_set_style_radius(ov_net_led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ov_net_led, COL_LED_OFF, 0);
    lv_obj_set_style_bg_opa(ov_net_led, LV_OPA_COVER, 0);
    lv_obj_align(ov_net_led, LV_ALIGN_TOP_RIGHT, 0, 4);

    lv_obj_t *nkeys = lv_label_create(net);
    lv_obj_set_style_text_color(nkeys, COL_TEXT_MUTED, 0);
    lv_obj_set_style_text_line_space(nkeys, 8, 0);
    lv_label_set_text(nkeys, "LAN 1\nIP Address\nGateway\nDNS");
    lv_obj_align(nkeys, LV_ALIGN_TOP_LEFT, 0, 30);

    ov_net_vals = lv_label_create(net);
    lv_obj_set_style_text_color(ov_net_vals, COL_TEXT, 0);
    lv_obj_set_style_text_line_space(ov_net_vals, 8, 0);
    lv_obj_set_width(ov_net_vals, 200);
    lv_obj_set_style_text_align(ov_net_vals, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(ov_net_vals, LV_LABEL_LONG_CLIP);
    lv_label_set_text(ov_net_vals, "--\n--\n--\n--");
    lv_obj_align(ov_net_vals, LV_ALIGN_TOP_RIGHT, 0, 30);

    lv_obj_t *proto = create_card(parent, 400, 210, 392, 156, "PROTOCOL STATUS");
    create_proto_tile(proto,   0, 22, 180, 54, "Modbus TCP",  "Running",   COL_LED_GREEN);
    create_proto_tile(proto, 192, 22, 180, 54, "MQTT",        "Connected", COL_LED_GREEN);
    create_proto_tile(proto,   0, 82, 180, 54, "OPC UA",      "Idle",      COL_LED_YELLOW);
    create_proto_tile(proto, 192, 82, 180, 54, "PLC Runtime", "5 ms",      COL_LED_GREEN);
}

static void plc_create(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, COL_BG_SCREEN, 0);

    plc_state   = create_tile(parent, 0, 0, "PLC STATE",   "Stopped",   "ladder runtime");
    plc_cycle   = create_tile(parent, 1, 0, "CYCLE TIME", "-- ms",     "scan period");
    plc_inputs  = create_tile(parent, 2, 0, "INPUTS",     "0/16",      "digital in");
    plc_outputs = create_tile(parent, 0, 1, "OUTPUTS",    "0/16",      "digital out");
    plc_program = create_tile(parent, 1, 1, "PROGRAM",    "none",      "loaded project");
    plc_runtime = create_tile(parent, 2, 1, "RUNTIME",    "v0.1",      "plc-firmware");
    set_tile_led(&plc_state, COL_LED_RED);
}

static void protocols_create(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, COL_BG_SCREEN, 0);

    pr_status   = create_tile(parent, 0, 0, "STATUS",       "Disconnected", "Modbus TCP master");
    pr_target   = create_tile(parent, 1, 0, "TARGET",       "--",           "ip:port slave");
    pr_success  = create_tile(parent, 2, 0, "SUCCESS",      "0",            "successful reads");
    pr_errors   = create_tile(parent, 0, 1, "ERRORS",       "0",            "read failures");
    pr_regs     = create_tile(parent, 1, 1, "REGISTERS",    "--",           "first 4 values");

    /* Configuration is done via the web UI — show a hint tile here. */
    pr_settings = create_tile(parent, 2, 1, "WEB UI", "Configure", "via browser");
    set_tile_led(&pr_settings, COL_ACCENT);
}

static void dlms_create(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, COL_BG_SCREEN, 0);

    dl_status   = create_tile(parent, 0, 0, "DLMS STATUS",  "Coming soon", "IEC 62056");
    dl_meter    = create_tile(parent, 1, 0, "METER ID",     "--",          "logical device");
    dl_obis     = create_tile(parent, 2, 0, "OBIS",         "1.0.1.8.0",   "active energy");
    dl_lastread = create_tile(parent, 0, 1, "LAST READ",    "--",          "timestamp");
    dl_errors   = create_tile(parent, 1, 1, "ERRORS",       "0",           "comm failures");
    dl_settings = create_tile(parent, 2, 1, "WEB UI",       "Configure",   "via browser");
    set_tile_led(&dl_settings, COL_ACCENT);
}

static void iot_create(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, COL_BG_SCREEN, 0);

    iot_mqtt   = create_tile(parent, 0, 0, "MQTT",      "Disconnected", "broker link");
    iot_cloud  = create_tile(parent, 1, 0, "CLOUD",     "offline",      "telemetry");
    iot_msgs   = create_tile(parent, 2, 0, "MESSAGES",  "0",            "published");
    iot_lastup = create_tile(parent, 0, 1, "LAST UP",   "--",           "uplink");
    iot_topic  = create_tile(parent, 1, 1, "TOPIC",     "/mdcu/data",   "mqtt path");
    iot_cfg    = create_tile(parent, 2, 1, "WEB UI",    "Configure",    "via browser");
    set_tile_led(&iot_cfg, COL_ACCENT);
}

static void settings_create(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, COL_BG_SCREEN, 0);

    st_network = create_tile(parent, 0, 0, "NETWORK",  "configure", "tap to edit");
    /* Clickable: open the Network config modal */
    lv_obj_add_flag(st_network.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(st_network.root, COL_TILE_PRESSED, LV_STATE_PRESSED);
    lv_obj_add_event_cb(st_network.root, netcfg_tile_event_cb, LV_EVENT_CLICKED, NULL);
    set_tile_led(&st_network, COL_ACCENT);

    st_time    = create_tile(parent, 1, 0, "TIME",     "NTP",    "rtc sync");

    /* THEME tile — clickable to toggle dark/light */
    st_display = create_tile(parent, 2, 0, "THEME",
                             theme_dark ? "Dark" : "Light",
                             "tap to switch");
    lv_obj_add_flag(st_display.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(st_display.root, COL_TILE_PRESSED, LV_STATE_PRESSED);
    lv_obj_add_event_cb(st_display.root, theme_toggle_event_cb, LV_EVENT_CLICKED, NULL);
    set_tile_led(&st_display, COL_ACCENT);

    st_logging = create_tile(parent, 0, 1, "LOGGING",  "info",   "/var/log");
    st_update  = create_tile(parent, 1, 1, "UPDATE",   "v1.0.0", "firmware ota");
    st_reboot  = create_tile(parent, 2, 1, "REBOOT",   "Action", "system control");
    set_tile_led(&st_reboot, COL_LED_RED);
}

static void theme_rebuild_async_cb(void *arg)
{
    (void)arg;
    ui_rebuild();
}

static void theme_toggle_event_cb(lv_event_t *e)
{
    (void)e;
    bool new_dark = !theme_dark;
    fprintf(stderr, "[THEME] toggle -> %s\n", new_dark ? "dark" : "light");
    theme_apply(new_dark);
    theme_save(new_dark);
    /* Defer the rebuild so we don't delete the object that's still
     * inside the current event dispatch. */
    lv_async_call(theme_rebuild_async_cb, NULL);
}

static void about_create(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, COL_BG_SCREEN, 0);

    char ver[32];
    snprintf(ver, sizeof(ver), "%d.%d.%d", MDCU_MAJOR_VER, MDCU_MINOR_VER, MDCU_BUILD_VER);

    ab_version = create_tile(parent, 0, 0, "VERSION",  ver,       "miniplc-hmi");
    ab_build   = create_tile(parent, 1, 0, "BUILD",    __DATE__,  __TIME__);
    ab_kernel  = create_tile(parent, 2, 0, "KERNEL",   "Yocto",   "poky kirkstone");
    ab_hw      = create_tile(parent, 0, 1, "HARDWARE", "RPi B+",  "ARM1176JZF-S");
    ab_license = create_tile(parent, 1, 1, "LICENSE",  "MIT",     "open source");
    ab_credits = create_tile(parent, 2, 1, "CREDITS",  "LVGL 9",  "lvgl.io");
    set_tile_led(&ab_version, COL_LED_GREEN);
}

/*****************************
 *      TAB SWITCH EVENT
 *****************************/
static void tabview_event_cb(lv_event_t *e)
{
    lv_obj_t *tv = lv_event_get_target(e);
    uint32_t idx = lv_tabview_get_tab_active(tv);
    if (idx < mm_count) {
        mmInst.current_mmId = idx;
    }
}

/*****************************
 *      DYNAMIC UPDATES
 *****************************/
static void overview_update(void)
{
    char buf[64];

    /* ---- CPU temperature gauge ---- */
    float t = 0.0f;
    int   have_temp = (read_cpu_temperature_c(&t) == 0);
    if (have_temp) {
        int pct = (int)((t / 85.0f) * 100.0f);
        lv_color_t c = (t >= 75.0f) ? COL_LED_RED
                     : (t >= 60.0f) ? COL_LED_YELLOW
                                    : COL_LED_GREEN;
        snprintf(buf, sizeof(buf), "%d\xC2\xB0", (int)(t + 0.5f));
        set_gauge(&ov_g_temp, pct, buf, c);
    } else {
        set_gauge(&ov_g_temp, 0, "N/A", COL_LED_OFF);
    }

    /* Mirror system metrics into the SYS range of the pool so every other
     * consumer (REST API, future ladder code, web UI) reads the same values. */
    if (mdcu_pool_is_open()) {
        if (have_temp)
            mdcu_set_i32(MDCU_SYS_CPU_TEMP_MILLIC, (int32_t)(t * 1000.0f));
        mdcu_set_u32(MDCU_SYS_EPOCH, (uint32_t)time(NULL));
        /* Heartbeat counter — visible proof in xxd that the pool is live. */
        mdcu_set_u16(MDCU_SYS_HEARTBEAT,
                     (uint16_t)(mdcu_get_u16(MDCU_SYS_HEARTBEAT) + 1U));
    }

    /* ---- CPU load + memory gauges, device-card uptime ---- */
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        int ncpu = get_nprocs();
        if (ncpu < 1) ncpu = 1;
        float load1    = si.loads[0] / 65536.0f;      /* 1-min load average */
        int   load_pct = (int)((load1 / ncpu) * 100.0f);
        if (load_pct > 100) load_pct = 100;
        snprintf(buf, sizeof(buf), "%d%%", load_pct);
        set_gauge(&ov_g_load, load_pct, buf, COL_ACCENT);

        unsigned long long total = (unsigned long long)si.totalram * si.mem_unit;
        unsigned long long freeb = (unsigned long long)si.freeram  * si.mem_unit;
        int mem_pct = total ? (int)(((total - freeb) * 100ULL) / total) : 0;
        snprintf(buf, sizeof(buf), "%d%%", mem_pct);
        set_gauge(&ov_g_mem, mem_pct, buf, lv_color_hex(COL_MEM_HEX));

        unsigned long up = si.uptime;
        snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
                 up / 3600, (up % 3600) / 60, up % 60);
        if (ov_dev_vals) {
            char v[160];
            snprintf(v, sizeof(v), "%s\n%s\n%s\n%s\n%s",
                     DEV_MODEL, DEV_FW, DEV_UI, DEV_SERIAL, buf);
            lv_label_set_text(ov_dev_vals, v);
        }

        if (mdcu_pool_is_open())
            mdcu_set_u32(MDCU_SYS_UPTIME_SEC, (uint32_t)si.uptime);
    }

    /* ---- Network / uplink — 4-row key/value list + status LED ----
     *     LAN 1        Up / Down
     *     IP Address   x.x.x.x
     *     Gateway      x.x.x.x
     *     DNS          x.x.x.x
     * LED: green = carrier up + has IP; yellow = carrier up but no IP;
     *      red = no carrier / link down. */
    {
        char ip[64]    = {0};
        char iface[32] = "eth0";   /* sensible default if no global IP yet */
        int has_ip = (read_primary_ipv4(ip, sizeof(ip), iface, sizeof(iface)) == 0) && ip[0];

        char state[16] = "?";
        read_link_state(iface, state, sizeof(state));
        int carrier = read_link_carrier(iface);   /* -1 unknown, 0 no, 1 yes */

        char gw[32]  = {0};
        char dns[64] = {0};
        read_default_gateway(gw,  sizeof(gw));
        read_dns_server     (dns, sizeof(dns));

        const char *eth_disp;
        if      (carrier == 1) eth_disp = "Up";
        else if (carrier == 0) eth_disp = "Down";
        else                   eth_disp = state;       /* fallback */

        if (ov_net_vals) {
            char vbuf[256];
            snprintf(vbuf, sizeof(vbuf), "%s\n%s\n%s\n%s",
                     eth_disp,
                     has_ip  ? ip  : "--",
                     gw[0]   ? gw  : "--",
                     dns[0]  ? dns : "--");
            lv_label_set_text(ov_net_vals, vbuf);
        }

        if (ov_net_led) {
            lv_color_t c = (has_ip && carrier != 0) ? COL_LED_GREEN
                         : (carrier == 0)           ? COL_LED_RED
                                                    : COL_LED_YELLOW;
            lv_obj_set_style_bg_color(ov_net_led, c, 0);
        }
    }
}

static void protocols_update(void)
{
    modbus_config_t cfg;
    if (mb_get_config(&cfg) != 0) return;

    /* Status */
    if (mb_is_connected() && cfg.enabled) {
        set_tile_value(&pr_status, "Connected");
        set_tile_led(&pr_status, COL_LED_GREEN);
    } else if (cfg.enabled) {
        set_tile_value(&pr_status, "Connecting");
        set_tile_led(&pr_status, COL_LED_YELLOW);
        mb_connect(&cfg);
    } else {
        set_tile_value(&pr_status, "Disabled");
        set_tile_led(&pr_status, COL_LED_OFF);
    }

    /* Target */
    char buf[80];
    snprintf(buf, sizeof(buf), "%s:%u", cfg.ip_address, cfg.port);
    set_tile_value(&pr_target, buf);
    snprintf(buf, sizeof(buf), "slave %u  regs %u@%u",
             cfg.slave_id, cfg.num_registers, cfg.start_address);
    set_tile_subtitle(&pr_target, buf);

    /* Poll registers */
    if (cfg.enabled && mb_is_connected()) {
        uint16_t regs[MODBUS_MAX_REGISTERS];
        if (mb_read_registers(cfg.start_address, cfg.num_registers, regs) == 0) {
            int n = cfg.num_registers > 4 ? 4 : cfg.num_registers;
            char tmp[80] = {0};
            for (int i = 0; i < n; i++) {
                char r[24];
                snprintf(r, sizeof(r), "%s%u", i ? " " : "", regs[i]);
                strncat(tmp, r, sizeof(tmp) - strlen(tmp) - 1);
            }
            set_tile_value(&pr_regs, tmp);
            snprintf(buf, sizeof(buf), "R%u..R%u",
                     cfg.start_address, cfg.start_address + n - 1);
            set_tile_subtitle(&pr_regs, buf);
            set_tile_led(&pr_regs, COL_LED_GREEN);
        } else {
            set_tile_led(&pr_regs, COL_LED_RED);
        }
    }

    /* Counters */
    modbus_data_t data;
    if (mb_get_data(&data) == 0) {
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)data.success_count);
        set_tile_value(&pr_success, buf);
        if (data.success_count > 0) set_tile_led(&pr_success, COL_LED_GREEN);

        snprintf(buf, sizeof(buf), "%lu", (unsigned long)data.error_count);
        set_tile_value(&pr_errors, buf);
        set_tile_led(&pr_errors,
                     data.error_count == 0 ? COL_LED_OFF :
                     (data.error_count < 10 ? COL_LED_YELLOW : COL_LED_RED));
    }
}

/*****************************
 *  NETWORK CONFIG MODAL
 *  (Settings tile NETWORK -> tap)
 *
 *  Reads/writes /etc/mdcu/network.conf (key=value), then triggers
 *  /usr/bin/mdcu-net-apply to regenerate /etc/network/interfaces and
 *  restart the configured interface.
 *****************************/

typedef struct {
    char mode[16];       /* "dhcp" or "static" */
    char iface[32];
    char address[64];
    char netmask[64];
    char gateway[64];
    char dns1[64];
    char dns2[64];
} netcfg_t;

/* Trim leading/trailing whitespace + newline IN PLACE.  Returns s. */
static char *netcfg_trim(char *s)
{
    if (!s) return s;
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                     s[n - 1] == ' '  || s[n - 1] == '\t')) {
        s[--n] = 0;
    }
    return s;
}

/* Restrict to chars allowed in /etc/mdcu/network.conf values.
 * The on-device apply script also sanitizes, this is a defence-in-depth. */
static void netcfg_sanitize(char *s)
{
    if (!s) return;
    char *w = s;
    for (char *r = s; *r; r++) {
        unsigned char c = (unsigned char)*r;
        if ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
             c == '.' || c == '-' || c == '_') {
            *w++ = (char)c;
        }
    }
    *w = 0;
}

static void netcfg_defaults(netcfg_t *c)
{
    memset(c, 0, sizeof(*c));
    snprintf(c->mode,  sizeof(c->mode),  "dhcp");
    snprintf(c->iface, sizeof(c->iface), "eth0");
}

static int netcfg_load(netcfg_t *c)
{
    netcfg_defaults(c);
    FILE *f = fopen(NETCONF_FILE, "r");
    if (!f) return -1;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = netcfg_trim(line);
        char *val = netcfg_trim(eq + 1);
        if (!*key || *key == '#') continue;
        if      (!strcmp(key, "mode"))    snprintf(c->mode,    sizeof(c->mode),    "%s", val);
        else if (!strcmp(key, "iface"))   snprintf(c->iface,   sizeof(c->iface),   "%s", val);
        else if (!strcmp(key, "address")) snprintf(c->address, sizeof(c->address), "%s", val);
        else if (!strcmp(key, "netmask")) snprintf(c->netmask, sizeof(c->netmask), "%s", val);
        else if (!strcmp(key, "gateway")) snprintf(c->gateway, sizeof(c->gateway), "%s", val);
        else if (!strcmp(key, "dns1"))    snprintf(c->dns1,    sizeof(c->dns1),    "%s", val);
        else if (!strcmp(key, "dns2"))    snprintf(c->dns2,    sizeof(c->dns2),    "%s", val);
    }
    fclose(f);
    return 0;
}

static int netcfg_save(const netcfg_t *c)
{
    /* Ensure /etc/mdcu exists (created by recipe, but safe to make sure) */
    int _mkrc = system("mkdir -p /etc/mdcu");
    (void)_mkrc;

    /* Atomic-ish write: tmp + rename */
    const char *tmp = NETCONF_FILE ".tmp";
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    fprintf(f, "# Managed by MiniHMI Settings -> NETWORK.\n");
    fprintf(f, "# Run /usr/bin/mdcu-net-apply to (re)apply.\n");
    fprintf(f, "mode=%s\n",    c->mode[0]    ? c->mode    : "dhcp");
    fprintf(f, "iface=%s\n",   c->iface[0]   ? c->iface   : "eth0");
    fprintf(f, "address=%s\n", c->address);
    fprintf(f, "netmask=%s\n", c->netmask);
    fprintf(f, "gateway=%s\n", c->gateway);
    fprintf(f, "dns1=%s\n",    c->dns1);
    fprintf(f, "dns2=%s\n",    c->dns2);
    fflush(f);
    fclose(f);
    if (rename(tmp, NETCONF_FILE) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

/* Show/hide the static-only fields based on the mode dropdown. */
static void netcfg_apply_mode_visibility(bool is_static)
{
    lv_obj_t *fields[] = {
        netcfg_ip_ta, netcfg_mask_ta, netcfg_gw_ta,
        netcfg_dns1_ta, netcfg_dns2_ta,
    };
    for (size_t i = 0; i < sizeof(fields)/sizeof(fields[0]); i++) {
        if (!fields[i]) continue;
        if (is_static) lv_obj_clear_flag(fields[i], LV_OBJ_FLAG_HIDDEN);
        else           lv_obj_add_flag  (fields[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void netcfg_mode_changed_cb(lv_event_t *e)
{
    (void)e;
    if (!netcfg_mode_dd) return;
    uint32_t idx = lv_dropdown_get_selected(netcfg_mode_dd);
    netcfg_apply_mode_visibility(idx == 1);   /* 0=DHCP, 1=Static */
}

static void netcfg_tile_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (netcfg_modal != NULL) return;          /* already open */

    netcfg_t cfg;
    netcfg_load(&cfg);

    netcfg_modal = lv_win_create(lv_screen_active());
    lv_obj_set_size(netcfg_modal, LV_HOR_RES - 40, LV_VER_RES - 100);
    lv_obj_align(netcfg_modal, LV_ALIGN_TOP_MID, 0, 60);

    lv_obj_t *header = lv_win_get_header(netcfg_modal);
    lv_obj_t *title  = lv_label_create(header);
    lv_label_set_text(title, "Network Configuration");

    lv_obj_t *content = lv_win_get_content(netcfg_modal);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_min_height(content, LV_VER_RES - 200, 0);

    lv_obj_t *lbl;

    /* Mode dropdown */
    lbl = lv_label_create(content); lv_label_set_text(lbl, "Mode:");
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, 10);
    netcfg_mode_dd = lv_dropdown_create(content);
    lv_dropdown_set_options_static(netcfg_mode_dd, "DHCP\nStatic");
    lv_obj_set_width(netcfg_mode_dd, 200);
    lv_obj_align(netcfg_mode_dd, LV_ALIGN_TOP_LEFT, 100, 5);
    lv_dropdown_set_selected(netcfg_mode_dd, (strcmp(cfg.mode, "static") == 0) ? 1 : 0);
    lv_obj_add_event_cb(netcfg_mode_dd, netcfg_mode_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Interface */
    lbl = lv_label_create(content); lv_label_set_text(lbl, "Interface:");
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, 55);
    netcfg_iface_ta = lv_textarea_create(content);
    lv_textarea_set_one_line(netcfg_iface_ta, true);
    lv_textarea_set_text(netcfg_iface_ta, cfg.iface);
    lv_textarea_set_accepted_chars(netcfg_iface_ta, "abcdefghijklmnopqrstuvwxyz0123456789");
    lv_obj_set_width(netcfg_iface_ta, LV_HOR_RES - 160);
    lv_obj_align(netcfg_iface_ta, LV_ALIGN_TOP_LEFT, 100, 50);
    lv_obj_add_event_cb(netcfg_iface_ta, netcfg_ta_event_cb, LV_EVENT_FOCUSED,   NULL);
    lv_obj_add_event_cb(netcfg_iface_ta, netcfg_ta_event_cb, LV_EVENT_DEFOCUSED, NULL);

    /* Helper to build the four "IP-like" rows */
    static const char *ip_chars = "0123456789.";

    /* IP Address */
    lbl = lv_label_create(content); lv_label_set_text(lbl, "IP Address:");
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, 100);
    netcfg_ip_ta = lv_textarea_create(content);
    lv_textarea_set_one_line(netcfg_ip_ta, true);
    lv_textarea_set_accepted_chars(netcfg_ip_ta, ip_chars);
    lv_textarea_set_text(netcfg_ip_ta, cfg.address);
    lv_obj_set_width(netcfg_ip_ta, LV_HOR_RES - 160);
    lv_obj_align(netcfg_ip_ta, LV_ALIGN_TOP_LEFT, 100, 95);
    lv_obj_add_event_cb(netcfg_ip_ta, netcfg_ta_event_cb, LV_EVENT_FOCUSED,   NULL);
    lv_obj_add_event_cb(netcfg_ip_ta, netcfg_ta_event_cb, LV_EVENT_DEFOCUSED, NULL);

    /* Netmask */
    lbl = lv_label_create(content); lv_label_set_text(lbl, "Netmask:");
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, 145);
    netcfg_mask_ta = lv_textarea_create(content);
    lv_textarea_set_one_line(netcfg_mask_ta, true);
    lv_textarea_set_accepted_chars(netcfg_mask_ta, ip_chars);
    lv_textarea_set_text(netcfg_mask_ta, cfg.netmask);
    lv_obj_set_width(netcfg_mask_ta, LV_HOR_RES - 160);
    lv_obj_align(netcfg_mask_ta, LV_ALIGN_TOP_LEFT, 100, 140);
    lv_obj_add_event_cb(netcfg_mask_ta, netcfg_ta_event_cb, LV_EVENT_FOCUSED,   NULL);
    lv_obj_add_event_cb(netcfg_mask_ta, netcfg_ta_event_cb, LV_EVENT_DEFOCUSED, NULL);

    /* Gateway */
    lbl = lv_label_create(content); lv_label_set_text(lbl, "Gateway:");
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, 190);
    netcfg_gw_ta = lv_textarea_create(content);
    lv_textarea_set_one_line(netcfg_gw_ta, true);
    lv_textarea_set_accepted_chars(netcfg_gw_ta, ip_chars);
    lv_textarea_set_text(netcfg_gw_ta, cfg.gateway);
    lv_obj_set_width(netcfg_gw_ta, LV_HOR_RES - 160);
    lv_obj_align(netcfg_gw_ta, LV_ALIGN_TOP_LEFT, 100, 185);
    lv_obj_add_event_cb(netcfg_gw_ta, netcfg_ta_event_cb, LV_EVENT_FOCUSED,   NULL);
    lv_obj_add_event_cb(netcfg_gw_ta, netcfg_ta_event_cb, LV_EVENT_DEFOCUSED, NULL);

    /* DNS 1 */
    lbl = lv_label_create(content); lv_label_set_text(lbl, "DNS 1:");
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, 235);
    netcfg_dns1_ta = lv_textarea_create(content);
    lv_textarea_set_one_line(netcfg_dns1_ta, true);
    lv_textarea_set_accepted_chars(netcfg_dns1_ta, ip_chars);
    lv_textarea_set_text(netcfg_dns1_ta, cfg.dns1);
    lv_obj_set_width(netcfg_dns1_ta, LV_HOR_RES - 160);
    lv_obj_align(netcfg_dns1_ta, LV_ALIGN_TOP_LEFT, 100, 230);
    lv_obj_add_event_cb(netcfg_dns1_ta, netcfg_ta_event_cb, LV_EVENT_FOCUSED,   NULL);
    lv_obj_add_event_cb(netcfg_dns1_ta, netcfg_ta_event_cb, LV_EVENT_DEFOCUSED, NULL);

    /* DNS 2 */
    lbl = lv_label_create(content); lv_label_set_text(lbl, "DNS 2:");
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, 280);
    netcfg_dns2_ta = lv_textarea_create(content);
    lv_textarea_set_one_line(netcfg_dns2_ta, true);
    lv_textarea_set_accepted_chars(netcfg_dns2_ta, ip_chars);
    lv_textarea_set_text(netcfg_dns2_ta, cfg.dns2);
    lv_obj_set_width(netcfg_dns2_ta, LV_HOR_RES - 160);
    lv_obj_align(netcfg_dns2_ta, LV_ALIGN_TOP_LEFT, 100, 275);
    lv_obj_add_event_cb(netcfg_dns2_ta, netcfg_ta_event_cb, LV_EVENT_FOCUSED,   NULL);
    lv_obj_add_event_cb(netcfg_dns2_ta, netcfg_ta_event_cb, LV_EVENT_DEFOCUSED, NULL);

    /* Status line (filled in after Save) */
    netcfg_status_lbl = lv_label_create(content);
    lv_label_set_text(netcfg_status_lbl, "");
    lv_obj_set_style_text_color(netcfg_status_lbl, COL_TEXT_MUTED, 0);
    lv_obj_align(netcfg_status_lbl, LV_ALIGN_TOP_LEFT, 10, 325);

    /* Save/Close */
    lv_obj_t *btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_HOR_RES - 80, 50);
    lv_obj_align(btn_cont, LV_ALIGN_TOP_LEFT, 10, 360);
    lv_obj_set_layout(btn_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *save_btn = lv_button_create(btn_cont);
    lv_obj_set_size(save_btn, 130, 36);
    lv_obj_t *save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, "Save & Apply");
    lv_obj_center(save_lbl);
    lv_obj_add_event_cb(save_btn, netcfg_save_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_btn = lv_button_create(btn_cont);
    lv_obj_set_size(close_btn, 100, 36);
    lv_obj_t *close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "Close");
    lv_obj_center(close_lbl);
    lv_obj_add_event_cb(close_btn, netcfg_close_event_cb, LV_EVENT_CLICKED, NULL);

    /* Keyboard for text fields */
    netcfg_keyboard = lv_keyboard_create(lv_screen_active());
    lv_obj_set_size(netcfg_keyboard, LV_HOR_RES, LV_VER_RES / 2);
    lv_obj_align(netcfg_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(netcfg_keyboard, LV_OBJ_FLAG_HIDDEN);

    /* Apply initial visibility based on the loaded mode */
    netcfg_apply_mode_visibility(strcmp(cfg.mode, "static") == 0);
}

static void netcfg_save_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!netcfg_mode_dd || !netcfg_iface_ta) return;

    netcfg_t cfg;
    netcfg_defaults(&cfg);

    uint32_t mode_idx = lv_dropdown_get_selected(netcfg_mode_dd);
    snprintf(cfg.mode, sizeof(cfg.mode), "%s", mode_idx == 1 ? "static" : "dhcp");

    const char *s;
    s = lv_textarea_get_text(netcfg_iface_ta); if (s) snprintf(cfg.iface,   sizeof(cfg.iface),   "%s", s);

    if (mode_idx == 1) {
        s = lv_textarea_get_text(netcfg_ip_ta);   if (s) snprintf(cfg.address, sizeof(cfg.address), "%s", s);
        s = lv_textarea_get_text(netcfg_mask_ta); if (s) snprintf(cfg.netmask, sizeof(cfg.netmask), "%s", s);
        s = lv_textarea_get_text(netcfg_gw_ta);   if (s) snprintf(cfg.gateway, sizeof(cfg.gateway), "%s", s);
        s = lv_textarea_get_text(netcfg_dns1_ta); if (s) snprintf(cfg.dns1,    sizeof(cfg.dns1),    "%s", s);
        s = lv_textarea_get_text(netcfg_dns2_ta); if (s) snprintf(cfg.dns2,    sizeof(cfg.dns2),    "%s", s);
    }

    /* Sanitize all string fields (apply script also sanitizes; this just
     * prevents writing junk into the conf file). */
    netcfg_sanitize(cfg.iface);
    netcfg_sanitize(cfg.address);
    netcfg_sanitize(cfg.netmask);
    netcfg_sanitize(cfg.gateway);
    netcfg_sanitize(cfg.dns1);
    netcfg_sanitize(cfg.dns2);
    if (!cfg.iface[0]) snprintf(cfg.iface, sizeof(cfg.iface), "eth0");

    fprintf(stderr, "[NETCFG] save: mode=%s iface=%s ip=%s mask=%s gw=%s dns1=%s dns2=%s\n",
            cfg.mode, cfg.iface, cfg.address, cfg.netmask, cfg.gateway, cfg.dns1, cfg.dns2);

    int rc = netcfg_save(&cfg);
    if (rc != 0) {
        if (netcfg_status_lbl) {
            lv_label_set_text(netcfg_status_lbl, "Failed to write /etc/mdcu/network.conf");
            lv_obj_set_style_text_color(netcfg_status_lbl, COL_LED_RED, 0);
        }
        return;
    }

    /* Kick off apply in the background — restarting the interface from a
     * blocking system() call here would freeze the HMI for a couple of
     * seconds.  The init script also runs this on every boot. */
    int sysrc = system(NETCONF_APPLY_CMD);
    (void)sysrc;

    if (netcfg_status_lbl) {
        lv_label_set_text(netcfg_status_lbl, "Saved. Applying...");
        lv_obj_set_style_text_color(netcfg_status_lbl, COL_LED_GREEN, 0);
    }

    /* Close after a tiny delay so the user sees the status flash */
    if (netcfg_keyboard) { lv_obj_del(netcfg_keyboard); netcfg_keyboard = NULL; }
    if (netcfg_modal)    { lv_obj_del(netcfg_modal);    netcfg_modal    = NULL;
                           netcfg_status_lbl = NULL; }
}

static void netcfg_close_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (netcfg_keyboard) { lv_obj_del(netcfg_keyboard); netcfg_keyboard = NULL; }
    if (netcfg_modal)    { lv_obj_del(netcfg_modal);    netcfg_modal    = NULL;
                           netcfg_status_lbl = NULL; }
}

static void netcfg_ta_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    if (!netcfg_keyboard) return;

    if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(netcfg_keyboard, ta);
        if (ta == netcfg_ip_ta || ta == netcfg_mask_ta || ta == netcfg_gw_ta ||
            ta == netcfg_dns1_ta || ta == netcfg_dns2_ta) {
            lv_keyboard_set_mode(netcfg_keyboard, LV_KEYBOARD_MODE_NUMBER);
        } else {
            lv_keyboard_set_mode(netcfg_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
        }
        lv_obj_clear_flag(netcfg_keyboard, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_add_flag(netcfg_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ********************
 *   GLOBAL FUNCTIONS
 ********************/

void menu_loop(void)
{
    uint32_t idle_time = 0;

    while (true) {
        time(&rawtime);
        timeinfo = localtime(&rawtime);

        /* Update header clock + date */
        if (rtc_date_lbl && timeinfo) {
            lv_label_set_text_fmt(rtc_date_lbl, "%02d:%02d:%02d",
                                  timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
            lv_obj_t *date_sub = (lv_obj_t *)lv_obj_get_user_data(rtc_date_lbl);
            if (date_sub) {
                lv_label_set_text_fmt(date_sub, "%02d/%02d/%04d",
                                      timeinfo->tm_mday,
                                      timeinfo->tm_mon + 1,
                                      timeinfo->tm_year + 1900);
            }
        }

        uint32_t now = (uint32_t)time(NULL);
        if ((now - modbus_last_poll_time) >= (MODBUS_POLL_INTERVAL_MS / 1000)) {
            switch (mmInst.current_mmId) {
                case mm_overview_id:  overview_update();  break;
                case mm_protocols_id: protocols_update(); break;
                default: break;
            }
            modbus_last_poll_time = now;
        }

        idle_time = lv_timer_handler();
        usleep(idle_time * 1000);
    }
}

static void ui_build(void)
{
#if LV_USE_THEME_DEFAULT
    lv_theme_default_init(NULL,
                          COL_ACCENT, lv_palette_main(LV_PALETTE_RED),
                          theme_dark, font_normal);
#endif

    /* Screen background */
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG_SCREEN, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, COL_TEXT, 0);
    lv_obj_set_style_text_font(scr, font_normal, 0);

    styles_init();

    /* Header bar */
    header_create(scr);

    /* Tab view fills the rest */
    tabview = lv_tabview_create(scr);
    lv_obj_set_size(tabview, LV_HOR_RES, LV_VER_RES - HEADER_H);
    lv_obj_set_pos(tabview, 0, HEADER_H);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview, TAB_BAR_H);

    /* Tab bar styling */
    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview);
    lv_obj_set_style_bg_color(tab_bar, COL_BG_TABBAR, 0);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(tab_bar, COL_TEXT_MUTED, 0);
    lv_obj_set_style_text_color(tab_bar, COL_ACCENT, LV_STATE_CHECKED);
    lv_obj_set_style_border_width(tab_bar, 0, 0);

    /* Content container styling */
    lv_obj_t *content = lv_tabview_get_content(tabview);
    lv_obj_set_style_bg_color(content, COL_BG_SCREEN, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);

    /* Create the tabs */
    for (uint32_t i = 0; i < mm_count; i++) {
        tab_pages[i] = lv_tabview_add_tab(tabview, mMenu[i].mmName);
    }
    overview_create (tab_pages[mm_overview_id]);
    plc_create      (tab_pages[mm_plc_id]);
    protocols_create(tab_pages[mm_protocols_id]);
    dlms_create     (tab_pages[mm_dlms_id]);
    iot_create      (tab_pages[mm_iot_id]);
    settings_create (tab_pages[mm_settings_id]);
    about_create    (tab_pages[mm_about_id]);

    /* Style each tab button */
    uint32_t n = lv_tabview_get_tab_count(tabview);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *btn = lv_tabview_get_tab_button(tabview, i);
        if (!btn) continue;
        lv_obj_set_style_bg_color(btn, COL_BG_TABBAR, 0);
        lv_obj_set_style_bg_color(btn, COL_BG_TAB_ACT, LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(btn, COL_TEXT_MUTED, 0);
        lv_obj_set_style_text_color(btn, COL_ACCENT, LV_STATE_CHECKED);
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM, LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, COL_ACCENT, LV_STATE_CHECKED);
        lv_obj_set_style_border_width(btn, 3, LV_STATE_CHECKED);
    }

    lv_obj_add_event_cb(tabview, tabview_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Restore the active tab after a rebuild */
    if (mmInst.current_mmId < mm_count) {
        lv_tabview_set_active(tabview, mmInst.current_mmId, LV_ANIM_OFF);
    }
}

static void ui_rebuild(void)
{
    /* Drop any open modal/keyboard references — they got deleted with the screen */
    netcfg_modal        = NULL;
    netcfg_keyboard     = NULL;
    netcfg_status_lbl   = NULL;
    rtc_date_lbl        = NULL;
    ov_dev_vals         = NULL;
    ov_net_vals         = NULL;
    ov_net_led          = NULL;

    lv_obj_clean(lv_screen_active());
    ui_build();
}

void menu_create(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);

    mb_init();
    {
        modbus_config_t cfg;
        if (mb_get_config(&cfg) == 0 && cfg.enabled) {
            fprintf(stderr, "[MODBUS] auto-connect -> %s:%u slave=%u\n",
                    cfg.ip_address, cfg.port, cfg.slave_id);
            if (mb_connect(&cfg) == 0) {
                uint16_t regs[MODBUS_MAX_REGISTERS];
                if (mb_read_registers(cfg.start_address, cfg.num_registers, regs) == 0) {
                    fprintf(stderr, "[MODBUS] self-test OK:");
                    for (uint16_t i = 0; i < cfg.num_registers && i < 10; i++)
                        fprintf(stderr, " R%u=%u", cfg.start_address + i, regs[i]);
                    fprintf(stderr, "\n");
                }
            }
        }
    }

#if LV_FONT_MONTSERRAT_24
    font_large  = &lv_font_montserrat_24;
#else
    font_large  = LV_FONT_DEFAULT;
#endif
#if LV_FONT_MONTSERRAT_16
    font_normal = &lv_font_montserrat_16;
#else
    font_normal = LV_FONT_DEFAULT;
#endif

    /* Load persisted theme (default dark) and seed the palette */
    theme_apply(theme_load());

    mmInst.current_mmId = mm_overview_id;
    ui_build();
}
/* EOF */
