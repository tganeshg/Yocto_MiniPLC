/*
 * MiniPLC HMI — display init.
 *
 * On the RPi B+ with the VC4 KMS DSI overlay the kernel exposes BOTH
 * /dev/dri/card0 (real KMS) and an emulated /dev/fb0 (vc4drmfb).  We tried
 * the fbdev driver first; even with LV_LINUX_FBDEV_RENDER_MODE_DIRECT it
 * leaves the panel mostly black (only a tiny region of the framebuffer
 * actually receives pixels).  That matches the warning baked into our
 * lv_conf.h:
 *
 *     #define LV_USE_LINUX_DRM 1
 *     // Driver for /dev/dri/card — preferred on Raspberry Pi KMS
 *     // (fixes black panel with fbdev-only).
 *
 * So: try DRM first, fall back to fbdev only if /dev/dri/card0 is missing
 * (e.g. early bring-up or fb-only kernels).  Either way, attach evdev for
 * the capacitive touchscreen.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lvgl/lv_conf.h>
#include <lvgl/lvgl.h>
#include "menu.h"

#if LV_USE_EVDEV
static void attach_touch(lv_display_t *disp)
{
    const char *input_device = LV_LINUX_EVDEV_POINTER_DEVICE;
    if (!input_device || !*input_device) {
        fprintf(stderr, "[HMI] LV_LINUX_EVDEV_POINTER_DEVICE not set — no touch input\n");
        return;
    }
    lv_indev_t *touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, input_device);
    if (touch) lv_indev_set_display(touch, disp);
    fprintf(stderr, "[HMI] touch input attached: %s\n", input_device);
}
#endif

static lv_display_t *disp_init(void)
{
    lv_display_t *disp = NULL;

#if LV_USE_LINUX_DRM
    /* Preferred path on RPi KMS — direct rendering via /dev/dri/card0. */
    if (access("/dev/dri/card0", F_OK) == 0) {
        disp = lv_linux_drm_create();
        if (disp) {
            lv_linux_drm_set_file(disp, "/dev/dri/card0", -1);
            fprintf(stderr, "[HMI] display: DRM /dev/dri/card0\n");
            return disp;
        }
        fprintf(stderr, "[HMI] lv_linux_drm_create() failed, falling back to fbdev\n");
    } else {
        fprintf(stderr, "[HMI] /dev/dri/card0 missing, falling back to fbdev\n");
    }
#endif

#if LV_USE_LINUX_FBDEV
    {
        const char *fbdev = LV_LINUX_FBDEV_DEVICE;
        disp = lv_linux_fbdev_create();
        if (!disp) {
            fprintf(stderr, "[HMI] lv_linux_fbdev_create() failed\n");
            return NULL;
        }
        lv_linux_fbdev_set_file(disp, fbdev);
        fprintf(stderr, "[HMI] display: fbdev %s\n", fbdev);
        return disp;
    }
#else
    #error "No display backend enabled in lv_conf.h (need LV_USE_LINUX_DRM or LV_USE_LINUX_FBDEV)"
#endif
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    lv_init();

    lv_display_t *disp = disp_init();
    if (!disp) {
        fprintf(stderr, "[HMI] no display backend available — aborting\n");
        return 1;
    }

#if LV_USE_EVDEV
    attach_touch(disp);
#endif

    menu_create();
    menu_loop();
    return 0;
}

/* EOF */
