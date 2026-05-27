/**
 * Local LVGL panel — PLC+HMI product (plan §12, HMI-07).
 * Prefers DRM/KMS on /dev/dri/card0 (Raspberry Pi), falls back to /dev/fb0.
 */
#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <linux/fb.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <lvgl.h>

#if LV_USE_LINUX_DRM
#include <xf86drm.h>
#include <xf86drmMode.h>
#endif

#if LV_USE_LINUX_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#endif
#if LV_USE_LINUX_FBDEV
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif

#define MANIFEST_PATH "/var/lib/plc/hmi/hmi_manifest.json"
#define FALLBACK_TEXT "No UI configured"
#define FB_DEVICE "/dev/fb0"
#define DRM_DEVICE "/dev/dri/card0"
#define LOG_PATH "/var/log/miniplc-hmi.log"

static FILE *log_fp;

static void log_line(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    if (log_fp)
        vfprintf(log_fp, fmt, ap);
    va_end(ap);
}

/** Kernel often boots panels with sysfs blanking on — screen stays black. */
static void unblank_all_fbs(void)
{
    char path[80];
    for (unsigned i = 0; i < 8; i++) {
        snprintf(path, sizeof(path), "/sys/class/graphics/fb%u/blank", i);
        FILE *w = fopen(path, "we");
        if (!w)
            continue;
        fputs("0\n", w);
        fclose(w);
    }
}

#if LV_USE_LINUX_DRM
/** True if a KMS connector with a mode is connected (matches LVGL drm backend). */
static bool drm_primary_available(void)
{
    int fd = open(DRM_DEVICE, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        log_line("miniplc-hmi: open %s: %s (using fbdev)\n", DRM_DEVICE,
                 strerror(errno));
        return false;
    }

    drmModeRes *res = drmModeGetResources(fd);
    if (!res) {
        log_line("miniplc-hmi: drmModeGetResources failed\n");
        close(fd);
        return false;
    }

    bool ok = false;
    for (int i = 0; i < res->count_connectors && !ok; i++) {
        drmModeConnector *conn =
            drmModeGetConnector(fd, res->connectors[i]);
        if (!conn)
            continue;
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0)
            ok = true;
        drmModeFreeConnector(conn);
    }
    drmModeFreeResources(res);
    close(fd);

    if (!ok)
        log_line("miniplc-hmi: no connected DRM connector (using fbdev)\n");
    return ok;
}
#endif

static int count_screens(void)
{
    json_object *root = json_object_from_file(MANIFEST_PATH);
    if (!root)
        return -1;

    json_object *sc = NULL;
    int n = -1;
    if (json_object_object_get_ex(root, "screens", &sc) &&
        json_object_is_type(sc, json_type_array))
        n = json_object_array_length(sc);
    json_object_put(root);
    return n;
}

/** Log framebuffer geometry; warn if LVGL fbdev backend cannot handle bpp. */
static bool probe_framebuffer(const char *path)
{
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        log_line("miniplc-hmi: open %s: %s\n", path, strerror(errno));
        return false;
    }

    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0 ||
        ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        log_line("miniplc-hmi: ioctl on %s failed: %s\n", path,
                strerror(errno));
        close(fd);
        return false;
    }

    log_line("miniplc-hmi: %s %ux%u %ubpp line_length=%u smem_len=%u\n",
             path, vinfo.xres, vinfo.yres, vinfo.bits_per_pixel,
             finfo.line_length, finfo.smem_len);

    if (vinfo.bits_per_pixel != 16 && vinfo.bits_per_pixel != 24 &&
        vinfo.bits_per_pixel != 32) {
        log_line(
            "miniplc-hmi: unsupported bpp %u (need 16/24/32); fbdev unusable.\n",
            vinfo.bits_per_pixel);
        close(fd);
        return false;
    }

    close(fd);
    return true;
}

static void build_ui(lv_obj_t *scr)
{
    const int n = count_screens();
    const char *msg;

    if (n < 0 || n == 0) {
        msg = FALLBACK_TEXT;
    } else {
        static char buf[128];
        snprintf(buf, sizeof(buf), "MiniPLC HMI (%d screen%s)", n,
                 n == 1 ? "" : "s");
        msg = buf;
    }

    lv_obj_set_style_bg_color(scr, lv_color_hex(0xF0F0F0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, msg);

    lv_obj_set_style_text_color(lbl, lv_color_hex(0x101010), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}

static lv_display_t *create_display_drm(void)
{
#if LV_USE_LINUX_DRM
    lv_display_t *disp = lv_linux_drm_create();
    if (!disp)
        return NULL;
    lv_linux_drm_set_file(disp, DRM_DEVICE, -1);
    return disp;
#else
    return NULL;
#endif
}

static lv_display_t *create_display_fbdev(void)
{
#if LV_USE_LINUX_FBDEV
    lv_display_t *disp = lv_linux_fbdev_create();
    if (!disp)
        return NULL;
    lv_linux_fbdev_set_file(disp, FB_DEVICE);
    return disp;
#else
    return NULL;
#endif
}

int main(void)
{
    log_fp = fopen(LOG_PATH, "ae");
    if (log_fp)
        setbuf(log_fp, NULL);

    log_line("miniplc-hmi: starting\n");

    unblank_all_fbs();
    lv_init();

    lv_display_t *disp = NULL;

#if LV_USE_LINUX_DRM
    if (drm_primary_available())
        disp = create_display_drm();
#endif

    if (!disp) {
        if (!probe_framebuffer(FB_DEVICE))
            goto fail;
        disp = create_display_fbdev();
        if (!disp)
            goto fail;
        log_line("miniplc-hmi: using framebuffer %s\n", FB_DEVICE);
    } else {
        log_line("miniplc-hmi: using DRM %s\n", DRM_DEVICE);
    }

    lv_display_set_default(disp);

    build_ui(lv_screen_active());
    lv_refr_now(disp);

    while (1) {
        lv_tick_inc(5);
        lv_timer_handler();
        usleep(5000);
    }

fail:
    log_line("miniplc-hmi: fatal init failure\n");
    if (log_fp)
        fclose(log_fp);
    return 1;
}
